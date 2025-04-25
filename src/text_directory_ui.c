/**
 * PicoCalc SD Firmware Loader
 *
 * Author: Hsuan Han Lai
 * Email: hsuan.han.lai@gmail.com
 * Website: https://hsuanhanlai.com
 * Year: 2025
 *
 * text_directory_ui.c
 *
 * Implementation for the Text Directory UI Navigator.
 *
 * This module provides a text-based UI for navigating directories and files on an SD card.
 * It uses lcdspi APIs for rendering, key_event APIs for input handling, and pico-vfs/standard POSIX APIs
 * for filesystem operations.
 *
 * Features:
 *  - UI Initialization: Sets up the display, input handling, and mounts the SD card filesystem.
 *  - Directory Navigation: Allows navigation through directories and files using arrow keys.
 *  - File Selection: Invokes a callback when a file is selected.
 *  - Status Messages: Displays temporary status messages at the bottom of the UI.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hardware/watchdog.h>
#include "pico/stdlib.h"
#include "lcdspi/lcdspi.h"
#include "key_event.h"
#include "text_directory_ui.h"
#include "debug.h"
#include <sys/stat.h>
#include <dirent.h>

// External functions for SD card handling
extern bool sd_card_inserted(void);
extern bool fs_init(void);

// UI Layout Constants
#define UI_WIDTH 280
#define UI_HEIGHT 280
#define UI_X 20 // Offset from top-left
#define UI_Y 20
#define HEADER_TITLE_HEIGHT 20 // Height for the title header
#define PATH_HEADER_HEIGHT 16  // Height for the current path display
#define STATUS_BAR_HEIGHT 16   // Height for the status bar

// UI Colors
#define COLOR_BG GRAY
#define COLOR_FG WHITE
#define COLOR_HIGHLIGHT GREEN

// Maximum number of directory entries
#define MAX_ENTRIES 128

// Data structure for directory entries
typedef struct
{
    char name[256];
    int is_dir; // 1 if directory, 0 if file
    off_t file_size; // Size of the file in bytes
} dir_entry_t;

// UI Layout Constants for file display
#define FILE_NAME_X (UI_X + 4)
#define FILE_NAME_AREA_WIDTH 200
#define FILE_SIZE_X (UI_X + UI_WIDTH - 70)
#define FILE_SIZE_AREA_WIDTH 60
#define CHAR_WIDTH 8
#define FILE_NAME_VISIBLE_CHARS (FILE_NAME_AREA_WIDTH / CHAR_WIDTH)
#define SCROLL_DELAY_MS 300

// Global variables for UI state
static char current_path[512] = "/sd";                   // Current directory path
static dir_entry_t entries[MAX_ENTRIES];                 // Directory entries
static int entry_count = 0;                              // Number of entries in the current directory
static int selected_index = 0;                           // Currently selected entry index
static char status_message[256] = "";                    // Status message
static uint32_t status_timestamp = 0;                    // Timestamp for status message
static final_selection_callback_t final_callback = NULL; // Callback for file selection

// Forward declarations
static void ui_refresh(void);
static void load_directory(const char *path);
static void process_key_event(int key);
static void ui_draw_title(void);
static void ui_draw_path_header(void);
static void ui_draw_directory_list(void);
static void ui_draw_directory_entry(int entry_idx, int posY, int font_height, int is_selected);
static void ui_update_selected_entry(void);
static void ui_draw_status_bar(void);
static void format_file_size(off_t size, int is_dir, char *buf, size_t buf_size);
static void get_scrolling_text(const char *text, char *out, size_t out_size, int visible_chars);

// Helper: Draw a filled rectangle
static void draw_filled_rect(int x, int y, int width, int height, int color)
{
    draw_rect_spi(x, y, x + width - 1, y + height - 1, color);
}

// Helper: Draw text at a specific position
static void draw_text(int x, int y, const char *text, int foreground, int background)
{
    lcd_set_cursor(x, y);
    lcd_print_string_color((char *)text, foreground, background);
}

/**
 * Format file size into human-readable string
 * Converts raw byte count to KB or MB with appropriate suffix
 */
static void format_file_size(off_t size, int is_dir, char *buf, size_t buf_size)
{
    if (is_dir)
    {
        snprintf(buf, buf_size, "DIR");
    }
    else if (size >= 1024 * 1024)
    {
        double mb = size / (1024.0 * 1024.0);
        snprintf(buf, buf_size, "%.1fMB", mb);
    }
    else
    {
        int kb = size / 1024;
        if (kb < 1)
            kb = 1;
        snprintf(buf, buf_size, "%dKB", kb);
    }
}

/**
 * Create scrolling text for long filenames
 * Creates a continuous scroll effect for text that exceeds visible area
 */
static void get_scrolling_text(const char *text, char *out, size_t out_size, int visible_chars)
{
    char scroll_buffer[512];
    snprintf(scroll_buffer, sizeof(scroll_buffer), "%s   %s", text, text);
    int scroll_len = strlen(scroll_buffer);
    uint32_t time_ms = time_us_64() / 1000;
    int offset = (time_ms / SCROLL_DELAY_MS) % scroll_len;
    
    int i;
    for (i = 0; i < visible_chars && i < out_size - 1; i++)
    {
        int idx = (offset + i) % scroll_len;
        out[i] = scroll_buffer[idx];
    }
    out[i] = '\0';
}

// Load directory entries into the global entries array
static void load_directory(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        entry_count = 0;
        return;
    }
    entry_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && entry_count < MAX_ENTRIES)
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;
        strncpy(entries[entry_count].name, ent->d_name, sizeof(entries[entry_count].name) - 1);
        entries[entry_count].name[sizeof(entries[entry_count].name) - 1] = '\0';

        // Build full path for stat
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);
        
        // Determine if the entry is a directory and get file size
        if (ent->d_type != DT_UNKNOWN)
        {
            entries[entry_count].is_dir = (ent->d_type == DT_DIR) ? 1 : 0;
            
            // Get file size using stat even if we know the type from d_type
            struct stat statbuf;
            if (stat(full_path, &statbuf) == 0)
            {
                entries[entry_count].file_size = entries[entry_count].is_dir ? 0 : statbuf.st_size;
            }
            else
            {
                entries[entry_count].file_size = 0;
            }
        }
        else
        {
            struct stat statbuf;
            if (stat(full_path, &statbuf) == 0)
            {
                entries[entry_count].is_dir = S_ISDIR(statbuf.st_mode) ? 1 : 0;
                entries[entry_count].file_size = entries[entry_count].is_dir ? 0 : statbuf.st_size;
            }
            else
            {
                entries[entry_count].is_dir = 0;
                entries[entry_count].file_size = 0;
            }
        }
        entry_count++;
    }
    closedir(dir);
    selected_index = 0;
}

// Draw the title header
static void ui_draw_title(void)
{
    draw_rect_spi(UI_X, UI_Y, UI_X + UI_WIDTH - 1, UI_Y + HEADER_TITLE_HEIGHT, BLACK);
    draw_text(UI_X + 2, UI_Y + 2, "PicoCalc SD Firmware Loader", WHITE, BLACK);
}

// Draw the current path header
static void ui_draw_path_header(void)
{
    char path_header[300];
    snprintf(path_header, sizeof(path_header), "Path: %s", current_path);
    int y = UI_Y + HEADER_TITLE_HEIGHT;
    draw_rect_spi(UI_X, y, UI_X + UI_WIDTH - 1, y + PATH_HEADER_HEIGHT - 1, COLOR_BG);
    draw_text(UI_X + 2, y + 2, path_header, COLOR_FG, COLOR_BG);
    draw_line_spi(UI_X, y + PATH_HEADER_HEIGHT - 2, UI_X + UI_WIDTH - 1, y + PATH_HEADER_HEIGHT - 2, COLOR_FG);
}

/**
 * Draw a single directory entry
 * 
 * @param entry_idx Index of the entry in the entries array
 * @param posY Vertical position to draw the entry
 * @param font_height Height of the font
 * @param is_selected Whether this entry is currently selected
 */
static void ui_draw_directory_entry(int entry_idx, int posY, int font_height, int is_selected)
{
    // Highlight background for selected item
    if (is_selected)
    {
        draw_rect_spi(UI_X, posY - 1, UI_X + UI_WIDTH - 1, posY + font_height, COLOR_HIGHLIGHT);
    }
    
    // Prepare filename with directory indicator
    char full_file_name[300];
    snprintf(full_file_name, sizeof(full_file_name), "%s%s", 
            entries[entry_idx].name, 
            entries[entry_idx].is_dir ? "/" : "");
    
    // Prepare display text with scrolling for selected items
    char display_buffer[300];
    if (is_selected && strlen(full_file_name) > FILE_NAME_VISIBLE_CHARS)
    {
        // Use scrolling text for selected long filenames
        get_scrolling_text(full_file_name, display_buffer, sizeof(display_buffer), FILE_NAME_VISIBLE_CHARS);
    }
    else
    {
        // For non-selected or short filenames
        if (strlen(full_file_name) > FILE_NAME_VISIBLE_CHARS)
        {
            // Truncate with ellipsis
            strncpy(display_buffer, full_file_name, FILE_NAME_VISIBLE_CHARS - 3);
            display_buffer[FILE_NAME_VISIBLE_CHARS - 3] = '\0';
            strcat(display_buffer, "...");
        }
        else
        {
            strncpy(display_buffer, full_file_name, sizeof(display_buffer) - 1);
            display_buffer[sizeof(display_buffer) - 1] = '\0';
        }
    }
    
    // Format and display file size
    char size_buffer[20];
    format_file_size(entries[entry_idx].file_size, entries[entry_idx].is_dir, 
                    size_buffer, sizeof(size_buffer));
    
    // Draw filename and file size
    draw_text(FILE_NAME_X, posY, display_buffer, COLOR_FG, is_selected ? COLOR_HIGHLIGHT : COLOR_BG);
    draw_text(FILE_SIZE_X, posY, size_buffer, COLOR_FG, is_selected ? COLOR_HIGHLIGHT : COLOR_BG);
}

/**
 * Update only the selected entry row
 * This is an optimization to avoid redrawing the entire directory list
 * when only the selected entry needs to be updated (e.g., for scrolling text)
 */
static void ui_update_selected_entry(void)
{
    const int font_height = 12;
    const int entry_padding = 2;
    int y_start = UI_Y + HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT;
    int available_height = UI_HEIGHT - (HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT + STATUS_BAR_HEIGHT);
    int max_visible = available_height / (font_height + entry_padding);
    int start_index = (selected_index >= max_visible) ? selected_index - max_visible + 1 : 0;
    
    // Calculate the position of the selected entry
    int visible_index = selected_index - start_index;
    if (visible_index >= 0 && visible_index < max_visible) {
        int posY = y_start + visible_index * (font_height + entry_padding);
        
        // Clear just the selected row
        draw_rect_spi(UI_X, posY - 1, UI_X + UI_WIDTH - 1, posY + font_height, COLOR_BG);
        
        // Redraw just the selected entry
        ui_draw_directory_entry(selected_index, posY, font_height, 1);
    }
}

// Draw the directory list
static void ui_draw_directory_list(void)
{
    const int font_height = 12;
    const int entry_padding = 2;
    int y_start = UI_Y + HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT;
    int available_height = UI_HEIGHT - (HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT + STATUS_BAR_HEIGHT);
    int max_visible = available_height / (font_height + entry_padding);
    int start_index = (selected_index >= max_visible) ? selected_index - max_visible + 1 : 0;

    draw_rect_spi(UI_X, y_start, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - STATUS_BAR_HEIGHT - 1, COLOR_BG);

    for (int i = 0; i < max_visible && (i + start_index) < entry_count; i++)
    {
        int posY = y_start + i * (font_height + entry_padding);
        int entry_idx = i + start_index;
        int is_selected = (entry_idx == selected_index);
        
        // Draw the entry using the helper function
        ui_draw_directory_entry(entry_idx, posY, font_height, is_selected);
    }
}

// Draw the status bar
static void ui_draw_status_bar(void)
{
    int y = UI_Y + UI_HEIGHT - STATUS_BAR_HEIGHT;
    draw_rect_spi(UI_X, y, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - 1, COLOR_BG);
    draw_line_spi(UI_X, y, UI_X + UI_WIDTH - 1, y, COLOR_FG);
    char truncated_message[UI_WIDTH / 8];
    strncpy(truncated_message, status_message, sizeof(truncated_message) - 1);
    truncated_message[sizeof(truncated_message) - 1] = '\0';
    draw_text(UI_X + 2, y + 2, truncated_message, COLOR_FG, COLOR_BG);
}

// Refresh the entire UI
static void ui_refresh(void)
{
    ui_draw_title();
    ui_draw_path_header();
    ui_draw_directory_list();
    ui_draw_status_bar();

    if (status_message[0] != '\0' && ((time_us_64() / 1000) - status_timestamp) > 3000)
    {
        status_message[0] = '\0';
        ui_draw_status_bar();
    }
}

// Handle key events for navigation and selection
static void process_key_event(int key)
{
    switch (key)
    {
    case KEY_ARROW_UP:
        if (selected_index > 0)
            selected_index--;
        ui_draw_directory_list();
        break;
    case KEY_ARROW_DOWN:
        if (selected_index < entry_count - 1)
            selected_index++;
        ui_draw_directory_list();
        break;
    case KEY_ENTER:
        if (entry_count > 0)
        {
            char new_path[512];
            if (entries[selected_index].is_dir)
            {
                snprintf(new_path, sizeof(new_path), "%s/%s", current_path, entries[selected_index].name);
                strncpy(current_path, new_path, sizeof(current_path) - 1);
                load_directory(current_path);
                ui_draw_path_header();
                ui_draw_directory_list();
            }
            else if (final_callback)
            {
                char final_selected[512];
                snprintf(final_selected, sizeof(final_selected), "%s/%s", current_path, entries[selected_index].name);
                final_callback(final_selected);
            }
        }
        break;
    case KEY_BACKSPACE:
        if (strcmp(current_path, "/sd") != 0)
        {
            char *last_slash = strrchr(current_path, '/');
            if (last_slash)
                *last_slash = '\0';
            if (current_path[0] == '\0')
                strncpy(current_path, "/sd", sizeof(current_path) - 1);
            load_directory(current_path);
            ui_draw_path_header();
            ui_draw_directory_list();
        }
        break;
    default:
        break;
    }
    ui_draw_status_bar();
}

// Public API: Set the final selection callback
void text_directory_ui_set_final_callback(final_selection_callback_t callback)
{
    final_callback = callback;
}

// Public API: Initialize the UI
bool text_directory_ui_init(void)
{
    draw_filled_rect(UI_X, UI_Y, UI_WIDTH, UI_HEIGHT, COLOR_BG);
    strncpy(current_path, "/sd", sizeof(current_path));
    load_directory(current_path);
    ui_refresh();
    return true;
}

// Public API: Set a status message
void text_directory_ui_set_status(const char *msg)
{
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
    status_timestamp = (time_us_64() / 1000);
    ui_draw_status_bar();
}

// Public API: Main event loop for the UI
void text_directory_ui_run(void)
{
    uint32_t last_scroll_update = 0;
    const uint32_t SCROLL_UPDATE_MS = 100; // Update scrolling text every 100ms
    
    while (true)
    {
        int key = keypad_get_key();
        if (key != 0)
            process_key_event(key);

        uint32_t current_time = time_us_64() / 1000;
        
        // Update scrolling text periodically
        if (current_time - last_scroll_update > SCROLL_UPDATE_MS)
        {
            // Only update the selected entry row if there are entries and a selected item might need scrolling
            if (entry_count > 0 && selected_index >= 0 && 
                strlen(entries[selected_index].name) + (entries[selected_index].is_dir ? 1 : 0) > FILE_NAME_VISIBLE_CHARS)
            {
                ui_update_selected_entry();
            }
            last_scroll_update = current_time;
        }

        // Clear status message after timeout
        if (status_message[0] != '\0' && (current_time - status_timestamp) > 3000)
        {
            status_message[0] = '\0';
            ui_draw_status_bar();
        }

        // Check for SD card removal during runtime
        if (!sd_card_inserted()) {
            text_directory_ui_set_status("SD card removed. Please reinsert card.");
            
            // Wait until the SD card is reinserted
            while (!sd_card_inserted()) {
                sleep_ms(100);
            }
            
            // Once reinserted, update the UI and reinitialize filesystem
            text_directory_ui_set_status("SD card detected. Remounting...");
            if (!fs_init()) {
                text_directory_ui_set_status("Failed to remount SD card!");
                sleep_ms(2000);
                watchdog_reboot(0, 0, 0);
            }
            
            // Refresh the directory listing
            load_directory(current_path);
            ui_draw_path_header();
            ui_draw_directory_list();
            text_directory_ui_set_status("SD card remounted successfully.");
        }

        sleep_ms(20); // Shorter sleep to make scrolling smoother
    }
}
