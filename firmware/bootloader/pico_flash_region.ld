/*
 * Bootloader Flash 区域定义（32KB）
 * 从 Flash 起始地址 0x10000000 开始，大小 32KB
 */
FLASH(rx) : ORIGIN = 0x10000000, LENGTH = (32 * 1024)
