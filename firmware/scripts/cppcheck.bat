@echo off
set BUILD_DIR=build
set COMPILE_DB=%BUILD_DIR%\compile_commands.json

if not exist %COMPILE_DB% (
    echo ERROR: compile_commands.json not found, build project first
    pause
    exit /b 1
)

cppcheck ^
--std=c11 ^
--enable=all ^
--suppress=missingIncludeSystem ^
--suppress=unmatchedSuppression ^
--error-exitcode=1 ^
--quiet ^
--project=%COMPILE_DB% ^
-I include/board -I include/device -I include/middleware -I include/app ^
--checkers-report=cppcheck_report.txt

echo Analysis complete, report: cppcheck_report.txt
pause
