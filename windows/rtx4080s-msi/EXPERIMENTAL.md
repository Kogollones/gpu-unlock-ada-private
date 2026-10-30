# MSI RTX 4080 SUPER / X870E experimental unlock

One manual hardware experiment succeeded on October 30, 2026: payload and
post-FLR full-rate readbacks passed, DMA was released, and Windows loaded the
signed NVIDIA 616.56 driver. Short FP32/FP16/BF16 numerical checks passed. See
[the boot and compute evidence](../../docs/evidence/rtx4080s-msi-windows-20261030/README.md).
This validates that observed attempt, not a general hardware qualification.
Matched speedup, repeated boots and extended stability remain unmeasured.
Host tests simulate firmware responses; they are distinct from the observed boot.

## Exact target and safeguards

This build admits the captured MSI `10de:2702 / 1462:5118`, A1, PCI `0000:01:00.0`,
BAR0 `0xdd000000`, AD103 `0x193000a1`, 16 GiB memory geometry, exact PROM/FWSEC
layout and asset hashes. The observed host is X870E GAMING PLUS WIFI, BIOS 1.A62.
Firmware paths, four loaded driver-code hashes, stop entry points and versions
must match. This is not an automatic profile generator for other machines.

The owner capture has two NVIDIA display children, one NVIDIA GOP, and one AMD
164e GOP. The adapter disconnects only five captured controller/driver pairs:
the auxiliary owner on each NVIDIA child, NVIDIA TextOut, GraphicsConsole, then
NVIDIA GOP. Each step must leave the AMD display, console/input callbacks and
every unrelated active relationship unchanged. Extra children or owners, any
unexpected graph drift, or changed driver code cause refusal. There is no USB
hotplug exception in this first X870E implementation.

The transaction requires complete firmware/payload checks, one SS0/SS1 write
sequence, one FLR, post-FLR full-rate readbacks, bus mastering off, verified DMA
cleanup and fresh checks before Windows. Uncertain live mappings remain allocated
until full power-off. A partial display release stops the application; it does
not retry or reconnect drivers. VBIOS flashing, voltage/clock changes, internal
ESP writes and boot-variable changes are not implemented.

`SEC2_CPUCTL=0xbadf5620` in the diagnostic is a denied cold read also present in
the retained transaction's original fixture. It remains an incomplete diagnostic
snapshot. The transaction does not use that value as successful SEC2 status:
SEC2 accessibility, payload completion and HALT must pass after its explicit
engine initialization. Required initial GSP/memory/permission registers and
stable entry selectors are independently checked.

## Offline evidence, October 30, 2026

- 90 real core/compatibility/PCI transaction scenarios with the private ROM and
  all five real hash-pinned firmware assets.
- 17 admission rejection cases.
- 32 X870E ownership cases replaying the captured 410-edge graph and four real
  driver-code hashes, including partial disconnects, AMD loss, wrong callbacks,
  extra consumers, claim failure, decode drift and close failure.
- 15 application-flow cases, including one-attempt behavior and Secure Boot on.
- 8 boot-I/O cases using the actual pinned Windows boot-manager bytes.
- 14 Python diagnostic/host-review regression tests.
- x64 EFI PE validation, a byte-identical rebuild, cryptographic signature
  verification, executable-byte comparison, tamper and wrong-signer rejection.

All GPU, firmware and EFI service effects in those tests are simulated. Two
byte-identical diagnostic files do not count as independent cold-boot trials.

## Manual attempt procedure

Use the prepared separate FAT32 USB. The program does not run an unlock unless
you press **U** at its menu. **S**, **Esc**, or 30 seconds selects the pinned stock
Windows loader after the initial read-only checks pass. A refusal before that
menu returns to firmware; choose Windows Boot Manager.

1. Finish normal work. Fully shut down the PC, switch off AC/PSU power for about
   30 seconds, then restore power. Leave the captured monitor and USB topology
   connected. Do not change BIOS, PCIe/ReBAR, storage or graphics settings.
2. Select the USB using the same one-time firmware boot selection used for the
   diagnostic. This exact candidate was accepted with Secure Boot enabled on
   the recorded host. If a later attempt reports a security violation,
   choose Windows Boot Manager and report that result; do not disable Secure
   Boot or replace keys merely to get past the message.
3. Read the menu. **U** starts one experimental transaction. After choosing U,
   leave peripherals untouched; the NVIDIA displays may freeze or go blank.
4. If Windows has not returned after four minutes, fully power off, wait ten
   seconds, remove the USB and boot Windows Boot Manager. Do not warm-retry the
   transaction. The same recovery applies to a stopped transaction.
5. After Windows returns, retain `RTX4080S-UNLOCK/RUNxxxx.TXT` on the USB. Review
   the current run before another attempt. A `TRANSACTION OK` line precedes the
   final handoff and alone does not prove Windows recovery or a speedup.

Keep the stock Windows boot option. One transaction, Windows recovery, driver
health and short numerical checks are recorded. Matched performance, repeated
cold/warm boots and extended stability remain outstanding. No persistent
automatic activation has been installed. The owner reported booting the USB;
the power-off/AC-cut procedure was not independently observed.

MSI documents exact-image enrollment under Secure Boot > Custom > Key Management
> Enroll Efi Image in its [AMD 800 BIOS guide, page 63](https://download.msi.com/archive/mnu_exe/mb/AMDAM5800BIOS_English.pdf).
That is a possible trust route if a later image is refused, not a change
made by these scripts. The pre-attempt database inspection found neither the local
signer certificate nor the new image hash; it does not explain how the earlier
diagnostic was admitted. The October 30 unlock image also executed with
`SecureBoot=1`; that observation does not explain the trust route. No security
settings were changed by the preparation or verification work.

## Exact tested artifact

The [private artifact bundle](../../firmware/private-windows/rtx4080s-msi-20261030/README.md)
preserves the signed EFI and its five pinned firmware assets, with original
licences. Its signed SHA-256 is
`cb33d814ad158451f207c3ce846b8a584701adf6f635c547700b46adcfc9bea2`.
The original binary still displays the pre-test "UNVALIDATED" banner. The
archived build/signing manifests likewise describe their pre-test creation
time; the separate hardware result records what happened afterwards.

## Rebuilding privately

Use MSVC x64 Build Tools and Python. Firmware extraction additionally needs the
`zstandard` Python package. Run from the repository root:

```powershell
python firmware/fetch_artifacts.py build/rtx4080s-msi/inputs
python windows/rtx4080s-msi/prepare_assets.py --inputs build/rtx4080s-msi/inputs --output build/rtx4080s-msi/assets
python windows/rtx4080s-msi/prepare_fixture.py --capture build/rtx4080s-msi/capture-review-20261030/CAP0002.JSON --manifest build/rtx4080s-msi/20261028-095849-900-2a20421d/manifest.json --output build/rtx4080s-msi/private-inputs
./windows/rtx4080s-msi/Build-Experimental.ps1 -PrivateInputs build/rtx4080s-msi/private-inputs -Assets build/rtx4080s-msi/assets
```

The ignored private input directory also requires a read-only copy of the actual
ESP's `bootmgfw.efi` and `rtx4080s_boot_profile.h` with the reviewed ESP GUID,
partition number/start/sector count, Windows file size and hash. A Windows update
can change this file; re-review it rather than relaxing the hash guard.

`Sign-Candidate.ps1` uses an existing same-user local certificate and verifies its
signature without exporting the key or enrolling firmware trust. Run it under
Windows PowerShell 5.1. `Stage-Usb.ps1` accepts only a separate FAT32 USB, verifies
all files and preserves the previous boot image; `-UseAsUsbBoot` explicitly
replaces only the USB fallback image. It never formats a drive. Without that
switch it stages an alternate file at `EFI/GPUUnlock/RTX4080S-EXPERIMENTAL.EFI`.

Restore a previous USB fallback from the verified local `usb-backup-*` directory
or `EFI/GPUUnlock/PREVIOUS-BOOTX64.EFI` on the USB. Removing the USB leaves normal
internal boot selection intact. Source credits are in [THIRD_PARTY.md](THIRD_PARTY.md).
