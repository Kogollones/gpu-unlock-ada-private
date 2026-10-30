[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$Candidate,
 [Parameter(Mandatory=$true)][ValidatePattern('^[0-9a-fA-F]{40}$')][string]$CertificateThumbprint
)
# Run with Windows PowerShell 5.1 for the .NET Framework SignedCms verifier.
$ErrorActionPreference='Stop'
$candidatePath=(Resolve-Path -LiteralPath $Candidate).Path
$manifest=Get-Content -LiteralPath (Join-Path $candidatePath 'manifest.json') -Raw | ConvertFrom-Json
if($manifest.profile -ne 'rtx4080s-msi-x870e' -or $manifest.host_tests_passed -ne $true -or $manifest.hardware_tested -ne $false){throw 'Require the tested experimental candidate manifest'}
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
foreach($source in $manifest.source_sha256.PSObject.Properties) {
 $path=Join-Path $repo $source.Name
 if((Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() -ne $source.Value){throw "Source changed since verification: $($source.Name)"}
}
foreach($test in $manifest.tests.PSObject.Properties) {
 if((Get-FileHash -LiteralPath (Join-Path $candidatePath ($test.Name+'.log'))).Hash.ToLowerInvariant() -ne $test.Value.sha256){throw 'Test evidence changed'}
}
$unsigned=Join-Path $candidatePath 'RTX4080S-EXPERIMENTAL.EFI'
if((Get-FileHash -LiteralPath $unsigned).Hash.ToLowerInvariant() -ne $manifest.efi_sha256){throw 'Candidate hash mismatch'}
$cert=Get-Item -LiteralPath ('Cert:\CurrentUser\My\'+$CertificateThumbprint)
if(!$cert.HasPrivateKey -or $cert.NotBefore -gt (Get-Date) -or $cert.NotAfter -le (Get-Date) -or $cert.PublicKey.Oid.Value -ne '1.2.840.113549.1.1.1'){throw 'A valid local RSA signing certificate is required'}
$kits=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin'
$signTool=Get-ChildItem -Path (Join-Path $kits '*/x64/signtool.exe') | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if(!$signTool){throw 'Windows SDK SignTool not found'}
$out=Join-Path $candidatePath 'signed'
if(Test-Path -LiteralPath $out){throw 'Signed output already exists; preserve it'}
New-Item -ItemType Directory -Path $out | Out-Null
$signed=Join-Path $out 'RTX4080S-EXPERIMENTAL.EFI'
Copy-Item -LiteralPath $unsigned -Destination $signed
& $signTool sign /fd SHA256 /s My /sha1 $CertificateThumbprint /nph $signed
if($LASTEXITCODE -ne 0){throw 'Signing failed'}
if(-not ('GpuUnlock.EfiSignature' -as [type])){Add-Type -Path (Join-Path $PSScriptRoot 'tools/EfiSignature.cs') -ReferencedAssemblies System.dll,System.Security.dll}
$originalBytes=[IO.File]::ReadAllBytes($unsigned)
$signedBytes=[IO.File]::ReadAllBytes($signed)
[GpuUnlock.EfiSignature]::Unchanged($originalBytes,$signedBytes)
$verified=[GpuUnlock.EfiSignature]::Verify($signedBytes,$CertificateThumbprint)
# Verify the verifier refuses changed executable bytes and a wrong signer.
$tampered=[byte[]]$signedBytes.Clone();$tampered[1024]=$tampered[1024] -bxor 1
$rejected=$false
try {[void][GpuUnlock.EfiSignature]::Verify($tampered,$CertificateThumbprint)} catch {$rejected=$true}
if(!$rejected){throw 'Verifier accepted a modified executable'}
$wrongSignerRejected=$false
try {[void][GpuUnlock.EfiSignature]::Verify($signedBytes,('0'*40))} catch {$wrongSignerRejected=$true}
if(!$wrongSignerRejected){throw 'Verifier accepted the wrong signer'}
[ordered]@{
 profile=$manifest.profile
 unsigned_sha256=$manifest.efi_sha256
 signed_sha256=(Get-FileHash -LiteralPath $signed).Hash.ToLowerInvariant()
 signer_thumbprint=$CertificateThumbprint
 image_digest_sha256=$verified.ImageDigestSha256
 cryptographic_signature_verified=$true
 executable_bytes_unchanged=$true
 tamper_rejected=$true
 wrong_signer_rejected=$true
 firmware_trust_verified=$false
 hardware_tested=$false
 private_key_exported=$false
 boot_settings_changed=$false
 utc=[DateTime]::UtcNow.ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'signing-manifest.json') -Encoding utf8
Write-Output "Signed and verified offline: $signed"
