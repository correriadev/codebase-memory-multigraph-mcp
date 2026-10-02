param(
    [switch]$PlanOnly,
    [switch]$Restore,
    [string]$BackupPath
)
$ErrorActionPreference = 'Stop'
$profileRoot = [IO.Path]::GetFullPath([Environment]::GetFolderPath('UserProfile'))
$backupBase = [IO.Path]::GetFullPath((Join-Path $profileRoot '.skill-backups'))
$workspaceRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$keepNames = @('temenos', 'dialogo-sombra', 'integrar-temenos')
$roots = @(
    (Join-Path $profileRoot '.codex/skills'),
    (Join-Path $profileRoot '.agents/skills'),
    (Join-Path $profileRoot '.gemini/config/skills'),
    (Join-Path $profileRoot '.gemini/antigravity-cli/skills'),
    (Join-Path $profileRoot '.gemini/antigravity/skills'),
    (Join-Path $profileRoot '.gemini/antigravity-ide/skills'),
    (Join-Path $profileRoot '.gemini/skills'),
    (Join-Path $profileRoot 'Documents/.agents/skills'),
    (Join-Path $profileRoot 'Documents/.codex/skills'),
    (Join-Path $workspaceRoot '.agents/skills'),
    (Join-Path $workspaceRoot '.codex/skills')
)
# Discover plugin skill directories without moving plugin binaries or connections.
$pluginRoots = @(
    (Join-Path $profileRoot '.codex/plugins/cache'),
    (Join-Path $profileRoot '.gemini/config/plugins'),
    (Join-Path $profileRoot '.gemini/antigravity-cli/plugins'),
    (Join-Path $workspaceRoot '.agents/plugins')
)
foreach ($pluginRoot in $pluginRoots) {
    if (Test-Path -LiteralPath $pluginRoot) {
        $roots += @(Get-ChildItem -LiteralPath $pluginRoot -Directory -Recurse -Force |
            Where-Object { $_.Name -eq 'skills' } | ForEach-Object { $_.FullName })
    }
}
$roots = @($roots | ForEach-Object { [IO.Path]::GetFullPath($_) } | Sort-Object -Unique)
function Assert-Source([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    $parent = Split-Path $resolved -Parent
    if ($parent -notin $roots) { throw "Origem fora das raízes inventariadas: $resolved" }
    if (-not $resolved.StartsWith($profileRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Origem fora do perfil autorizado: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) {
        $links = @(Get-Item -LiteralPath $resolved -Force) + @(Get-ChildItem -LiteralPath $resolved -Recurse -Force)
        if ($links | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
            throw "Link exige inspeção manual antes de mover: $resolved"
        }
    }
}
function Assert-Backup([string]$Path) {
    $resolved = [IO.Path]::GetFullPath($Path)
    if (-not $resolved.StartsWith($backupBase + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Destino fora da pasta de backup: $resolved"
    }
}
function Get-Inventory([string]$Path) {
    @(Get-ChildItem -LiteralPath $Path -File -Recurse -Force | ForEach-Object {
        [pscustomobject]@{
            Relative = $_.FullName.Substring($Path.Length).TrimStart('\', '/')
            Hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        }
    })
}
function Assert-Inventory([string]$Path, $Inventory) {
    $actual = @(Get-Inventory $Path)
    if ($actual.Count -ne @($Inventory).Count) { throw "Contagem divergente: $Path" }
    foreach ($file in $Inventory) {
        $fullPath = [IO.Path]::GetFullPath((Join-Path $Path $file.Relative))
        if (-not $fullPath.StartsWith($Path + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Arquivo fora do destino: $fullPath"
        }
        if ((Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash -ne $file.Hash) { throw "Hash divergente: $fullPath" }
    }
}
if ($Restore) {
    if (-not $BackupPath) { throw 'Informe -BackupPath para restaurar.' }
    $BackupPath = [IO.Path]::GetFullPath($BackupPath)
    Assert-Backup $BackupPath
    $manifestPath = Join-Path $BackupPath 'manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    # Plugin roots may now be empty; original root inventory is stored in manifest.
    $roots = @($manifest.Roots)
    foreach ($entry in $manifest.Entries) {
        Assert-Source $entry.Source
        Assert-Backup $entry.Backup
        if (Test-Path -LiteralPath $entry.Source) { throw "Destino já existe; não sobrescrito: $($entry.Source)" }
        Assert-Inventory $entry.Backup $entry.Files
    }
    if ($PlanOnly) { $manifest.Entries | Select-Object Source, Backup; exit 0 }
    foreach ($entry in $manifest.Entries) {
        $parent = Split-Path $entry.Source -Parent
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
        Move-Item -LiteralPath $entry.Backup -Destination $entry.Source
        Assert-Inventory $entry.Source $entry.Files
    }
    Write-Output "Restauradas $(@($manifest.Entries).Count) pastas; manifesto preservado em $manifestPath"
    exit 0
}
$sources = @()
foreach ($root in $roots) {
    if (Test-Path -LiteralPath $root) {
        foreach ($entry in Get-ChildItem -LiteralPath $root -Force) {
            if ($entry.PSIsContainer -and $entry.Name -notin $keepNames) {
                Assert-Source $entry.FullName
                $sources += $entry.FullName
            } elseif (-not $entry.PSIsContainer -and $entry.Name -eq 'SKILL.md') {
                throw "SKILL.md diretamente na raiz exige inspeção: $root"
            }
        }
    }
}
if ($PlanOnly) { $sources; Write-Output "Total de pastas: $($sources.Count)"; exit 0 }
if (-not $BackupPath) { $BackupPath = Join-Path $backupBase ('fractal-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$BackupPath = [IO.Path]::GetFullPath($BackupPath)
Assert-Backup $BackupPath
if (Test-Path -LiteralPath $BackupPath) { throw "Backup já existe: $BackupPath" }
$entries = @()
for ($index = 0; $index -lt $sources.Count; $index++) {
    $sourcePath = $sources[$index]
    $targetPath = Join-Path $BackupPath ('payload/' + $index.ToString('D3'))
    Assert-Source $sourcePath
    Assert-Backup $targetPath
    $entries += [pscustomobject]@{ Source = $sourcePath; Backup = $targetPath; Files = @(Get-Inventory $sourcePath); State = 'PLANNED' }
}
New-Item -ItemType Directory -Path (Join-Path $BackupPath 'payload') -Force | Out-Null
$manifestPath = Join-Path $BackupPath 'manifest.json'
$manifest = [pscustomobject]@{ CreatedAt = (Get-Date -Format o); Roots = $roots; Kept = $keepNames; Entries = $entries }
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
foreach ($entry in $entries) {
    # Recheck immediately before each recursive move.
    Assert-Source $entry.Source
    Assert-Backup $entry.Backup
    Move-Item -LiteralPath $entry.Source -Destination $entry.Backup
    Assert-Inventory $entry.Backup $entry.Files
    $entry.State = 'MOVED_VERIFIED'
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
}
foreach ($root in $roots) {
    if (Test-Path -LiteralPath $root) {
        $remaining = @(Get-ChildItem -LiteralPath $root -Directory -Force)
        if ($remaining | Where-Object { $_.Name -notin $keepNames }) { throw "Restaram pastas não fractais em $root" }
        Write-Output "$root : $($remaining.Name -join ', ')"
    }
}
Write-Output "Backup verificado: $BackupPath"
Write-Output "Pastas movidas: $($entries.Count)"
