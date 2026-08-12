#ifndef PROTOCOL_CONFIG_HID_H
#define PROTOCOL_CONFIG_HID_H

#include <stdint.h>
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize config HID module (if needed) */
void config_hid_init(void);

/* Periodic task to be called from main loop (was hid_config_task) */
void hid_config_task(void);

/* TinyUSB callbacks (implemented in module) */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_CONFIG_HID_H */
