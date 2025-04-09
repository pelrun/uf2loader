/*
 * text_directory_ui.h
 * 
 */

#ifndef TEXT_DIRECTORY_UI_H
#define TEXT_DIRECTORY_UI_H

#include <stdbool.h>

// Callback type: invoked when the user makes a final selection. The selected path is passed as an argument.
typedef void (*final_selection_callback_t)(const char *selected_path);

// Initialize the text directory UI. This sets up the SD card filesystem and the display UI.
// Returns true if initialization succeeded, false otherwise.
bool text_directory_ui_init(void);

// Run the main event loop for the directory navigation UI. This function polls for key events,
// updates the selection cursor, and processes directory changes.
void text_directory_ui_run(void);

// Register a callback that will be called when the final selection is made.
void text_directory_ui_set_final_callback(final_selection_callback_t callback);

// Public API: Set a status or error message to be displayed in the status bar (auto-clears after 3 seconds)
void text_directory_ui_set_status(const char *msg);

#endif // TEXT_DIRECTORY_UI_H
