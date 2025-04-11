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
} dir_entry_t;

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
static void ui_draw_status_bar(void);

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

        // Determine if the entry is a directory
        if (ent->d_type != DT_UNKNOWN)
        {
            entries[entry_count].is_dir = (ent->d_type == DT_DIR) ? 1 : 0;
        }
        else
        {
            struct stat statbuf;
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, ent->d_name);
            if (stat(full_path, &statbuf) == 0)
            {
                entries[entry_count].is_dir = S_ISDIR(statbuf.st_mode) ? 1 : 0;
            }
            else
            {
                entries[entry_count].is_dir = 0;
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
        int posX = UI_X + 4;
        int posY = y_start + i * (font_height + entry_padding);
        if (i + start_index == selected_index)
        {
            draw_rect_spi(posX - 4, posY - 1, posX + UI_WIDTH - 8, posY + font_height, COLOR_HIGHLIGHT);
        }
        char text_buffer[300];
        snprintf(text_buffer, sizeof(text_buffer), "%s%s", entries[i + start_index].name, entries[i + start_index].is_dir ? "/" : "");
        draw_text(posX, posY, text_buffer, COLOR_FG, COLOR_BG);
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
    while (true)
    {
        int key = keypad_get_key();
        if (key != 0)
            process_key_event(key);

        if (status_message[0] != '\0' && ((time_us_64() / 1000) - status_timestamp) > 3000)
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

        sleep_ms(100);
    }
}
