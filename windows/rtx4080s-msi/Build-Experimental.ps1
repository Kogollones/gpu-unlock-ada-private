param(
 [Parameter(Mandatory=$true)][string]$PrivateInputs,
 [Parameter(Mandatory=$true)][string]$Assets,
 [string]$Python='python',
 [string]$VcVars64,
 [switch]$CompileOnly
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$private=(Resolve-Path -LiteralPath $PrivateInputs).Path
$assetDir=(Resolve-Path -LiteralPath $Assets).Path
foreach($path in @($repo,$private,$assetDir)) {if($path -match '["&|<>%^!\r\n]'){throw 'Unsupported command-shell path characters'}}
foreach($file in @('physical-prom.bin','bootmgfw.efi','rtx4080s_boot_profile.h','rtx4080s_captured.h','rtx4080s_graph.h','driver-140.bin','driver-415.bin','driver-420.bin','driver-569.bin')) {
 if(!(Test-Path -LiteralPath (Join-Path $private $file) -PathType Leaf)){throw "Missing reviewed private input: $file"}
}
if(!$VcVars64) {
 $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
 $VcVars64=@(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'VC\Auxiliary\Build\vcvars64.bat') | Select-Object -First 1
}
if(!$VcVars64 -or !(Test-Path -LiteralPath $VcVars64) -or $VcVars64 -match '["&|<>%^!\r\n]'){throw 'MSVC x64 setup script required'}
$out=Join-Path $repo ('build/rtx4080s-msi/candidate-'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $out | Out-Null
$shared=Join-Path $repo 'windows/ad104-msi'
$flags='/nologo /std:c11 /W3 /WX /O2 /D_CRT_SECURE_NO_WARNINGS /D__KERNEL__ /DGU_UEFI_PORT /DGU_AD104_TRANSACTION /DGU_ENTRY_LATCHED_SS1_FULL /DGU_RTX4080S_MSI'
$flags+=' /I"'+$shared+'/compat" /I"'+$shared+'/vendor/edk2/Include" /I"'+$shared+'/vendor/edk2/Include/X64" /I"'+$private+'"'
$core=@('port','compat','checks','profile','sha256','vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')
$coreSources=($core | ForEach-Object {'"'+$shared+'/'+$_+'.c"'}) -join ' '
$commands=[Collections.Generic.List[string]]::new()
$commands.Add('@echo off')
$commands.Add('call "'+$VcVars64+'" >nul')
$commands.Add('if errorlevel 1 exit /b 1')
$commands.Add('cd /d "'+$out+'"')
$cases=@(
 @{Name='transaction';Source="$shared/tests/test_efi_transaction.c";Extra=$coreSources;Args=('"'+$private+'/physical-prom.bin" "'+$assetDir+'"')},
 @{Name='guards';Source="$shared/tests/test_guards.c";Extra=$coreSources;Args=('"'+$private+'/physical-prom.bin" "'+$assetDir+'"')},
 @{Name='ownership';Source="$PSScriptRoot/test_release.c";Extra=('"'+$shared+'/sha256.c" "'+$shared+'/format.c"');Args=('"'+$private+'"')},
 @{Name='app';Source="$shared/tests/test_app_flow.c";Extra='';Args=''},
 @{Name='boot-io';Source="$shared/tests/test_boot_io.c";Extra=('"'+$shared+'/sha256.c" "'+$shared+'/format.c"');Args=('"'+$private+'/bootmgfw.efi" "'+$assetDir+'"')}
)
foreach($case in $cases) {
 $commands.Add('cl '+$flags+' "'+$case.Source+'" '+$case.Extra+' /Fe:'+ $case.Name+'.exe')
 $commands.Add('if errorlevel 1 exit /b 1')
 if(!$CompileOnly) {
  $commands.Add($case.Name+'.exe '+$case.Args+' > '+$case.Name+'.log 2>&1')
  $commands.Add('if errorlevel 1 exit /b 1')
 }
}
$efiSources=@($core | ForEach-Object {Join-Path $shared ($_+'.c')})
$efiSources+=@("$shared/app.c","$shared/boot_io.c","$shared/format.c","$PSScriptRoot/x870e_release.c")
$objects=@()
for($i=0;$i -lt $efiSources.Count;$i++) {
 $object='efi-'+$i+'.obj';$objects+=$object
 $commands.Add('cl '+$flags+' /Oi /GS- /Gs65536 /Zl /c "'+$efiSources[$i]+'" /Fo:'+ $object)
 $commands.Add('if errorlevel 1 exit /b 1')
}
$commands.Add('cl '+$flags+' /Od /Oi- /GS- /Zl /c "'+$shared+'/runtime.c" /Fo:efi-runtime.obj')
$commands.Add('if errorlevel 1 exit /b 1')
$commands.Add('link /nologo /subsystem:efi_application /entry:UefiMain /machine:x64 /nodefaultlib /incremental:no /fixed:no /dynamicbase:no /nxcompat:no /brepro /out:RTX4080S-EXPERIMENTAL.EFI '+($objects -join ' ')+' efi-runtime.obj')
$commands.Add('exit /b %errorlevel%')
Set-Content -LiteralPath (Join-Path $out 'compile.cmd') -Value $commands -Encoding ascii
Write-Output "Offline build output: $out"
& $env:ComSpec /d /c ('"'+(Join-Path $out 'compile.cmd')+'"')
if($LASTEXITCODE -ne 0){throw "Compile/test failed. Inspect logs in $out; no boot kit is approved."}
& $Python (Join-Path $PSScriptRoot 'verify_candidate.py') $out --private-inputs $private --assets $assetDir $(if($CompileOnly){'--compile-only'})
if($LASTEXITCODE -ne 0){throw 'Candidate verification failed'}
Write-Output "Candidate built offline: $out. Hardware execution and performance remain unvalidated."
