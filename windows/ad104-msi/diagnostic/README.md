# Read-only AD104 UEFI probe

v0.4 diagnostic source used to capture the reference board. It reads PCI configuration, bounded PROM/register data, paths, protocol ownership and selected loaded driver code. It writes JSON to its own USB only; it does not unlock, write GPU MMIO, change BME, release graphics drivers or edit boot variables.

Build using `Build.ps1 -Python python -VcVars <path-to-vcvars64.bat>`. Output: `build/AD104-PROBE.EFI`. A new build manifest is not hardware qualification. Preserve previous USB boot files before manually deploying a reviewed probe. No formatter/automatic installer is provided.

An independent diagnostic-only profile is available with
`Build.ps1 -Profile rtx4080s-msi`. It produces `build/RTX4080S-PROBE.EFI`,
pins MSI `2702 / 1462:5118` and its observed Windows BAR0, and retains the
original profile as the default. This does not add an unlock target. See the
[preparation status](../../rtx4080s-msi/README.md). Capture analysis binds the
profile to the selected build manifest and rejects a different profile.

Captures contain full proprietary ROM/loaded code and local details. Do not commit or publish raw captures. The parent port's `tools/extract_private_capture.py` extracts matching private test inputs. Other code/ROM profiles require separate review, not a bypassed check.

MIT licence in `LICENSE.txt`; EDK II and related notices in `THIRD_PARTY.md`. PE-validator copyright/licence remains in its source.
