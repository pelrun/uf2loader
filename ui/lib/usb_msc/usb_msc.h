#ifndef USB_MSC_H
#define USB_MSC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the USB Mass Storage Class interface.
void usb_msc_init(void);

// Stops the USB Mass Storage Class interface.
// Deinitializes TinyUSB device stack
void usb_msc_stop(void);

// Returns true if MSC interface is currently mounted/connected to a host.
bool usb_msc_is_mounted(void);

#ifdef __cplusplus
}
#endif

#endif // USB_MSC_H
