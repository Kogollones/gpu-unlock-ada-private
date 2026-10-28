# Prepare files on the current disk only. Does not install or execute an EFI.
param([string]$Python='python', [string]$VcVars)
$ErrorActionPreference='Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$diagnostic = Join-Path $repoRoot 'windows\ad104-msi\diagnostic'
$inventoryText = & (Join-Path $PSScriptRoot 'Get-Preflight.ps1')
$inventory = ($inventoryText -join "`n") | ConvertFrom-Json
if (!$inventory.exact_windows_identity_observed) { throw 'Expected one MSI 10de:2702 / 1462:5118 A1. No kit created.' }
$buildArguments = @{ Python=$Python; Profile='rtx4080s-msi' }
if ($VcVars) { $buildArguments.VcVars=$VcVars }
& (Join-Path $diagnostic 'Build.ps1') @buildArguments
if (!$?) { throw 'Diagnostic build failed' }
$manifestPath = Join-Path $diagnostic 'build\manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.profile -ne 'rtx4080s-msi' -or $manifest.binary -ne 'RTX4080S-PROBE.EFI' -or $manifest.gpu_write_capability -ne $false -or $manifest.hardware_tested -ne $false) { throw 'Unexpected diagnostic manifest' }
$binary = Join-Path $diagnostic ('build\' + $manifest.binary)
if ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.sha256) { throw 'Diagnostic hash mismatch' }
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)
$kit = Join-Path $repoRoot ('build\rtx4080s-msi\' + $stamp)
$boot = Join-Path $kit 'USB\EFI\BOOT'
New-Item -ItemType Directory -Path $boot | Out-Null
Copy-Item -LiteralPath $binary -Destination (Join-Path $boot 'BOOTX64.EFI')
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $kit 'manifest.json')
Copy-Item -LiteralPath (Join-Path $diagnostic 'LICENSE.txt') -Destination (Join-Path $kit 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $diagnostic 'THIRD_PARTY.md') -Destination (Join-Path $kit 'THIRD_PARTY.md')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.md') -Destination (Join-Path $kit 'README-FIRST.md')
Set-Content -LiteralPath (Join-Path $kit 'windows-inventory.json') -Value $inventoryText -Encoding utf8
$checksums = [ordered]@{}
Get-ChildItem -LiteralPath $kit -File -Recurse | Sort-Object FullName | ForEach-Object {
    $checksums[$_.FullName.Substring($kit.Length+1)] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$checksums | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $kit 'SHA256SUMS.json') -Encoding utf8
Write-Output ('Prepared diagnostic kit: ' + $kit)
Write-Output ('Secure Boot (Windows registry): ' + $inventory.secure_boot_enabled)
Write-Output 'No restart or activation performed. This unsigned diagnostic is not an unlocker.'
