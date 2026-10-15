@echo off
REM 配置存储模块单元测试编译脚本
REM 使用 gcc 编译，在 PC 上运行测试

setlocal
cd /d "%~dp0"

echo ========================================
echo   编译配置模块单元测试
echo ========================================

gcc -std=c11 -Wall -Wextra -I../../include -I. ^
    test_config.c ^
    ../../src/board/config.c ^
    -o test_config.exe

if %errorlevel% neq 0 (
    echo.
    echo [错误] 编译失败！
    exit /b 1
)

echo.
echo [成功] 编译完成，运行测试...
echo.

test_config.exe

echo.
echo 测试完成，退出码: %errorlevel%

endlocal
