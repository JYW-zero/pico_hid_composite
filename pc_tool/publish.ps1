# HID Config Tool 发布脚本
# 用法: .\publish.ps1 -Version "1.0.0"

param(
    [string]$Version = "1.0.0",
    [string]$Configuration = "Release",
    [string]$Runtime = "win-x64",
    [bool]$SelfContained = $true,
    [bool]$SingleFile = $true,
    [bool]$Trim = $false
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  HID Config Tool 发布脚本" -ForegroundColor Cyan
Write-Host "  版本: $Version" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 项目路径
$ProjectPath = "src\HidConfigTool.App\HidConfigTool.App.csproj"
$OutputDir = "publish\$Version\$Runtime"

# 清理旧的发布文件
if (Test-Path $OutputDir) {
    Write-Host "清理旧的发布文件..." -ForegroundColor Yellow
    Remove-Item -Path $OutputDir -Recurse -Force
}

# 发布命令
Write-Host "正在发布..." -ForegroundColor Green
Write-Host "运行时: $Runtime" -ForegroundColor Gray
Write-Host "配置: $Configuration" -ForegroundColor Gray
Write-Host "自包含: $SelfContained" -ForegroundColor Gray
Write-Host "单文件: $SingleFile" -ForegroundColor Gray
Write-Host "裁剪: $Trim" -ForegroundColor Gray
Write-Host ""

$publishArgs = @(
    "publish", $ProjectPath,
    "-c", $Configuration,
    "-r", $Runtime,
    "--self-contained", $SelfContained.ToString().ToLower(),
    "-o", $OutputDir,
    "/p:Version=$Version",
    "/p:AssemblyVersion=$Version",
    "/p:FileVersion=$Version"
)

if ($SingleFile) {
    $publishArgs += "/p:PublishSingleFile=true"
    $publishArgs += "/p:IncludeNativeLibrariesForSelfExtract=true"
}

if ($Trim) {
    $publishArgs += "/p:PublishTrimmed=true"
}

& dotnet @publishArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "发布失败!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "发布成功!" -ForegroundColor Green
Write-Host "输出目录: $OutputDir" -ForegroundColor Green

# 计算文件大小
$exePath = Join-Path $OutputDir "HidConfigTool.App.exe"
if (Test-Path $exePath) {
    $fileSize = (Get-Item $exePath).Length / 1MB
    Write-Host "主程序大小: $([math]::Round($fileSize, 2)) MB" -ForegroundColor Gray
}

# 创建压缩包
Write-Host ""
Write-Host "正在创建压缩包..." -ForegroundColor Yellow
$zipPath = "publish\HIDConfigTool-$Version-$Runtime.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Compress-Archive -Path "$OutputDir\*" -DestinationPath $zipPath -Force

$zipSize = (Get-Item $zipPath).Length / 1MB
Write-Host "压缩包已创建: $zipPath" -ForegroundColor Green
Write-Host "压缩包大小: $([math]::Round($zipSize, 2)) MB" -ForegroundColor Gray

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  发布完成!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
