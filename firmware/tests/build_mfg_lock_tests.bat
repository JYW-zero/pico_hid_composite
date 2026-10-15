@echo off
REM ============================================================================
REM tests/build_mfg_lock_tests.bat
REM Build and run the release manufacturing-lock capability tests.
REM
REM capability.c derives mfg availability from compile-time FEATURE_* macros,
REM so the locked (release) shape is compiled standalone here with the release
REM trim matrix + FEATURE_MFG_DEFAULT_LOCKED=1; test_mfg_lock.c has its own
REM Unity main and needs no hardware mocks (capability.c is hardware-free).
REM ============================================================================

cd /d "%~dp0"

echo [1/2] Building mfg-lock tests (release matrix, MFG_DEFAULT_LOCKED=1)...

gcc -DFEATURE_FACTORY_TEST=1 ^
  -DFEATURE_PERF_MONITOR=0 ^
  -DFEATURE_FAULT_LOG=1 ^
  -DFEATURE_KEY_STATS=0 ^
  -DFEATURE_DEBUG_UART=0 ^
  -DFEATURE_RAW_DIAG=0 ^
  -DFEATURE_FAULT_COMPACT=1 ^
  -DFEATURE_MFG_DEFAULT_LOCKED=1 ^
  -DFEATURE_DFU_BOOTLOADER=0 ^
  -I../include ^
  -Iunity/src ^
  -Imock ^
  -std=c11 ^
  -Wall -Wextra ^
  unity/src/unity.c ^
  ../src/protocol/capability.c ^
  ../src/protocol/command_gate.c ^
  test_mfg_lock.c ^
  -o run_mfg_lock_tests.exe

if %ERRORLEVEL% EQU 0 (
    echo [2/2] Running mfg-lock tests...
    run_mfg_lock_tests.exe
) else (
    echo.
    echo Build failed.
)
