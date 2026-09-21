# codebase-memory-mcp setup script (Windows)
# Default: runs official install.ps1 (download pre-built native Windows binary)
# -FromSource: builds from source natively on Windows using MSYS2 CLANG64 toolchain

param(
    [switch]$FromSource,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BinaryName = "codebase-memory-mcp.exe"

if ($Help) {
    Write-Host ""
    Write-Host "Usage: .\scripts\setup-windows.ps1 [-FromSource] [-Help]"
    Write-Host ""
    Write-Host "  Default:      Install pre-built native Windows binary via install.ps1"
    Write-Host "  -FromSource:  Build from source natively on Windows using MSYS2 CLANG64"
    Write-Host ""
    exit 0
}

Write-Host ""
Write-Host "codebase-memory-mcp installer (Windows)" -ForegroundColor White
Write-Host ""

if ($FromSource) {
    Write-Host "Building codebase-memory-mcp from source natively on Windows..." -ForegroundColor Cyan
    $BuildScript = Join-Path $PSScriptRoot "build-windows.ps1"
    & pwsh -File $BuildScript

    $BuiltBinary = Join-Path $RepoRoot "build\c\$BinaryName"
    if (-not (Test-Path $BuiltBinary)) {
        Write-Host "Error: Built binary not found at $BuiltBinary" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "Installing locally compiled binary..." -ForegroundColor Cyan
    & $BuiltBinary install -y
} else {
    $RootInstaller = Join-Path $RepoRoot "install.ps1"
    if (Test-Path $RootInstaller) {
        & pwsh -File $RootInstaller
    } else {
        Write-Host "Invoking remote installer..." -ForegroundColor Cyan
        Invoke-RestMethod "https://raw.githubusercontent.com/DeusData/codebase-memory-mcp/main/install.ps1" | Invoke-Expression
    }
}
