/**
 * PicoCalc UF2 Loader
 *
 * Originally by : Hsuan Han Lai
 * Email: hsuan.han.lai@gmail.com
 * Website: https://hsuanhanlai.com
 * Year: 2025
 *
 * text_directory_ui.c
 *
 * Implementation for the Text Directory UI Navigator.
 *
 * This module provides a text-based UI for navigating directories and files on an SD card.
 * It uses lcdspi APIs for rendering, key_event APIs for input handling, and pico-vfs/standard POSIX
 * APIs for filesystem operations.
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
#include "binary_info_reader.h"
#include "lcdspi.h"
#include "key_event.h"
#include "text_directory_ui.h"
#include "debug.h"

#include "ff.h"

#include "proginfo.h"

#include "ui.h"

#if ENABLE_USB
#include "usb_msc.h"
#include "tusb.h"
#endif

// UI Layout Constants
#define UI_WIDTH 280
#define UI_HEIGHT 280
#define UI_X 20  // Offset from top-left
#define UI_Y 20
#define HEADER_TITLE_HEIGHT 20  // Height for the title header
#define PATH_HEADER_HEIGHT 16   // Height for the current path display
#define STATUS_BAR_HEIGHT 16    // Height for the status bar

// UI Colors
#define COLOR_BG BLACK
#define COLOR_FG WHITE
#define COLOR_HIGHLIGHT LITEGRAY

// Maximum number of directory entries
#define MAX_ENTRIES 128

enum entry_type_e
{
  ENTRY_IS_FILE,
  ENTRY_IS_DIR,
  ENTRY_IS_LAST_APP,
};

// Data structure for directory entries
typedef struct
{
  char name[256];
  int type;
  off_t file_size;  // Size of the file in bytes
  uint16_t x;
  uint16_t y;
} dir_entry_t;

// UI Layout Constants for file display
#define FILE_NAME_X (UI_X + 4)
#define FILE_NAME_AREA_WIDTH 200
#define FILE_SIZE_X (UI_X + UI_WIDTH - 70)
#define FILE_SIZE_AREA_WIDTH 60
#define CHAR_WIDTH 8
#define FILE_NAME_VISIBLE_CHARS (FILE_NAME_AREA_WIDTH / CHAR_WIDTH)
#define SCROLL_DELAY_MS 300

#if PICO_RP2040
#define FW_PATH "/pico1-apps"
#elif PICO_RP2350
#define FW_PATH "/pico2-apps"
#endif

// Global variables for UI state
static char current_path[512] = FW_PATH;  // Current directory path
static dir_entry_t entries[MAX_ENTRIES];  // Directory entries
static int entry_count = 0;               // Number of entries in the current directory
static int last_selected_index = 0, selected_index = 0;
static uint8_t page_index = 0;
static uint8_t last_page_index = 0;
static uint8_t update_required = 0;
static char status_message[256] = "";                     // Status message
static final_selection_callback_t final_callback = NULL;  // Callback for file selection
static uint32_t last_scrolling = 0;                       // for text scrolling in selected entry
// Forward declarations
static void load_directory(const char *path);
static void ui_draw_title(void);
static void ui_draw_directory_list(void);
static void ui_draw_directory_entry(int entry_idx);
static void ui_draw_status_bar(void);
static void ui_draw_empty_tip(void);
static void format_file_size(off_t size, int type, char *buf, size_t buf_size);
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
static void format_file_size(off_t size, int type, char *buf, size_t buf_size)
{
  if (type == ENTRY_IS_LAST_APP)
  {
    snprintf(buf, buf_size, "");
  }
  else if (type == ENTRY_IS_DIR)
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
    if (kb < 1) kb = 1;
    snprintf(buf, buf_size, "%dKB", kb);
  }
}

static void set_default_entry()
{
  entry_count = 0;

  const char *filename = pr_binary_info_program_name();

  if (!filename)
  {
    filename = bl_proginfo_filename();
  }

  if (!filename)
  {
    filename = "Current App";
  }

  snprintf(entries[entry_count].name, sizeof(entries[entry_count].name), "[%s]", filename);

  entries[entry_count].type = ENTRY_IS_LAST_APP;
  entries[entry_count].file_size = 0;
}

/**
 * Create scrolling text for long filenames
 * Creates a continuous scroll effect for text that exceeds visible area
 */
static void get_scrolling_text(const char *text, char *out, size_t out_size, int visible_chars)
{
  char scroll_buffer[512];
  snprintf(scroll_buffer, sizeof(scroll_buffer), "%s   %s", text, text);

  int scroll_len = strlen(text) + 3;
  uint32_t time_ms = (time_us_64() / 1000) - last_scrolling;
  int offset = (time_ms / SCROLL_DELAY_MS) % scroll_len;

  int i;
  for (i = 0; i < visible_chars && i < out_size - 1; i++)
  {
    int idx = (offset + i) % scroll_len;
    out[i] = scroll_buffer[idx];
  }
  out[i] = '\0';
}

bool has_suffix(const char *filename, const char *suffix)
{
  size_t len_filename = strlen(filename);
  size_t len_suffix = strlen(suffix);
  if (len_filename < len_suffix) return false;
  return strcasecmp(filename + len_filename - len_suffix, suffix) == 0;
}

// Load directory entries into the global entries array
static void load_directory(const char *path)
{
  DIR dir;
  FILINFO fno;

  FRESULT res = f_opendir(&dir, path);
  if (res != FR_OK)
  {
    entry_count = 0;
    return;
  }
  set_default_entry();

  entry_count = 1;
  while ((f_readdir(&dir, &fno)) == FR_OK && entry_count < MAX_ENTRIES)
  {
    if (fno.fname[0] == 0) break;                                // end of dir
    if (fno.fname[0] == '.') continue;                           // hide dotfiles
    if (fno.fattrib & AM_HID || fno.fattrib & AM_SYS) continue;  // hide hidden files
    if (has_suffix(fno.fname, ".uf2") == false) continue;        // hide non uf2 files

    strlcpy(entries[entry_count].name, fno.fname, sizeof(entries[entry_count].name));

    // Build full path for stat
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, fno.fname);

    // Determine if the entry is a directory and get file size
    entries[entry_count].type = (fno.fattrib & AM_DIR) ? ENTRY_IS_DIR : ENTRY_IS_FILE;
    entries[entry_count].file_size = (fno.fattrib & AM_DIR) ? 0 : fno.fsize;
    entry_count++;
  }
  f_closedir(&dir);
  selected_index = 0;
}

// Draw the title header
static void ui_draw_title(void)
{
  draw_rect_spi(UI_X, UI_Y, UI_X + UI_WIDTH - 1, UI_Y + HEADER_TITLE_HEIGHT, BLACK);
  draw_text(UI_X + 2, UI_Y + 2, "PicoCalc UF2 Loader " PICO_PROGRAM_VERSION_STRING, WHITE, BLACK);
}

static void ui_draw_empty_tip()
{
  int y = UI_Y + UI_HEIGHT / 2;  // center
  int y_start = UI_Y + HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT;
  draw_rect_spi(UI_X, y_start, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - STATUS_BAR_HEIGHT - 1,
                COLOR_BG);

  // draw_rect_spi(UI_X, UI_Y + HEADER_TITLE_HEIGHT+1, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - 2,
  // COLOR_BG);

  draw_text(UI_X + 2, y + 2, "No .uf2 files in folder", COLOR_FG, COLOR_BG);
  draw_text(UI_X + 2, y + 12 + 2, "Please copy .uf2 files to the", COLOR_FG, COLOR_BG);
  draw_text(UI_X + 2, y + 24 + 2, FW_PATH " folder", COLOR_FG, COLOR_BG);

  set_default_entry();
  // Draw the entry using the helper function
  ui_draw_directory_entry(0);
}

// Draw the current path header
void text_directory_ui_update_path(void)
{
  char path_header[300];
  if (!sd_insert_state)
  {
    snprintf(path_header, sizeof(path_header), "SD card not found");
  }
  else
  {
    snprintf(path_header, sizeof(path_header), "Path: SD%s", current_path);
  }
  int y = UI_Y + HEADER_TITLE_HEIGHT;
  draw_rect_spi(UI_X, y, UI_X + UI_WIDTH - 1, y + PATH_HEADER_HEIGHT - 1, COLOR_BG);
  draw_text(UI_X + 2, y + 2, path_header, COLOR_FG, COLOR_BG);
  draw_line_spi(UI_X, y + PATH_HEADER_HEIGHT - 2, UI_X + UI_WIDTH - 1, y + PATH_HEADER_HEIGHT - 2,
                COLOR_FG);
}

/**
 * Draw a single directory entry
 *
 * @param entry_idx Index of the entry in the entries array
 * @param posY Vertical position to draw the entry
 * @param font_height Height of the font
 * @param is_selected Whether this entry is currently selected
 */
static void ui_draw_directory_entry(int entry_idx)
{
  int y_start = UI_Y + HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT;
  int posY = y_start + (entry_idx % ITEMS_PER_PAGE) * (FONT_HEIGHT + ENTRY_PADDING);
  int is_selected = (entry_idx == selected_index);
  entries[entry_idx].x = FILE_NAME_X;
  entries[entry_idx].y = posY;
  // Highlight background for selected item
  if (is_selected)
  {
    draw_rect_spi(UI_X, posY - 1, UI_X + UI_WIDTH - 1, posY + FONT_HEIGHT, COLOR_HIGHLIGHT);
  }

  // Prepare filename with directory indicator
  char full_file_name[300];
  snprintf(full_file_name, sizeof(full_file_name), "%s%s", entries[entry_idx].name,
           (entries[entry_idx].type == ENTRY_IS_DIR) ? "/" : "");

  // Prepare display text with scrolling for selected items
  char display_buffer[300];
  if (is_selected && strlen(full_file_name) > FILE_NAME_VISIBLE_CHARS)
  {
    // Use scrolling text for selected long filenames
    get_scrolling_text(full_file_name, display_buffer, sizeof(display_buffer),
                       FILE_NAME_VISIBLE_CHARS);
  }
  else
  {
    // For non-selected or short filenames
    if (strlen(full_file_name) > FILE_NAME_VISIBLE_CHARS)
    {
      // Truncate with ellipsis
      strlcpy(display_buffer, full_file_name, FILE_NAME_VISIBLE_CHARS - 3);
      strcat(display_buffer, "...");
    }
    else
    {
      strlcpy(display_buffer, full_file_name, sizeof(display_buffer));
    }
  }

  // Format and display file size
  char size_buffer[20];
  format_file_size(entries[entry_idx].file_size, entries[entry_idx].type, size_buffer,
                   sizeof(size_buffer));

  // Draw filename and file size
  draw_text(FILE_NAME_X, posY, display_buffer, is_selected ? COLOR_BG : COLOR_FG,
            is_selected ? COLOR_HIGHLIGHT : COLOR_BG);
  draw_text(FILE_SIZE_X, posY, size_buffer, is_selected ? COLOR_BG : COLOR_FG,
            is_selected ? COLOR_HIGHLIGHT : COLOR_BG);
}

/**
 * Update only the selected entry row
 * This is an optimization to avoid redrawing the entire directory list
 * when only the selected entry needs to be updated (e.g., for scrolling text)
 */
static void ui_update_selected_entry()
{
  if (last_selected_index != selected_index)
  {
    uint16_t y = entries[last_selected_index % ITEMS_PER_PAGE].y;
    draw_rect_spi(UI_X, y - 1, UI_X + UI_WIDTH - 1, y + FONT_HEIGHT, COLOR_BG);
    ui_draw_directory_entry(last_selected_index);
    last_selected_index = selected_index;
  }
  ui_draw_directory_entry(selected_index);
}

static void ui_scroll_selected_entry()
{
  static uint32_t last_scroll_update = 0;
  uint32_t current_time = time_us_64() / 1000;

  // Update scrolling text periodically
  if (current_time - last_scroll_update > SCROLL_UPDATE_MS)
  {
    last_scroll_update = current_time;
    // Only update the selected entry row if there are entries and a selected item might need
    // scrolling
    if (entry_count > 0 && selected_index >= 0 &&
        strlen(entries[selected_index].name) + (entries[selected_index].type ? 1 : 0) >
            FILE_NAME_VISIBLE_CHARS)
    {
      ui_update_selected_entry();
    }
  }
}

static void ui_clear_directory_list(void)
{
  if (entry_count < 1) return;
  int y_start = UI_Y + HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT;

  for (int i = 1; i < entry_count; i++)
  {
    strlcpy(entries[entry_count].name, "", sizeof(entries[entry_count].name));
    entries[entry_count].type = 0;
    entries[entry_count].file_size = 0;
  }

  draw_rect_spi(UI_X, y_start, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - STATUS_BAR_HEIGHT - 1,
                COLOR_BG);

  entry_count = 1;
  last_selected_index = 0;
  selected_index = 0;
  ui_update_selected_entry();
}

// Draw the directory list
static void ui_draw_directory_list(void)
{
  if (entry_count <= 0) return;
  page_index = (selected_index / ITEMS_PER_PAGE) * ITEMS_PER_PAGE;

  int y_start = UI_Y + HEADER_TITLE_HEIGHT + PATH_HEADER_HEIGHT;

  if (page_index != last_page_index)
  {
    draw_rect_spi(UI_X, y_start, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - STATUS_BAR_HEIGHT - 1,
                  COLOR_BG);
    last_page_index = page_index;
    update_required = 1;
  }
  last_scrolling = time_us_64() / 1000;
  if (update_required)
  {
    for (int i = page_index; i < page_index + ITEMS_PER_PAGE; i++)
    {
      if (i >= entry_count) break;
      // Draw the entry using the helper function
      ui_draw_directory_entry(i);
    }
  }
  ui_update_selected_entry();
  update_required = 0;
}

// Draw the status bar
static void ui_draw_status_bar(void)
{
  int y = UI_Y + UI_HEIGHT - STATUS_BAR_HEIGHT;
  draw_rect_spi(UI_X, y, UI_X + UI_WIDTH - 1, UI_Y + UI_HEIGHT - 1, COLOR_BG);
  draw_line_spi(UI_X, y, UI_X + UI_WIDTH - 1, y, COLOR_FG);
  char truncated_message[UI_WIDTH / 8];
  strlcpy(truncated_message, status_message, sizeof(truncated_message));
  draw_text(UI_X + 2, y + 2, truncated_message, COLOR_FG, COLOR_BG);
}

void text_directory_ui_update_title()
{
  // Battery icon
  char buf[8];
  int pcnt = keypad_get_battery();
  if (pcnt < 0) return;
  int level = pcnt * 13 / 100;
  int pad = 0;
  sprintf(buf, "%d%%", pcnt);
  int y = UI_Y;
  if (pcnt < 10)
  {
    pad = 8;
  }
  else if (pcnt >= 10 && pcnt < 100)
  {
    pad = 0;
  }
  else if (pcnt == 100)
  {
    pad = -8;
  }

  draw_rect_spi(UI_X + UI_WIDTH - 16 - 20 - 5 - 8, y, UI_X + UI_WIDTH, y + HEADER_TITLE_HEIGHT,
                COLOR_BG);
  draw_text(UI_X + UI_WIDTH - 16 - 20 - 5 + pad, y + 2, buf, COLOR_FG, COLOR_BG);
  draw_battery_icon(UI_X + UI_WIDTH - 16, y + 4, level);
}

static void ui_set_default_status()
{
  if (entry_count == 0)
  {
    text_directory_ui_set_status("Enter to load.");
    ui_draw_empty_tip();
  }
  else
  {
    if (sd_insert_state)
    {
      text_directory_ui_set_status("Up/Down to select, Enter to load.");
      // ui_draw_status_bar();
    }
  }
}

// Refresh the entire UI
static void ui_refresh(void)
{
  ui_draw_title();
  text_directory_ui_update_path();
  ui_draw_directory_list();
  text_directory_ui_update_title();
  ui_set_default_status();
}

void move_selection(int dir)
{
  last_selected_index = selected_index;

  selected_index = (selected_index + dir) % entry_count;
  if (selected_index < 0)
  {
    selected_index += entry_count;
  }

  ui_draw_directory_list();
}

void enter_dir(void)
{
  char new_path[512];
  snprintf(new_path, sizeof(new_path), "%s/%s", current_path, entries[selected_index].name);
  strlcpy(current_path, new_path, sizeof(current_path));
  load_directory(current_path);
  text_directory_ui_update_path();
  ui_draw_directory_list();
}

void leave_dir(void)
{
  if (strcmp(current_path, FW_PATH) != 0)
  {
    char *last_slash = strrchr(current_path, '/');
    if (last_slash) *last_slash = '\0';
    if (current_path[0] == '\0') strlcpy(current_path, FW_PATH, sizeof(current_path));
    load_directory(current_path);
    text_directory_ui_update_path();
    ui_draw_directory_list();
  }
}

// Handle key events for navigation and selection
void process_key_event()
{
  int key = keypad_get_key();
  if (key <= 0)
  {
    return;
  }

  switch (key)
  {
    case KEY_ARROW_UP:
      move_selection(-1);
      break;
    case KEY_ARROW_DOWN:
      move_selection(1);
      break;
    case KEY_ENTER:
      if (entry_count == 0)
      {
        // directly load app from flash
        final_callback(NULL);
      }
      if (entry_count > 0)
      {
        switch (entries[selected_index].type)
        {
          case ENTRY_IS_DIR:
            enter_dir();
            break;
          case ENTRY_IS_LAST_APP:
            if (final_callback)
            {
              final_callback(NULL);
            }
            break;
          default:
            if (final_callback)
            {
              char final_selected[512];
              snprintf(final_selected, sizeof(final_selected), "%s/%s", current_path,
                       entries[selected_index].name);
              final_callback(final_selected);
            }
        }
      }
      break;
    case KEY_BACKSPACE:
      leave_dir();
      break;
    default:
      break;
  }
}

// Public API: Set the final selection callback
void text_directory_ui_set_final_callback(final_selection_callback_t callback)
{
  final_callback = callback;
}

// Public API: Initialize the UI
void text_directory_ui_init(void)
{
  update_required = 1;
  draw_filled_rect(UI_X, UI_Y, UI_WIDTH, UI_HEIGHT, COLOR_BG);
  strlcpy(current_path, FW_PATH, sizeof(current_path));
  load_directory(current_path);
  ui_refresh();
  last_scrolling = time_us_64() / 1000;
}

// Public API: Set a status message
void text_directory_ui_set_status(const char *msg)
{
  if (strcmp(status_message, msg) == 0)
  {
    return;
  }
  strlcpy(status_message, msg, sizeof(status_message));
  ui_draw_status_bar();
}

void ui_bat_update(void)
{
  static int next_bat_update = 0;
  int uptime_ms = time_us_64() / 1000;

  if (uptime_ms - next_bat_update < 0)
  {
    return;
  }

  next_bat_update = uptime_ms + BAT_UPDATE_MS;
  text_directory_ui_update_title();
}

void ui_disconnect_sd()
{
  fs_deinit();

  if (!sd_insert_state)
  {
    text_directory_ui_set_status("SD card removed.");
  }
#if ENABLE_USB
  else if (usb_msc_is_mounted())
  {
    text_directory_ui_set_status("USB is connected");
  }
#endif

  text_directory_ui_update_path();
  ui_clear_directory_list();

  // Wait until the SD card is reinserted
  while (!sd_card_inserted())
  {
    ui_bat_update();
    process_key_event();

    sleep_ms(20);
  }

#if ENABLE_USB
  // Run mass storage until usb is disconnected
  while (usb_msc_is_mounted())
  {
    tud_task();
    __wfi();
  }
#endif

  // Once reinserted, update the UI and reinitialize filesystem
  text_directory_ui_set_status("Remounting...");
  bool mounted = false;
  for (int retry = 5; retry > 0 && !mounted; retry--)
  {
    sleep_ms(500);
    mounted = fs_init();
  }

  if (!mounted)
  {
    text_directory_ui_set_status("Failed to remount SD card!");
    sleep_ms(2000);
    reboot();
  }

  // Refresh the directory listing
  load_directory(current_path);
  update_required = 1;

  ui_refresh();
}

// Public API: Main event loop for the UI
void text_directory_ui_run(void)
{
  process_key_event();

  ui_scroll_selected_entry();

  ui_bat_update();

  // Check for SD card removal during runtime
  if (!sd_card_inserted()
#if ENABLE_USB
      || usb_msc_is_mounted()
#endif
  )
  {
    ui_disconnect_sd();
  }
}
