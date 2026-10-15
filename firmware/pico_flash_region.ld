/*
 * 自定义 Flash 区域定义（2MB Flash）
 * 应用程序从 0x10009000 开始（Bootloader 32KB + 元数据 4KB 之后）
 * 覆盖 Pico SDK 默认的 pico_flash_region.ld
 *
 * Flash 布局：
 *   0x10000000 - 0x10007FFF: Bootloader (32KB)
 *   0x10008000 - 0x10008FFF: DFU 元数据 (4KB)
 *   0x10009000 - 0x101EFFFF: 应用程序 (1948KB)
 *   0x101F0000 - 0x101FFFFF: 用户配置 (64KB)
 */
FLASH(rx) : ORIGIN = 0x10009000, LENGTH = (1948 * 1024)
