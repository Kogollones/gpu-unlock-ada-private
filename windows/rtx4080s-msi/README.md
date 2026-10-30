# MSI RTX 4080 SUPER Windows port

Status, October 30, 2026: **one successful manual USB unlock and Windows
handoff**, with signed NVIDIA 616.56, Secure Boot enabled and passing short CUDA
numerical checks. FP16/FP32 cuBLAS at 4096³ measured 186.41 TFLOP/s and BF16/FP32
176.14 TFLOP/s. No matched stock run or extended stability test exists for this
machine. See the [complete result and comparison](../../docs/evidence/rtx4080s-msi-windows-20261030/README.md),
[exact tested firmware bundle](../../firmware/private-windows/rtx4080s-msi-20261030/README.md),
and [implementation, offline tests and manual procedure](EXPERIMENTAL.md).
The preparation commands below still build only the read-only diagnostic.
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

Preparation now also requires the observed MSI X870E motherboard, BIOS 1.A62 and
AMD `1002:164e` integrated adapter. A BIOS update or hardware change requires a
profile review. Every new kit includes `port-readiness.json` with the remaining
development work. Secure Boot enabled or unknown is reported as unresolved for
the unsigned image; it does not prevent an offline build.

The diagnostic profile requires `10de:2702 / 1462:5118`, revision A1, one unique
matching GPU, PCI `0000:01:00.0`, memory decode enabled and bus mastering disabled,
BAR0 `0xdd000000` with a matching 16 MiB firmware resource descriptor, and AD103
chip `0x193000a1`. The BAR/location were confirmed in the October 30 UEFI
captures and the successful transaction. If those guards fail, the probe records inventory and the
refusal instead of enabling decode or reading MMIO. It preserves the existing
AD104 diagnostic profile and does not modify the unlock application's admission.

The bounded register/PROM reads derive from the existing diagnostic and the
repository's AD103 evidence. GOP enumeration supports multiple display children.
The observed owner topology is three monitors: one connected to AMD integrated
graphics and two to NVIDIA. This cabling does not establish firmware console
ownership or authorize disconnecting either NVIDIA display child.

The observed host is MSI X870E GAMING PLUS WIFI, BIOS 1.A62, Windows 11 build
26200, NVIDIA 616.56, VBIOS 95.03.44.00.6a, 16 GiB card. Secure Boot is currently
enabled. `Prepare.ps1` produces an unsigned diagnostic; the owner's October 28
kit was subsequently signed with the existing local certificate and booted
successfully. The owner reports selecting the USB without changing security
settings. No firmware keys were enrolled by this work. The experimental unlock
uses the same signer and executed with `SecureBoot=1` on October 30. This
confirms acceptance on this host; it does not identify the trust route or
establish acceptance on other firmware installations.

The October 30 captures are already available and reviewed. For a later
diagnostic capture, use a separate FAT32 USB and preserve existing USB boot
files. The probe refuses to log on a non-USB device, saves new
`RTX4080S-PROBE/CAPnnnn.JSON` files, and returns to firmware after eight seconds;
it does not chainload Windows. Select Windows Boot Manager afterwards.
Raw captures contain firmware and loaded code: keep them private and outside Git.

Analyze a later capture with the manifest from that exact kit:

```powershell
python windows/ad104-msi/diagnostic/tools/analyze.py X:/RTX4080S-PROBE/CAP0001.JSON --manifest build/rtx4080s-msi/YOUR-KIT/manifest.json
```

For a combined host and capture review, use:

```powershell
python windows/rtx4080s-msi/review.py --inventory build/rtx4080s-msi/YOUR-KIT/windows-inventory.json --capture X:/RTX4080S-PROBE/CAP0001.JSON --manifest build/rtx4080s-msi/YOUR-KIT/manifest.json
```

The review rejects contradictory PCI/register data and identifies AMD GOPs,
NVIDIA console ownership, missing driver code and incomplete ownership graphs.
GOP counts describe firmware output handles, which may differ from the number
of plugged-in monitors. They do not authorize releasing a driver. Supply
`--mock` when analyzing synthetic fixtures. A supplied capture is not independently
authenticated; the report always leaves activation and hardware qualification
false. Optional `--output NEW-FILE.json` refuses to overwrite an existing file.

The October 30 captures were reviewed for ROM/FWSEC inputs, memory geometry,
BARs and the complete console/driver ownership graph before implementing the
write-enabled profile and replaying its transaction/cleanup. The subsequent
USB log records payload completion, full-rate selectors before and after FLR,
and released DMA. Windows driver health and numerical checks were collected
after that boot. The diagnostic review still reports activation/qualification
false: a capture review alone cannot establish an actual unlock. Hardware
evidence is recorded separately, and a matched speedup remains unmeasured.

The archived candidate and its pre-test manifests retain their original
"UNVALIDATED" wording and false hardware-test fields. They describe what was
known when the exact binary was built; the dated result supplies later evidence.
Do not rebuild solely to change the banner and call that new image tested.

Reference implementations are board-specific: the checkout's
[AD104 port](../ad104-msi/README.md) pins another MSI GPU/Z490 host;
[Goldlionren's AD103 port](https://github.com/Goldlionren/gpu-unlock-4080s)
pins `2702 / 19da:4702` and an Intel GOP topology. Neither admits this MSI/AMD
system without independent capture and review.
