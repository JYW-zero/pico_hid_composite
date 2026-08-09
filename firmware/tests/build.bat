@echo off
REM ========================================
REM  Windows ?????????
REM  ?? MinGW GCC ???? Windows GCC ??
REM ========================================

setlocal

set CC=gcc
set CFLAGS=-I../include -Iunity/src -Imock -Wall -Wextra -std=c11
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
        test_config.c ^
        test_runner.c
set OUTPUT=run_tests.exe

echo Building unit tests...
%CC% %CFLAGS% %SRCS% -o %OUTPUT%
if %errorlevel% neq 0 (
    echo Build failed!
    exit /b 1
)

echo Build successful!
echo.
echo Running tests...
%OUTPUT%

endlocal





