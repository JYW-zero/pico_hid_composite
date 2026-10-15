#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint8_t g_mock_flash[65536];
static uint64_t g_mock_time_ms = 0;
void flash_layout_init(void) {}
typedef enum { FLASH_REGION_CONFIG_A=0, FLASH_REGION_CONFIG_B, FLASH_REGION_FAULT_LOG, FLASH_REGION_KEY_STATS, FLASH_REGION_COUNT } flash_region_e;
uint32_t flash_layout_get_offset(flash_region_e r) { return r==0?57344:61440; }
uint32_t flash_layout_get_xip_addr(flash_region_e r) { return 0x10000000+flash_layout_get_offset(r); }
uint32_t flash_layout_get_size(flash_region_e r) { return 4096; }
bool flash_service_erase(uint32_t o, uint32_t s) { memset(g_mock_flash+o,0xFF,s); return true; }
bool flash_service_program(uint32_t o, const uint8_t* d, uint32_t s) { memcpy(g_mock_flash+o,d,s); return true; }
void fault_record(int level, const char* m, const char* msg) {}
#define config_get_flash_base config_get_flash_base_weak
#define config_enter_flash_write config_enter_flash_write_weak
#define config_exit_flash_write config_exit_flash_write_weak
#include "../../src/board/config.c"
#undef config_get_flash_base
#undef config_enter_flash_write
#undef config_exit_flash_write
uintptr_t config_get_flash_base(void) { return (uintptr_t)g_mock_flash; }
void config_enter_flash_write(void) {}
void config_exit_flash_write(void) {}
int main() { printf("config.c loaded\n"); config_init(); printf("config_init done\n"); return 0; }
