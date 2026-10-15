/*
 * 应用程序链接脚本
 * Flash: 0x10009000, 1948KB（Bootloader 32KB + 元数据 4KB + 配置区 64KB 之后）
 * RAM:   0x20000000, 512KB
 *
 * 基于 Pico SDK 2.x RP2350 默认链接脚本修改，
 * Flash 起始地址偏移 36KB（0x9000）。
 *
 * 注意：当前项目使用 pico_flash_region.ld 覆盖方式，此文件为备用参考。
 */

MEMORY
{
    FLASH(rx)  : ORIGIN = 0x10009000, LENGTH = (1948 * 1024)
    RAM(rwx)   : ORIGIN = 0x20000000, LENGTH = (512 * 1024)
    SCRATCH_X(rwx) : ORIGIN = 0x20040000, LENGTH = (4 * 1024)
    SCRATCH_Y(rwx) : ORIGIN = 0x20041000, LENGTH = (4 * 1024)
}

ENTRY(_entry_point)

SECTIONS
{
    /* 向量表放在应用程序 Flash 起始位置 */
    .vector_table :
    {
        KEEP(*(.vector_table))
        KEEP(*(.vector_table.*))
    } > FLASH

    /* 代码段 */
    .text :
    {
        *(.text*)
        *(.init*)
        *(.fini*)
        *(.rodata*)
        *(.gnu.linkonce.t*)
        *(.gnu.linkonce.r*)
        . = ALIGN(4);
        __etext = .;
    } > FLASH

    /* 初始化数据 */
    .data : AT (__etext)
    {
        __data_start__ = .;
        *(.data*)
        *(.gnu.linkonce.d*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM

    /* 未初始化数据 */
    .bss :
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    /* 堆 */
    .heap :
    {
        __heap_start__ = .;
        end = __heap_start__;
        _end = end;
        . = . + 0x4000;  /* 16KB 堆 */
        . = ALIGN(8);
        __heap_end__ = .;
    } > RAM

    /* 栈 */
    .stack :
    {
        . = ALIGN(8);
        __StackTop = ORIGIN(RAM) + LENGTH(RAM);
        __StackLimit = __StackTop - 0x4000;  /* 16KB 栈 */
        . = __StackLimit;
    } > RAM

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}

PROVIDE(__flash_binary_start = ORIGIN(FLASH));
PROVIDE(__flash_binary_end = ORIGIN(FLASH) + LENGTH(FLASH));
PROVIDE(__flash_info_location = ORIGIN(FLASH) + LENGTH(FLASH) - 256);
