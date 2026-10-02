param([switch]$Update)
$ErrorActionPreference = 'Stop'
$skillNames = @('temenos', 'dialogo-sombra', 'integrar-temenos')
$profileRoot = [Environment]::GetFolderPath('UserProfile')
$destinationRoots = @(
    (Join-Path $profileRoot '.codex/skills'),
    (Join-Path $profileRoot '.gemini/config/skills'),
    (Join-Path $profileRoot '.gemini/antigravity-cli/skills')
)
$copyPlan = @()
$updatePlan = @()
# Preflight all destinations before changing any of them.
foreach ($destinationRoot in $destinationRoots) {
    foreach ($skillName in $skillNames) {
        $sourcePath = Join-Path $PSScriptRoot $skillName
        $targetPath = Join-Path $destinationRoot $skillName
        if (Test-Path -LiteralPath $targetPath) {
            if ($Update) {
                $updatePlan += @{ Source = $sourcePath; Target = $targetPath; Root = $destinationRoot }
                continue
            }
            $sourceFiles = @(Get-ChildItem -LiteralPath $sourcePath -File -Recurse)
            $targetFiles = @(Get-ChildItem -LiteralPath $targetPath -File -Recurse)
            if ($sourceFiles.Count -ne $targetFiles.Count) { throw "Destino diferente: $targetPath" }
            foreach ($sourceFile in $sourceFiles) {
                $relativePath = $sourceFile.FullName.Substring($sourcePath.Length).TrimStart('\', '/')
                $targetFile = Join-Path $targetPath $relativePath
                if (-not (Test-Path -LiteralPath $targetFile -PathType Leaf)) { throw "Arquivo ausente: $targetFile" }
                if ((Get-FileHash -LiteralPath $sourceFile.FullName).Hash -ne (Get-FileHash -LiteralPath $targetFile).Hash) {
                    throw "Destino diferente; não sobrescrito: $targetFile"
                }
            }
        } else {
            $copyPlan += @{ Source = $sourcePath; Target = $targetPath; Root = $destinationRoot }
        }
    }
}
if ($updatePlan.Count -gt 0) {
    $backupRoot = Join-Path $profileRoot ('.skill-backups/fractal-update-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    New-Item -ItemType Directory -Path $backupRoot | Out-Null
    $backupManifest = @()
    for ($index = 0; $index -lt $updatePlan.Count; $index++) {
        $entry = $updatePlan[$index]
        $backupPath = Join-Path $backupRoot $index.ToString('D3')
        Copy-Item -LiteralPath $entry.Target -Destination $backupPath -Recurse
        foreach ($targetFile in Get-ChildItem -LiteralPath $entry.Target -File -Recurse) {
            $relativePath = $targetFile.FullName.Substring($entry.Target.Length).TrimStart('\', '/')
            if ((Get-FileHash -LiteralPath $targetFile.FullName).Hash -ne (Get-FileHash -LiteralPath (Join-Path $backupPath $relativePath)).Hash) {
                throw "Backup divergente: $($targetFile.FullName)"
            }
        }
        $backupManifest += @{ Original = $entry.Target; Backup = $backupPath }
    }
    $backupManifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backupRoot 'manifest.json') -Encoding UTF8
    foreach ($entry in $updatePlan) {
        foreach ($sourceFile in Get-ChildItem -LiteralPath $entry.Source -File -Recurse) {
            $relativePath = $sourceFile.FullName.Substring($entry.Source.Length).TrimStart('\', '/')
            $targetFile = Join-Path $entry.Target $relativePath
            New-Item -ItemType Directory -Path (Split-Path $targetFile -Parent) -Force | Out-Null
            Copy-Item -LiteralPath $sourceFile.FullName -Destination $targetFile -Force
        }
    }
    Write-Output "Backup anterior verificado: $backupRoot"
}
foreach ($entry in $copyPlan) {
    New-Item -ItemType Directory -Path $entry.Root -Force | Out-Null
    Copy-Item -LiteralPath $entry.Source -Destination $entry.Target -Recurse
}
# Verify every deployed file, including destinations already present.
foreach ($destinationRoot in $destinationRoots) {
    foreach ($skillName in $skillNames) {
        $sourcePath = Join-Path $PSScriptRoot $skillName
        $targetPath = Join-Path $destinationRoot $skillName
        foreach ($sourceFile in Get-ChildItem -LiteralPath $sourcePath -File -Recurse) {
            $relativePath = $sourceFile.FullName.Substring($sourcePath.Length).TrimStart('\', '/')
            $targetFile = Join-Path $targetPath $relativePath
            if ((Get-FileHash -LiteralPath $sourceFile.FullName).Hash -ne (Get-FileHash -LiteralPath $targetFile).Hash) {
                throw "Falha de verificação: $targetFile"
            }
        }
        Write-Output "Verificado: $targetPath"
    }
}
