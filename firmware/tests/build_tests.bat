@echo off
cd /d "%~dp0"
gcc -I../include -Iunity/src -Imock -std=c11 ^
  unity/src/unity.c ^
  mock/mock_gpio.c ^
  mock/mock_time.c ^
  mock/mock_flash.c ^
  mock/mock_tusb.c ^
  mock/mock_watchdog.c ^
  mock/mock_sync.c ^
  mock/mock_pico.c ^
  ../src/board/config.c ^
  ../src/middleware/debounce.c ^
  ../src/middleware/fault.c ^
  ../src/middleware/fault_full.c ^
  ../src/middleware/scheduler.c ^
  ../src/middleware/crc32.c ^
  ../src/protocol/config_validator.c ^
  ../src/protocol/config_block_receiver.c ^
  ../src/protocol/config_hid_helpers.c ^
  ../src/protocol/config_lock.c ^
  ../src/protocol/command_confirm.c ^
  ../src/protocol/capability.c ^
  ../src/protocol/command_gate.c ^
  ../src/app/keymap.c ^
  ../src/app/macro.c ^
  ../src/device/encoder.c ^
  test_debounce.c ^
  test_keymap.c ^
  test_encoder.c ^
  test_scheduler.c ^
  test_config.c ^
  test_crc.c ^
  test_macro.c ^
  test_fault.c ^
  test_config_validator.c ^
  test_config_block_receiver.c ^
  test_config_hid_helpers.c ^
  test_config_lock.c ^
  test_command_confirm.c ^
  test_capability.c ^
  test_command_gate.c ^
  test_runner.c ^
  -o run_tests.exe 2>&1
if %ERRORLEVEL% EQU 0 (
  echo Build successful! Running tests...
  run_tests.exe
) else (
  echo Build failed with error %ERRORLEVEL%
)
