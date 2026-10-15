@echo off
REM ============================================================
REM  Pico HID 复合设备 - 一键编译脚本
REM  编译 Bootloader + 应用程序，并合并为完整固件
REM ============================================================

setlocal enabledelayedexpansion

set FIRMWARE_DIR=%~dp0
set BUILD_DIR=%FIRMWARE_DIR%build
set BOOTLOADER_BUILD_DIR=%FIRMWARE_DIR%bootloader\build

echo ============================================================
echo  编译应用程序
echo ============================================================
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [错误] CMake 配置失败
    exit /b 1
)
nmake
if errorlevel 1 (
    echo [错误] 应用程序编译失败
    exit /b 1
)
echo [成功] 应用程序编译完成
echo.

echo ============================================================
echo  编译 Bootloader
echo ============================================================
if not exist "%BOOTLOADER_BUILD_DIR%" mkdir "%BOOTLOADER_BUILD_DIR%"
cd "%BOOTLOADER_BUILD_DIR%"
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [错误] Bootloader CMake 配置失败
    exit /b 1
)
nmake
if errorlevel 1 (
    echo [错误] Bootloader 编译失败
    exit /b 1
)
echo [成功] Bootloader 编译完成
echo.

echo ============================================================
echo  合并固件
echo ============================================================
cd "%FIRMWARE_DIR%"
python scripts\merge_firmware.py ^
    bootloader\build\pico_hid_bootloader.uf2 ^
    build\pico_hid_composite.uf2 ^
    build\pico_hid_composite_full.uf2
if errorlevel 1 (
    echo [错误] 固件合并失败
    exit /b 1
)

echo.
echo ============================================================
echo  编译完成!
echo ============================================================
echo  输出文件:
echo    Bootloader:   bootloader\build\pico_hid_bootloader.uf2
echo    应用程序:     build\pico_hid_composite.uf2
echo    完整固件:     build\pico_hid_composite_full.uf2
echo.
echo  首次烧录请使用完整固件（按住 BOOTSEL 后拖入 pico_hid_composite_full.uf2）
echo  后续升级可通过上位机 DFU 功能使用应用程序固件（pico_hid_composite.uf2）
echo ============================================================

endlocal
