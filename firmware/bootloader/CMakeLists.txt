# Bootloader CMakeLists.txt
# 独立编译，生成 bootloader.uf2

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == Pico SDK 引入（与主项目一致） ==
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

set(PICO_BOARD pico2 CACHE STRING "Board type")

# 使用父目录的 pico_sdk_import.cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../pico_sdk_import.cmake)

project(pico_hid_bootloader C CXX ASM)

pico_sdk_init()

# Bootloader 可执行文件
add_executable(pico_hid_bootloader
    src/main.c
    src/dfu_handler.c
    src/usb_descriptors.c
    ../src/device/dfu/dfu_metadata.c
    ../src/middleware/sha256.c
    ../src/middleware/hmac_sha256.c
)

target_include_directories(pico_hid_bootloader PRIVATE
    include
    ../include
    ../include/device
    ../include/device/dfu
    ../include/protocol
    ../include/middleware
)

target_link_libraries(pico_hid_bootloader PRIVATE
    pico_stdlib
    pico_unique_id
    tinyusb_device
    tinyusb_board
    hardware_flash
    hardware_watchdog
    hardware_gpio
    hardware_sync
)

# 禁用 USB stdio（避免 CDC 与 HID 接口冲突），只使用 UART 调试输出
pico_enable_stdio_usb(pico_hid_bootloader 0)
pico_enable_stdio_uart(pico_hid_bootloader 1)

# Bootloader 从 Flash 起始地址开始，大小限制 32KB
# 通过覆盖 pico_flash_region.ld 实现
pico_add_linker_script_override_path(pico_hid_bootloader ${CMAKE_CURRENT_SOURCE_DIR})

# 设置 Flash 大小为 2MB
target_compile_definitions(pico_hid_bootloader PRIVATE PICO_FLASH_SIZE_BYTES=2097152)

# 禁用默认 UF2 生成（picotool 对 Bootloader 段检查有问题），用自定义命令
set(PICO_NO_UF2 1)
pico_add_extra_outputs(pico_hid_bootloader)

add_custom_command(TARGET pico_hid_bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pico_hid_bootloader> ${CMAKE_BINARY_DIR}/pico_hid_bootloader.bin
    COMMAND picotool uf2 convert ${CMAKE_BINARY_DIR}/pico_hid_bootloader.bin -t bin ${CMAKE_BINARY_DIR}/pico_hid_bootloader.uf2 -o 0x10000000 --family rp2350-arm-s
    COMMENT "Generating Bootloader UF2"
    VERBATIM
)

# 编译选项
target_compile_definitions(pico_hid_bootloader PRIVATE
    PICO_BOOTLOADER=1
    CFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE
)
