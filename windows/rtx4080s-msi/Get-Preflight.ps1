# Read-only Windows inventory. No GPU MMIO, firmware execution or boot changes.
param()
$ErrorActionPreference = 'Stop'
$os = Get-CimInstance Win32_OperatingSystem
$board = Get-CimInstance Win32_BaseBoard
$bios = Get-CimInstance Win32_BIOS
$adapters = @(Get-CimInstance Win32_VideoController | ForEach-Object {
    $pci = $null
    if ($_.PNPDeviceID -match '^PCI\\VEN_([0-9A-F]{4})&DEV_([0-9A-F]{4})&SUBSYS_([0-9A-F]{4})([0-9A-F]{4})&REV_([0-9A-F]{2})') {
        $pci = [ordered]@{ vendor=$Matches[1]; device=$Matches[2]; subsystem_vendor=$Matches[4]; subsystem_device=$Matches[3]; revision=$Matches[5] }
    }
    [ordered]@{ name=$_.Name; driver_version=$_.DriverVersion; pci=$pci }
})
$targets = @($adapters | Where-Object { $_.pci -and $_.pci.vendor -eq '10DE' -and $_.pci.device -eq '2702' -and $_.pci.subsystem_vendor -eq '1462' -and $_.pci.subsystem_device -eq '5118' -and $_.pci.revision -eq 'A1' })
$secureBoot = $null
try {
    $state = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State' -Name UEFISecureBootEnabled
    $secureBoot = [bool]$state.UEFISecureBootEnabled
} catch { } # Unknown stays null; it must never be reported as disabled.
$summary = [ordered]@{
    schema=1
    collected_utc=[DateTime]::UtcNow.ToString('o')
    os=[ordered]@{ name=$os.Caption; version=$os.Version; build=$os.BuildNumber }
    board=[ordered]@{ manufacturer=$board.Manufacturer; product=$board.Product; version=$board.Version }
    bios=$bios.SMBIOSBIOSVersion
    adapters=$adapters
    secure_boot_enabled=$secureBoot
    expected_diagnostic_profile='rtx4080s-msi'
    exact_windows_identity_observed=($targets.Count -eq 1)
    exact_host_profile_observed=($targets.Count -eq 1 -and $board.Manufacturer -eq 'Micro-Star International Co., Ltd.' -and $board.Product -eq 'X870E GAMING PLUS WIFI (MS-7E70)' -and $bios.SMBIOSBIOSVersion -eq '1.A62' -and @($adapters | Where-Object { $_.pci -and $_.pci.vendor -eq '1002' -and $_.pci.device -eq '164E' }).Count -eq 1)
    preboot_capture_available=$false
    hardware_unlock_validated=$false
    unlock_attempted=$false
    reboot_required_for_activation=$true
}
$summary | ConvertTo-Json -Depth 7
