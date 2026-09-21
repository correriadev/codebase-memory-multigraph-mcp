# build-windows.ps1 - Native Windows build script for codebase-memory-mcp.
# Uses MSYS2 CLANG64 toolchain to build codebase-memory-mcp.exe.

param(
    [switch]$WithUI,
    [string]$MsysRoot = "C:\msys64",
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$RemainingArgs
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$BashExe = Join-Path $MsysRoot "usr\bin\bash.exe"
$EnvExe = Join-Path $MsysRoot "usr\bin\env.exe"

if (-not (Test-Path $BashExe)) {
    Write-Host "Error: MSYS2 not found at $MsysRoot." -ForegroundColor Red
    Write-Host "Install MSYS2 via: winget install MSYS2.MSYS2" -ForegroundColor Yellow
    Write-Host "Then install the CLANG64 toolchain in MSYS2:" -ForegroundColor Yellow
    Write-Host "  pacman -S mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-zlib make" -ForegroundColor Yellow
    exit 1
}

# Convert Windows path to MSYS2 POSIX path
$Drive = $RepoRoot.Drive.Name.ToLower()
$PathWithoutDrive = $RepoRoot.Path.Substring(3).Replace('\', '/')
$MsysPath = "/$Drive/$PathWithoutDrive"

$BuildArgs = @("CC=clang", "CXX=clang++")
if ($WithUI) {
    $BuildArgs += "--with-ui"
}
if ($RemainingArgs) {
    $BuildArgs += $RemainingArgs
}

$BuildCmd = "cd '$MsysPath' && scripts/build.sh " + ($BuildArgs -join " ")

Write-Host "Building codebase-memory-mcp.exe (Native Windows via MSYS2 CLANG64)..." -ForegroundColor Cyan
& $EnvExe MSYSTEM=CLANG64 MSYS2_PATH_TYPE=inherit $BashExe -l -c "$BuildCmd"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Native build successful: build\c\codebase-memory-mcp.exe" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
