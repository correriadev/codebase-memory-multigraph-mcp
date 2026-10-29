# scripts/deploy-local.ps1 - Deploy local idempotente do CBM Orchestrator
[CmdletBinding()]
param(
    [string]$TargetDir = "$env:LOCALAPPDATA\Programs\codebase-memory-mcp",
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    # Detect repo root relative to this script
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

$CliTs = Join-Path $RepoRoot "packages\cbm-orchestrator\src\cli.ts"
if (-not (Test-Path $CliTs)) {
    # Fallback to local packages dir if run inside packages/cbm-orchestrator
    $CliTsFallback = Join-Path $RepoRoot "src\cli.ts"
    if (Test-Path $CliTsFallback) {
        $CliTs = $CliTsFallback
    } else {
        Write-Error "Entrypoint cli.ts nao encontrado em: $CliTs"
        exit 1
    }
}

if (-not (Test-Path $TargetDir)) {
    New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
}

$CmdShim = Join-Path $TargetDir "cbm.cmd"
$ShShim = Join-Path $TargetDir "cbm"

$CmdContent = "@echo off`r`nnode `"$CliTs`" %*"
$ShContent = "#!/usr/bin/env sh`nexec node `"$CliTs`" `"`$@`""

[System.IO.File]::WriteAllText($CmdShim, $CmdContent, [System.Text.Encoding]::ASCII)
[System.IO.File]::WriteAllText($ShShim, $ShContent, [System.Text.Encoding]::ASCII)

Write-Host "Shims do CBM criados com sucesso em: $TargetDir" -ForegroundColor Green
Write-Host "  - CMD/PowerShell: $CmdShim"
Write-Host "  - Shell (Bash):   $ShShim"

# Verify if TargetDir is in PATH
$NormTarget = $TargetDir.Replace("/", "\").TrimEnd("\")
$InPath = ($env:PATH -split ";") | Where-Object { $_.Replace("/", "\").TrimEnd("\") -eq $NormTarget }
if (-not $InPath) {
    Write-Warning "O diretorio $TargetDir nao foi detectado no PATH."
    Write-Host "Para adicionar ao seu PATH de usuario, adicione $TargetDir a variavel PATH."
} else {
    Write-Host "Diretorio de deploy ja esta no PATH do usuario." -ForegroundColor Cyan
}
