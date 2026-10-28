# MSI RTX 4080 SUPER Windows port preparation

Status: a diagnostic build, **not an implemented or hardware-validated unlock**.
Preparation can run entirely in the current Windows session:

```powershell
.\windows\rtx4080s-msi\Get-Preflight.ps1
.\windows\rtx4080s-msi\Prepare.ps1
```

`Prepare.ps1` collects model/driver inventory without serial numbers or device
instance identifiers, builds the read-only UEFI probe with MSVC, runs host mocks,
checks the PE and source access rules, and creates a new timestamped kit beneath
`build/rtx4080s-msi/`. It writes only to the checkout. The kit includes
`USB/EFI/BOOT/BOOTX64.EFI`, manifests, checksums and notices. It does not execute
the EFI, install a driver, copy to an ESP or USB, change security/boot settings,
restart Windows, or run a GPU benchmark.

The diagnostic profile requires `10de:2702 / 1462:5118`, revision A1, one unique
matching GPU, PCI `0000:01:00.0`, memory decode enabled and bus mastering disabled,
BAR0 `0xdd000000` with a matching 16 MiB firmware resource descriptor, and AD103
chip `0x193000a1`. The BAR/location come from Windows inventory and remain
unconfirmed in UEFI. If those guards fail, the probe records inventory and the
refusal instead of enabling decode or reading MMIO. It preserves the existing
AD104 diagnostic profile and does not modify the unlock application's admission.

The bounded register/PROM reads derive from the existing diagnostic and the
repository's AD103 evidence. GOP enumeration supports multiple display children.
The observed owner topology is three monitors: one connected to AMD integrated
graphics and two to NVIDIA. This cabling does not establish firmware console
ownership or authorize disconnecting either NVIDIA display child.

The observed host is MSI X870E GAMING PLUS WIFI, BIOS 1.A62, Windows 11 build
26200, NVIDIA 616.56, VBIOS 95.03.44.00.6a, 16 GiB card. Secure Boot is currently
enabled according to the Windows registry. The kit is unsigned and **cannot be
booted under that policy as-is**. No signing keys or certificates are generated
or enrolled, and Secure Boot is not disabled by these scripts.

The next development input is a manually authorized preboot capture on a
separate FAT32 USB. Do not run that step until the owner is ready to shut down
and the unsigned-image boot policy has been resolved. Preserve existing USB
boot files. The probe refuses to log on a non-USB device, saves new
`RTX4080S-PROBE/CAPnnnn.JSON` files, and returns to firmware after eight seconds;
it does not chainload Windows. Select Windows Boot Manager afterwards.
Raw captures contain firmware and loaded code: keep them private and outside Git.

Analyze a later capture with the manifest from that exact kit:

```powershell
python windows/ad104-msi/diagnostic/tools/analyze.py X:/RTX4080S-PROBE/CAP0001.JSON --manifest build/rtx4080s-msi/YOUR-KIT/manifest.json
```

A capture must be reviewed for ROM/FWSEC inputs, memory geometry, BARs and the
complete console/driver ownership graph before implementing the write-enabled
profile and replaying its transaction/cleanup. Later hardware validation needs
the actual Windows handoff, current-boot logs, numerical correctness and matched
stock/unlocked measurements. A compiled EFI or passing host mock is not proof
of an unlock. No unlock or performance gain has been observed on this PC.

Reference implementations are board-specific: the checkout's
[AD104 port](../ad104-msi/README.md) pins another MSI GPU/Z490 host;
[Goldlionren's AD103 port](https://github.com/Goldlionren/gpu-unlock-4080s)
pins `2702 / 19da:4702` and an Intel GOP topology. Neither admits this MSI/AMD
system without independent capture and review.
