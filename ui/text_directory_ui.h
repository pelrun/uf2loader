/*
 * text_directory_ui.h
 *
 */

#ifndef TEXT_DIRECTORY_UI_H
#define TEXT_DIRECTORY_UI_H

#include <stdbool.h>

#define ITEMS_PER_PAGE 16
#define FONT_HEIGHT 12
#define ENTRY_PADDING 2
#define BAT_UPDATE_MS 60000
#define SCROLL_UPDATE_MS 500
// Callback type: invoked when the user makes a final selection. The selected path is passed as an argument.
typedef void (*final_selection_callback_t)(const char *selected_path);

// Initialize the text directory UI. This sets up the SD card filesystem and the display UI.
// Returns true if initialization succeeded, false otherwise.
void text_directory_ui_init(void);

// Run the main event loop for the directory navigation UI. This function polls for key events,
// updates the selection cursor, and processes directory changes.
void text_directory_ui_run(void);

// Register a callback that will be called when the final selection is made.
void text_directory_ui_set_final_callback(final_selection_callback_t callback);

// Public API: Set a status or error message to be displayed in the status bar (auto-clears after 3 seconds)
void text_directory_ui_set_status(const char *msg);

void text_directory_ui_update_path();

void text_directory_ui_update_title();

#endif // TEXT_DIRECTORY_UI_H
