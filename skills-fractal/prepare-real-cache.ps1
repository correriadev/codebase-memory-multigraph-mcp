param()
$ErrorActionPreference = 'Stop'
$profileRoot = [Environment]::GetFolderPath('UserProfile')
$cacheParent = Join-Path $profileRoot '.cache'
$realCache = Join-Path $cacheParent 'codebase-memory-mcp'
if (-not (Test-Path -LiteralPath (Join-Path $realCache 'C-Users-corre-Documents-harness-kit.db'))) {
    throw 'Grafo real do HarnessKit ausente; nenhuma permissao alterada.'
}
$acl = Get-Acl -LiteralPath $cacheParent
$sandboxGroup = ([Security.Principal.NTAccount]::new($env:COMPUTERNAME, 'CodexSandboxUsers')).Translate([Security.Principal.SecurityIdentifier]).Value
$expected = @($sandboxGroup, 'S-1-5-21-2782030190-84299153-3546617168-1754943142')
$rules = @($acl.Access | Where-Object {
    (-not $_.IsInherited) -and $_.AccessControlType -eq 'Allow' -and
    $_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value -in $expected
})
if ($rules.Count -ne 2) { throw 'As duas regras de sandbox esperadas nao correspondem; nenhuma alteracao.' }
$backupRoot = Join-Path (Split-Path $PSScriptRoot -Parent) ('docs/temenos-tests/cache-acl-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
$originalSddl = $acl.Sddl
$originalSddl | Set-Content -LiteralPath (Join-Path $backupRoot 'cache-before.sddl') -Encoding UTF8
foreach ($rule in $rules) {
    $acl.RemoveAccessRuleSpecific($rule)
    $replacement = [Security.AccessControl.FileSystemAccessRule]::new(
        $rule.IdentityReference, [Security.AccessControl.FileSystemRights]::ReadAndExecute,
        $rule.InheritanceFlags, $rule.PropagationFlags, [Security.AccessControl.AccessControlType]::Allow)
    $acl.AddAccessRule($replacement)
}
Set-Acl -LiteralPath $cacheParent -AclObject $acl
$runtimeRoot = Join-Path $realCache 'fractal-runtime'
New-Item -ItemType Directory -Path $runtimeRoot -Force | Out-Null
$runtimeAcl = [Security.AccessControl.DirectorySecurity]::new()
$currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent().User
$runtimeAcl.SetOwner($currentIdentity)
$runtimeAcl.SetAccessRuleProtection($true, $false)
$runtimeRule = [Security.AccessControl.FileSystemAccessRule]::new(
    $currentIdentity, [Security.AccessControl.FileSystemRights]::FullControl,
    ([Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit),
    [Security.AccessControl.PropagationFlags]::None, [Security.AccessControl.AccessControlType]::Allow)
$runtimeAcl.AddAccessRule($runtimeRule)
Set-Acl -LiteralPath $runtimeRoot -AclObject $runtimeAcl
(Get-Acl -LiteralPath $cacheParent).Sddl | Set-Content -LiteralPath (Join-Path $backupRoot 'cache-after.sddl') -Encoding UTF8
Write-Output "Duas regras de sandbox na pasta .cache limitadas a leitura/execucao; backup: $backupRoot"
Write-Output "Runtime privado: $runtimeRoot"
Write-Output 'AppData e permissões do grafo real permaneceram intactos.'
