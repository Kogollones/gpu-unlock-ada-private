# Exact MSI RTX 4080 SUPER / X870E USB bundle — October 30, 2026

`rtx4080s-msi-x870e-20261030.zip` is the owner's private backup of the exact
signed EFI and firmware assets used by the
[successful USB attempt](../../../docs/evidence/rtx4080s-msi-windows-20261030/README.md).
It is a board/host-specific experimental artifact, not a generic GPU installer.
The archive has not been booted as a newly staged USB; its executable and asset
bytes match those that were tested. Packaging does not add a second hardware run.

The profile pins MSI `10de:2702 / 1462:5118`, revision A1, PCI 01:00.0, the
captured X870E / BIOS 1.A62 display ownership, VBIOS/FWSEC inputs and this host's
Windows boot-manager identity. Windows or BIOS updates can invalidate these
guards. The archive makes no firmware trust or compatibility promise for other
machines.

## Contents

| Archive path | Purpose |
|---|---|
| `USB/EFI/BOOT/BOOTX64.EFI` | Exact signed EFI, SHA-256 `cb33d814ad158451f207c3ce846b8a584701adf6f635c547700b46adcfc9bea2` |
| `USB/EFI/GPUUnlock/RTX4080S-EXPERIMENTAL.EFI` | Identical alternate EFI |
| `USB/RTX4080S-ASSETS/booter_load_ad102.bin` | Pinned NVIDIA AD102 booter |
| `USB/RTX4080S-ASSETS/gsp_ga10x.bin` | Unchanged NVIDIA GSP firmware |
| `USB/RTX4080S-ASSETS/gsp_boot_ad102.bin` | Pinned GSP boot image |
| `USB/RTX4080S-ASSETS/gsp_boot_desc_ad102.bin` | Pinned GSP boot descriptor |
| `USB/RTX4080S-ASSETS/v67-ad102-610.43.03-four-plm.bin` | Deterministically generated four-PLM payload |
| `USB/RTX4080S-ASSETS/NVIDIA-LICENSE` | Complete original NVIDIA agreement |
| `SOURCE/` | Exact owner-generated profile headers used for the build; no ROM or driver bytes |
| `RECORDS/` | Original build/signing manifests |
| `NOTICES/` | Original GPL, MIT, firmware and upstream notices |
| `MANUAL.md` | Manual boot procedure and limits |
| `FILES.json` | Per-member byte lengths and SHA-256 hashes |

`manifest.json` outside the ZIP records the archive hash and every member hash.
`SHA256SUMS` provides a plain checksum list for this directory. Verify those
hashes before using or copying an artifact. The ZIP contains no private signing
key or certificate container, Windows boot-manager binary, physical VBIOS dump,
raw diagnostic capture or captured driver code. Source/profile hashes and the
EFI's public signature are not private keys.

## Use and recovery

Read [EXPERIMENTAL.md](../../../windows/rtx4080s-msi/EXPERIMENTAL.md) before any
later attempt. Preserve an existing USB's files and verify the destination is
the separate FAT32 USB. This archive performs no installation, formatting,
internal ESP writes, key enrollment, boot-variable changes or reboot.

The program requires **U** for one attempt; **S**, **Esc**, or the 30-second
timeout selects stock Windows after its read-only guards pass. Keep Windows
Boot Manager available. The current installed USB's previous diagnostic backup
is not replaced by creating this archive. The exact EFI keeps its original
pre-test "UNVALIDATED" banner; the dated hardware report documents later results.

The runtime firmware is loaded temporarily; this bundle is not a VBIOS-flashing
package. No persistent automatic activation has been installed. The current
record does not establish operation across subsequent power cycles or reboots.

## Source, licensing and rebuild

Implementation and build scripts are at
[`windows/rtx4080s-msi`](../../../windows/rtx4080s-msi/README.md), with shared
transaction code in `windows/ad104-msi`. Original authors and licences remain
credited. Firmware comes from the pinned 610.43.03 inputs documented in
[`firmware/NOTICE.md`](../../NOTICE.md); the complete NVIDIA licence is retained
inside the archive. Archiving does not relicense the firmware or authorize
public redistribution. This directory is excluded from package-source exports.

`SOURCE/` supplies the original generated profile headers for byte provenance.
The offline test/build workflow still requires the owner's excluded ROM,
captured driver bytes and actual pinned Windows boot manager as described in
the manual. A freshly compiled or re-signed binary is a new candidate unless
byte identity is verified; a passing offline test is not a hardware boot.
