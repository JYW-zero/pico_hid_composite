# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# 仅作为顶层项目时才引入 SDK（作为子目录时由顶层引入）
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Pull in Raspberry Pi Pico SDK (must be before project)
    include(pico_sdk_import.cmake)
endif()

project(pico_hid_composite C CXX ASM)

# 仅作为顶层项目时才初始化 SDK
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Initialise the Raspberry Pi Pico SDK
    pico_sdk_init()
endif()

# ==================== 分层源码扫描（两个构建目标共享同一源集） ====================
# 功能裁剪通过“整翻译单元条件编译”实现：features.h 的 FEATURE_* 开关把对应 .c
# 的全部内容包住，release 下这些翻译单元编译为空、不产生符号。两目标源集完全
# 相同、仅编译定义不同，可避免源文件清单漏排导致的链接错误。
# CONFIGURE_DEPENDS 保证新增文件自动纳入编译。
file(GLOB SRC_BOARD       CONFIGURE_DEPENDS src/board/*.c)
file(GLOB SRC_DEVICE      CONFIGURE_DEPENDS src/device/*.c)
file(GLOB SRC_DEVICE_DFU  CONFIGURE_DEPENDS src/device/dfu/*.c)
file(GLOB SRC_MIDDLEWARE  CONFIGURE_DEPENDS src/middleware/*.c)
file(GLOB SRC_PROTOCOL    CONFIGURE_DEPENDS src/protocol/*.c)
file(GLOB SRC_APP         CONFIGURE_DEPENDS src/app/*.c)

set(FIRMWARE_SOURCES
    ${SRC_BOARD}
    ${SRC_DEVICE}
    ${SRC_DEVICE_DFU}
    ${SRC_MIDDLEWARE}
    ${SRC_PROTOCOL}
    ${SRC_APP}
)

# ==================== 固件变体工厂（阶段2：一套代码、两种形态） ====================
# 裁剪矩阵与 docs/FEATURE_ROLE_SPLIT_PLAN.md §8.2 对齐：
#   dev     工程全开 -> firmware_dev.uf2
#   release 量产裁剪 -> firmware_release.uf2
# 注：host 单元测试不走本文件，由 include/features.h 的 #ifndef 默认值兜底
#     （默认全开、紧凑后端关、制造不锁）。
function(add_firmware_variant TARGET PROFILE)
    add_executable(${TARGET} ${FIRMWARE_SOURCES})

    target_include_directories(${TARGET} PRIVATE
        include
        include/board
        include/device
        include/middleware
        include/protocol
        include/app
    )

    target_link_libraries(${TARGET} PRIVATE
        pico_stdlib
        pico_unique_id
        pico_multicore
        tinyusb_device
        tinyusb_board
        hardware_spi
        hardware_watchdog
        hardware_adc
        hardware_gpio
        hardware_flash
        hardware_sync
        pico_low_power
    )

    # Core1 栈大小：SCRATCH_X 仅 4KB，最大只能设 4096
    # Flash 大小固定 2MB（当前硬件）
    target_compile_definitions(${TARGET} PRIVATE
        PICO_CORE1_STACK_SIZE=4096
        PICO_FLASH_SIZE_BYTES=2097152
    )

    if(PROFILE STREQUAL "release")
        # 量产：关闭任务级 perf / 按键统计 / 调试串口 / raw 诊断；
        # 保留工厂自检（factory_test.c 编入，运行时门控）与 Fault（走紧凑后端）；
        # 制造能力上电默认锁定（FEATURE_MFG_DEFAULT_LOCKED）。
        target_compile_definitions(${TARGET} PRIVATE
            FEATURE_FACTORY_TEST=1
            FEATURE_PERF_MONITOR=0
            FEATURE_FAULT_LOG=1
            FEATURE_KEY_STATS=0
            FEATURE_DEBUG_UART=0
            FEATURE_RAW_DIAG=0
            FEATURE_FAULT_COMPACT=1
            FEATURE_MFG_DEFAULT_LOCKED=1
            FEATURE_DFU_BOOTLOADER=0
            LOG_LEVEL=0
        )
        # 量产不实例化任何 stdio 后端（不占用 UART 引脚，printf 退化为空操作）
        pico_enable_stdio_uart(${TARGET} 0)
        pico_enable_stdio_usb(${TARGET} 0)
    else()
        # 工程：全部功能开启，紧凑 Fault 后端与制造默认锁定均关闭（历史行为）
        target_compile_definitions(${TARGET} PRIVATE
            FEATURE_FACTORY_TEST=1
            FEATURE_PERF_MONITOR=1
            FEATURE_FAULT_LOG=1
            FEATURE_KEY_STATS=1
            FEATURE_DEBUG_UART=1
            FEATURE_RAW_DIAG=1
            FEATURE_FAULT_COMPACT=0
            FEATURE_MFG_DEFAULT_LOCKED=0
            # bootloader 当前临时禁用（见文件末 [临时禁用 DFU/Bootloader]），dev 同样不编入
            FEATURE_DFU_BOOTLOADER=0
        )
        pico_enable_stdio_uart(${TARGET} 1)
        pico_enable_stdio_usb(${TARGET} 0)
    endif()

    # [临时禁用 DFU/Bootloader] 使用默认 UF2 生成（应用链接到 0x10000000）
    pico_add_extra_outputs(${TARGET})
endfunction()

add_firmware_variant(firmware_dev dev)
add_firmware_variant(firmware_release release)

# [临时禁用 DFU/Bootloader] 应用程序原计划从 0x10009000 开始（Bootloader 32KB +
# 元数据 4KB 之后），当前临时直接链接到 0x10000000。恢复时需在变体函数内：
#   target_compile_definitions(<tgt> PRIVATE PICO_FLASH_START_ADDR=0x10009000)
#   pico_add_linker_script_override_path(<tgt> ${CMAKE_CURRENT_SOURCE_DIR})
# 并改用 picotool 以 --offset 0x10009000 转换 UF2。
# 同时把对应变体的 FEATURE_DFU_BOOTLOADER 置 1，固件才会上报 CAP_DFU_UPGRADE(bit10)，
# 上位机方据此显示“一键/在线 DFU”；置 0 期间 bit10=0，上位机仅保留 BOOTSEL UF2 手动升级。
