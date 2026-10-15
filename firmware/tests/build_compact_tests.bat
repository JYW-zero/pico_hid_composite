@echo off
REM ============================================================================
REM tests/build_compact_tests.bat
REM Build and run the compact Fault backend tests (FEATURE_FAULT_COMPACT=1).
REM
REM fault.c cannot link both full and compact backends into one binary, so the
REM compact profile uses a standalone executable: test_fault_compact.c provides
REM its own Unity main; test_runner.c and fault_full.c are NOT compiled here.
REM ============================================================================

cd /d "%~dp0"

echo [1/2] Building compact fault tests (FEATURE_FAULT_COMPACT=1)...

gcc -DFEATURE_FAULT_COMPACT=1 ^
  -I../include ^
  -Iunity/src ^
  -Imock ^
  -std=c11 ^
  -Wall -Wextra ^
  unity/src/unity.c ^
  mock/mock_flash.c ^
  mock/mock_time.c ^
  mock/mock_watchdog.c ^
  mock/mock_sync.c ^
  mock/mock_pico.c ^
  ../src/middleware/fault.c ^
  ../src/middleware/fault_compact.c ^
  test_fault_compact.c ^
  -o run_compact_tests.exe

if %ERRORLEVEL% EQU 0 (
    echo [2/2] Running compact fault tests...
    run_compact_tests.exe
) else (
    echo.
    echo Build failed.
)
