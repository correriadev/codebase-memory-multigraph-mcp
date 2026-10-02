param([switch]$Local)
$ErrorActionPreference = 'Stop'
$profileRoot = [Environment]::GetFolderPath('UserProfile')
$relativeTarget = if ($Local) { 'AppData/Local' } else { 'AppData' }
$target = Join-Path $profileRoot $relativeTarget
$acl = Get-Acl -LiteralPath $target
$sandboxGroup = ([Security.Principal.NTAccount]::new($env:COMPUTERNAME, 'CodexSandboxUsers')).Translate([Security.Principal.SecurityIdentifier]).Value
$specificSid = if ($Local) { 'S-1-5-21-2252986256-3806048835-2817933630-1977549711' } else { 'S-1-5-21-2548428058-3917772657-4183310038-754475743' }
$expected = @($sandboxGroup, $specificSid)
$rules = @($acl.Access | Where-Object {
    (-not $_.IsInherited) -and $_.AccessControlType -eq 'Allow' -and
    $_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value -in $expected
})
if ($rules.Count -ne 2) { throw 'Regras de sandbox diferentes das inspecionadas; nenhuma alteracao.' }
$backupRoot = Join-Path (Split-Path $PSScriptRoot -Parent) ('docs/temenos-tests/runtime-acl-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
$acl.Sddl | Set-Content -LiteralPath (Join-Path $backupRoot 'appdata-before.sddl') -Encoding UTF8
foreach ($rule in $rules) {
    $acl.RemoveAccessRuleSpecific($rule)
    $replacement = [Security.AccessControl.FileSystemAccessRule]::new(
        $rule.IdentityReference, [Security.AccessControl.FileSystemRights]::ReadAndExecute,
        $rule.InheritanceFlags, $rule.PropagationFlags, [Security.AccessControl.AccessControlType]::Allow)
    $acl.AddAccessRule($replacement)
}
Set-Acl -LiteralPath $target -AclObject $acl
(Get-Acl -LiteralPath $target).Sddl | Set-Content -LiteralPath (Join-Path $backupRoot 'appdata-after.sddl') -Encoding UTF8
Write-Output "Duas regras de escrita da sandbox em $relativeTarget limitadas a leitura/execucao. Backup: $backupRoot"
Write-Output 'Usuario, SYSTEM e Administradores preservados; nenhum processo existente encerrado.'
