#ifndef __UI_H_
#define __UI_H_

// External functions for SD card handling
extern bool sd_insert_state;
bool sd_card_inserted(void);

bool fs_init(void);
void fs_deinit(void);

void reboot(void);

#endif // __UI_H_
