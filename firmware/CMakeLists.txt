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

# Add executable. Default name is the project name, version 0.1

add_executable(pico_hid_composite
    src/app/main.c
)

# 分层自动扫描源码，CONFIGURE_DEPENDS保证新增文件自动编译
file(GLOB SRC_BOARD      CONFIGURE_DEPENDS src/board/*.c)
file(GLOB SRC_DEVICE     CONFIGURE_DEPENDS src/device/*.c)
file(GLOB SRC_MIDDLEWARE CONFIGURE_DEPENDS src/middleware/*.c)
file(GLOB SRC_PROTOCOL   CONFIGURE_DEPENDS src/protocol/*.c)
file(GLOB SRC_APP        CONFIGURE_DEPENDS src/app/*.c)

target_sources(pico_hid_composite PRIVATE
    ${SRC_BOARD}
    ${SRC_DEVICE}
    ${SRC_MIDDLEWARE}
    ${SRC_PROTOCOL}
    ${SRC_APP}
)

# 头文件搜索路径，target私有作用域，不污染全局
target_include_directories(pico_hid_composite PRIVATE
    include
    include/board
    include/device
    include/middleware
    include/protocol
    include/app
)

# 链接底层依赖库
target_link_libraries(pico_hid_composite PRIVATE
    pico_stdlib
    pico_unique_id
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

# 链接脚本由pico-sdk自动根据芯片型号选择，无需手动指定
# 如需自定义内存布局，再调用 pico_set_linker_script()

# Uncomment this line to enable fix for Errata RP2040-E5 (the fix requires use of GPIO 15)
#target_compile_definitions(pico_hid_composite PUBLIC PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1)

pico_add_extra_outputs(pico_hid_composite)

# add url via pico_set_program_url




