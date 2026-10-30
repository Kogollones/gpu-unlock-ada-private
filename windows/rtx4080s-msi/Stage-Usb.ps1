[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$Candidate,
 [Parameter(Mandatory=$true)][string]$Assets,
 [Parameter(Mandatory=$true)][ValidatePattern('^[D-Zd-z]$')][string]$DriveLetter,
 [switch]$UseAsUsbBoot
)
$ErrorActionPreference='Stop'
$candidatePath=(Resolve-Path -LiteralPath $Candidate).Path
$assetPath=(Resolve-Path -LiteralPath $Assets).Path
$manifest=Get-Content -LiteralPath (Join-Path $candidatePath 'manifest.json') -Raw | ConvertFrom-Json
$signed=Join-Path $candidatePath 'signed/RTX4080S-EXPERIMENTAL.EFI'
$signing=Get-Content -LiteralPath (Join-Path $candidatePath 'signed/signing-manifest.json') -Raw | ConvertFrom-Json
if($manifest.profile -ne 'rtx4080s-msi-x870e' -or $manifest.host_tests_passed -ne $true -or $signing.cryptographic_signature_verified -ne $true -or $signing.executable_bytes_unchanged -ne $true -or $signing.unsigned_sha256 -ne $manifest.efi_sha256){throw 'Require the verified, tested and signed candidate'}
if((Get-FileHash -LiteralPath $signed).Hash.ToLowerInvariant() -ne $signing.signed_sha256){throw 'Signed candidate changed'}
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
foreach($source in $manifest.source_sha256.PSObject.Properties) {
 if((Get-FileHash -LiteralPath (Join-Path $repo $source.Name)).Hash.ToLowerInvariant() -ne $source.Value){throw "Source changed since verification: $($source.Name)"}
}
foreach($test in $manifest.tests.PSObject.Properties) {
 if((Get-FileHash -LiteralPath (Join-Path $candidatePath ($test.Name+'.log'))).Hash.ToLowerInvariant() -ne $test.Value.sha256){throw 'Test evidence changed'}
}
$volume=Get-Volume -DriveLetter $DriveLetter
$partition=Get-Partition -DriveLetter $DriveLetter
$disk=$partition | Get-Disk
if($volume.FileSystem -ne 'FAT32' -or $disk.BusType -ne 'USB' -or $disk.IsBoot -or $disk.IsSystem){throw 'Only a separate FAT32 USB data volume is permitted'}
$root=$DriveLetter.ToUpper()+':\'
$pins=[ordered]@{
 'booter_load_ad102.bin'='6ee05b20e1dc244c08c052f36ef28469b8d09f4955d1fe33e48a179e11a7fd49'
 'v67-ad102-610.43.03-four-plm.bin'='44c36b2d6b3424e53e18f3e55db85c833655de071ce66ce42749588a4a7dc8cb'
 'gsp_ga10x.bin'='572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a'
 'gsp_boot_ad102.bin'='45d6335a8185d6ff13e8d1484cea73f68d3d13aa1c19dc080ed1c3482dd5bac0'
 'gsp_boot_desc_ad102.bin'='83f8f3f1f5f9496e0ea8fc7103c2beb64d0b6de2f5ffe725f13514de5a95fa42'
 'NVIDIA-LICENSE'='c692fb14ad4f499c6aee1b9e144e6905148c420e6225b17cb2937366c1e507ad'
}
$assetDest=Join-Path $root 'RTX4080S-ASSETS'
foreach($item in $pins.GetEnumerator()) {
 if((Get-FileHash -LiteralPath (Join-Path $assetPath $item.Key)).Hash.ToLowerInvariant() -ne $item.Value){throw "Asset hash mismatch: $($item.Key)"}
 $existing=Join-Path $assetDest $item.Key
 if((Test-Path -LiteralPath $existing) -and (Get-FileHash -LiteralPath $existing).Hash.ToLowerInvariant() -ne $item.Value){throw "Existing USB file differs: $existing"}
}
$efiDir=Join-Path $root 'EFI/GPUUnlock'
$efiDest=Join-Path $efiDir 'RTX4080S-EXPERIMENTAL.EFI'
if(Test-Path -LiteralPath $efiDest){throw 'Candidate already staged; preserve the existing copy'}
$fallback=Join-Path $root 'EFI/BOOT/BOOTX64.EFI'
$originalHash=if(Test-Path -LiteralPath $fallback){(Get-FileHash -LiteralPath $fallback).Hash.ToLowerInvariant()}else{$null}
$backup=Join-Path $candidatePath ('usb-backup-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
if($originalHash) {
 Copy-Item -LiteralPath $fallback -Destination (Join-Path $backup 'BOOTX64.EFI')
 if((Get-FileHash -LiteralPath (Join-Path $backup 'BOOTX64.EFI')).Hash.ToLowerInvariant() -ne $originalHash){throw 'USB boot backup verification failed'}
}
New-Item -ItemType Directory -Path $assetDest,$efiDir -Force | Out-Null
foreach($item in $pins.GetEnumerator()) {
 $destination=Join-Path $assetDest $item.Key
 if(!(Test-Path -LiteralPath $destination)){Copy-Item -LiteralPath (Join-Path $assetPath $item.Key) -Destination $destination}
 if((Get-FileHash -LiteralPath $destination).Hash.ToLowerInvariant() -ne $item.Value){throw 'USB asset readback mismatch'}
}
Copy-Item -LiteralPath $signed -Destination $efiDest
if((Get-FileHash -LiteralPath $efiDest).Hash.ToLowerInvariant() -ne $signing.signed_sha256){throw 'USB image readback mismatch'}
Copy-Item -LiteralPath (Join-Path $candidatePath 'manifest.json'),(Join-Path $candidatePath 'signed/signing-manifest.json') -Destination $efiDir
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'EXPERIMENTAL.md') -Destination (Join-Path $efiDir 'READ-ME.md')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'THIRD_PARTY.md'),(Join-Path $PSScriptRoot '../ad104-msi/THIRD_PARTY_NOTICES.md'),(Join-Path $PSScriptRoot '../ad104-msi/UPSTREAM-WINDOWS-NOTICES.md'),(Join-Path $repo 'LICENSE') -Destination $efiDir
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../ad104-msi/LICENSES') -Destination $efiDir -Recurse
if($UseAsUsbBoot) {
 if($originalHash) {
  $old=Join-Path $efiDir 'PREVIOUS-BOOTX64.EFI'
  if(Test-Path -LiteralPath $old){throw 'Previous boot backup already exists'}
  Copy-Item -LiteralPath (Join-Path $backup 'BOOTX64.EFI') -Destination $old
  if((Get-FileHash -LiteralPath $old).Hash.ToLowerInvariant() -ne $originalHash){throw 'USB backup readback failed'}
 }
 New-Item -ItemType Directory -Path (Split-Path $fallback) -Force | Out-Null
 Copy-Item -LiteralPath $signed -Destination $fallback -Force
 if((Get-FileHash -LiteralPath $fallback).Hash.ToLowerInvariant() -ne $signing.signed_sha256){throw 'USB boot image readback mismatch; restore verified local backup'}
}
[ordered]@{
 utc=[DateTime]::UtcNow.ToString('o');drive=$root;disk_number=$disk.Number;
 original_boot_sha256=$originalHash;candidate_sha256=$signing.signed_sha256;
 candidate_path=$efiDest;boot_fallback_replaced=[bool]$UseAsUsbBoot;
 backup=$backup;assets_readback_verified=$true;image_readback_verified=$true;
 firmware_settings_changed=$false;internal_esp_changed=$false;gpu_executed=$false;reboot_performed=$false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $candidatePath 'usb-staging-receipt.json') -Encoding utf8
Write-Output "USB candidate staged and readback verified: $efiDest"
