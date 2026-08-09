@echo off
REM ========================================
REM  ??????????
REM  ?????????????
REM ========================================
setlocal

echo ========================================
echo   HID Composite Device - Unit Tests
echo ========================================
echo.

REM ?????????
cd /d "%~dp0"

set CC=gcc
set CFLAGS=-I../include -Iunity/src -Imock -Wall -Wextra -std=c11 -O2
set SRCS=unity/src/unity.c ^
        mock/mock_gpio.c ^
        mock/mock_time.c ^
        mock/mock_flash.c ^
        ../src/board/config.c ^
        ../src/middleware/debounce.c ^
        ../src/middleware/fault.c ^
        ../src/middleware/scheduler.c ^
        ../src/app/keymap.c ^
        ../src/device/encoder.c ^
        test_debounce.c ^
        test_keymap.c ^
        test_encoder.c ^
        test_scheduler.c ^
        test_fault.c ^
        test_config.c ^
        test_runner.c
set OUTPUT=run_tests.exe

echo [1/2] Building...
%CC% %CFLAGS% %SRCS% -o %OUTPUT%
if %errorlevel% neq 0 (
    echo.
    echo ========================================
    echo   BUILD FAILED
    echo ========================================
    endlocal
    exit /b 1
)
echo Build OK.
echo.

echo [2/2] Running tests...
echo.
%OUTPUT%
set TEST_RESULT=%errorlevel%

echo.
echo ========================================
if %TEST_RESULT% equ 0 (
    echo   ALL TESTS PASSED
) else (
    echo   SOME TESTS FAILED
)
echo ========================================

endlocal
exit /b %TEST_RESULT%

