/* Mock middleware/flash_service.h */
#ifndef MIDDLEWARE_FLASH_SERVICE_H
#define MIDDLEWARE_FLASH_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

void flash_service_init(void);
void flash_service_core1_init(void);
bool flash_service_erase(uint32_t flash_offset, uint32_t size);
bool flash_service_program(uint32_t flash_offset, const uint8_t *data, uint32_t size);
bool flash_service_write_sector(uint32_t flash_offset, const uint8_t *data);

#endif
