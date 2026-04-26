# Scrcpy Lite 构建脚本
# 使用方法: .\build.ps1 [Release^|Debug]

param(
    [string]$config = "Release"
)

Write-Host "Building Scrcpy Lite in $config mode..." -ForegroundColor Green

# 创建 build 目录
if (!(Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build

# 配置 CMake
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake .. -G "Visual Studio 17 2022" -A x64

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# 编译
Write-Host "Building project..." -ForegroundColor Yellow
cmake --build . --config $config

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "
Build successful!" -ForegroundColor Green
Write-Host "Executable: build\bin\\\scrcpy_app.exe" -ForegroundColor Cyan
