<#
.SYNOPSIS
    pico_hid_composite 项目构建脚本
.DESCRIPTION
    支持编译固件、上位机，或全部编译
    智能检测 Pico SDK，不存在时自动从官方拉取
.PARAMETER Target
    编译目标：Firmware / PcTool / All（默认 All）
.PARAMETER Config
    编译配置：Debug / Release（默认 Debug）
.PARAMETER SdkPath
    手动指定 Pico SDK 路径（可选）
.EXAMPLE
    .\build.ps1 -Target All
    .\build.ps1 -Target Firmware -Config Release
    .\build.ps1 -Target Firmware -SdkPath "C:\pico-sdk"
#>

param(
    [ValidateSet("Firmware", "PcTool", "All")]
    [string]$Target = "All",
    
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    
    [string]$SdkPath = ""
)

$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# ============================================================
# 颜色输出函数
# ============================================================
function Write-Success { param($msg) Write-Host "[OK] " -ForegroundColor Green -NoNewline; Write-Host $msg }
function Write-Info    { param($msg) Write-Host "[..] " -ForegroundColor Cyan -NoNewline; Write-Host $msg }
function Write-Error   { param($msg) Write-Host "[XX] " -ForegroundColor Red -NoNewline; Write-Host $msg }
function Write-Warn    { param($msg) Write-Host "[!!] " -ForegroundColor Yellow -NoNewline; Write-Host $msg }

# ============================================================
# SDK 智能检测与管理
# ============================================================
function Get-SdkVersion {
    $versionFile = Join-Path $ScriptDir "firmware\lib\.sdk_version"
    if (Test-Path $versionFile) {
        return (Get-Content $versionFile -Raw).Trim()
    }
    return "2.3.0"  # 默认版本
}

function Find-PicoSdk {
    param([string]$PreferredPath = "")
    
    # 1. 优先使用手动指定的路径
    if ($PreferredPath -and (Test-Path $PreferredPath)) {
        Write-Host "   来源: 手动指定" -ForegroundColor DarkGray
        return $PreferredPath
    }
    
    # 2. 检测环境变量
    if ($env:PICO_SDK_PATH -and (Test-Path $env:PICO_SDK_PATH)) {
        Write-Host "   来源: 环境变量 PICO_SDK_PATH" -ForegroundColor DarkGray
        return $env:PICO_SDK_PATH
    }
    
    # 3. 检测 Pico VS Code 扩展默认路径
    $sdkVersion = Get-SdkVersion
    $pluginSdk = "$env:USERPROFILE\.pico-sdk\sdk\$sdkVersion"
    if (Test-Path $pluginSdk) {
        Write-Host "   来源: Pico VS Code 扩展" -ForegroundColor DarkGray
        return $pluginSdk
    }
    
    # 4. 检测项目本地 lib/pico-sdk
    $localSdk = Join-Path $ScriptDir "firmware\lib\pico-sdk"
    if (Test-Path $localSdk) {
        Write-Host "   来源: 项目本地 lib/pico-sdk" -ForegroundColor DarkGray
        return $localSdk
    }
    
    return $null
}

function Find-Toolchain {
    # 1. 环境变量
    if ($env:PICO_TOOLCHAIN_PATH -and (Test-Path $env:PICO_TOOLCHAIN_PATH)) {
        Write-Host "   来源: 环境变量 PICO_TOOLCHAIN_PATH" -ForegroundColor DarkGray
        return $env:PICO_TOOLCHAIN_PATH
    }
    
    # 2. Pico 扩展默认路径
    $pluginToolchain = "$env:USERPROFILE\.pico-sdk\toolchain\15_2_Rel1"
    if (Test-Path $pluginToolchain) {
        Write-Host "   来源: Pico VS Code 扩展" -ForegroundColor DarkGray
        return $pluginToolchain
    }
    
    # 3. PATH 里找 arm-none-eabi-gcc
    $gcc = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
    if ($gcc) {
        $toolchainPath = Split-Path (Split-Path $gcc.Source)
        Write-Host "   来源: 系统 PATH" -ForegroundColor DarkGray
        return $toolchainPath
    }
    
    return $null
}

function Install-PicoSdk {
    param([string]$Version, [string]$DestPath)
    
    Write-Warn "未检测到 Pico SDK，正在自动从官方拉取..."
    Write-Host "   版本: $Version"
    Write-Host "   目标: $DestPath"
    Write-Host ""
    
    # 检查 git 是否可用
    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        throw "未找到 git，请先安装 Git 或手动设置 PICO_SDK_PATH"
    }
    
    # 浅克隆 SDK
    Write-Info "  克隆 Pico SDK..."
    & git clone --depth 1 --branch $Version https://github.com/raspberrypi/pico-sdk.git $DestPath
    if ($LASTEXITCODE -ne 0) {
        throw "git clone 失败"
    }
    
    # 初始化子模块（TinyUSB 等）
    Write-Info "  初始化子模块（TinyUSB 等）..."
    Push-Location $DestPath
    & git submodule update --init --depth 1
    $submoduleResult = $LASTEXITCODE
    Pop-Location
    
    if ($submoduleResult -ne 0) {
        Write-Warn "子模块初始化可能有问题，请检查网络连接"
    }
    
    Write-Success "Pico SDK 拉取完成!"
    return $DestPath
}

function Setup-PicoEnv {
    param([string]$PreferredSdkPath = "")
    
    Write-Info "检测 Pico SDK..."
    
    # 查找 SDK
    $sdkPath = Find-PicoSdk -PreferredPath $PreferredSdkPath
    
    # 如果没找到，自动拉取
    if (-not $sdkPath) {
        $version = Get-SdkVersion
        $destPath = Join-Path $ScriptDir "firmware\lib\pico-sdk"
        $sdkPath = Install-PicoSdk -Version $version -DestPath $destPath
    }
    
    Write-Success "Pico SDK 路径: $sdkPath"
    
    # 设置环境变量
    $env:PICO_SDK_PATH = $sdkPath
    
    # 查找工具链
    Write-Host ""
    Write-Info "检测 Arm 工具链..."
    $toolchainPath = Find-Toolchain
    
    if ($toolchainPath) {
        Write-Success "工具链路径: $toolchainPath"
        $env:PICO_TOOLCHAIN_PATH = $toolchainPath
        
        # 添加工具链到 PATH
        $toolBin = Join-Path $toolchainPath "bin"
        if (Test-Path $toolBin) {
            $env:Path = "$toolBin;$env:Path"
        }
    } else {
        Write-Warn "未检测到 Arm GNU Toolchain"
        Write-Host "   请安装 Arm GNU Toolchain 或使用 Pico VS Code 扩展"
        Write-Host "   下载地址: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads"
    }
    
    # 添加其他工具到 PATH
    $additionalPaths = @(
        "$env:USERPROFILE\.pico-sdk\picotool\2.3.0\picotool",
        "$env:USERPROFILE\.pico-sdk\ninja\v1.13.2",
        "$env:USERPROFILE\.pico-sdk\cmake\v4.3.4\bin"
    )
    
    foreach ($p in $additionalPaths) {
        if (Test-Path $p) {
            $env:Path = "$p;$env:Path"
        }
    }
    
    return $sdkPath
}

# ============================================================
# 主程序
# ============================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "  pico_hid_composite 构建脚本" -ForegroundColor Magenta
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "目标: $Target"
Write-Host "配置: $Config"
Write-Host ""

$success = $true

# ========== 编译固件 ==========
if ($Target -eq "Firmware" -or $Target -eq "All") {
    Write-Info "开始编译固件..."
    Write-Host ""
    
    $firmwareDir = Join-Path $ScriptDir "firmware"
    $buildDir = Join-Path $firmwareDir "build"
    
    try {
        # 设置 Pico 开发环境
        Setup-PicoEnv -PreferredSdkPath $SdkPath
        
        Write-Host ""
        
        # 创建 build 目录
        if (-not (Test-Path $buildDir)) {
            New-Item -ItemType Directory -Path $buildDir | Out-Null
        }
        
        # CMake 配置
        Write-Info "  CMake 配置中..."
        $buildTypeArg = "-DCMAKE_BUILD_TYPE=" + $Config
        Push-Location $buildDir
        & cmake $firmwareDir -G Ninja $buildTypeArg
        $cmakeResult = $LASTEXITCODE
        Pop-Location
        
        if ($cmakeResult -ne 0) {
            throw "CMake 配置失败 (退出码: $cmakeResult)"
        }
        
        # Ninja 编译
        Write-Info "  Ninja 编译中..."
        & ninja -C $buildDir
        $ninjaResult = $LASTEXITCODE
        
        if ($ninjaResult -ne 0) {
            throw "Ninja 编译失败 (退出码: $ninjaResult)"
        }
        
        # 检查输出文件
        $uf2File = Join-Path $buildDir "pico_hid_composite.uf2"
        if (Test-Path $uf2File) {
            $size = (Get-Item $uf2File).Length / 1KB
            Write-Success "固件编译成功! ($([math]::Round($size, 1)) KB)"
            Write-Host "      输出: $uf2File"
        } else {
            Write-Error "固件编译失败: 未找到 .uf2 文件"
            $success = $false
        }
    }
    catch {
        Write-Error "固件编译失败: $_"
        $success = $false
    }
    
    Write-Host ""
}

# ========== 编译上位机 ==========
if ($Target -eq "PcTool" -or $Target -eq "All") {
    Write-Info "开始编译上位机..."
    
    $pcToolDir = Join-Path $ScriptDir "pc_tool"
    
    try {
        Push-Location $pcToolDir
        
        # 还原 NuGet 包
        Write-Info "  还原 NuGet 包..."
        dotnet restore | Out-Null
        
        # 编译
        Write-Info "  dotnet build 中..."
        dotnet build --configuration $Config --no-restore
        
        if ($LASTEXITCODE -eq 0) {
            Write-Success "上位机编译成功!"
            
            # 显示主程序路径
            $exePath = Join-Path $pcToolDir "src\HidConfigTool.App\bin\$Config\net10.0-windows10.0.19041.0\HidConfigTool.App.exe"
            if (Test-Path $exePath) {
                Write-Host "      输出: $exePath"
            }
        } else {
            Write-Error "上位机编译失败!"
            $success = $false
        }
        
        Pop-Location
    }
    catch {
        Write-Error "上位机编译失败: $_"
        $success = $false
    }
    
    Write-Host ""
}

# ========== 总结 ==========
Write-Host "========================================" -ForegroundColor Magenta
if ($success) {
    Write-Success "构建完成! 全部成功"
    exit 0
} else {
    Write-Error "构建完成! 存在失败"
    exit 1
}
