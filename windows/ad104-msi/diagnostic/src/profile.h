/* SPDX-License-Identifier: MIT
 * Diagnostic identities only. These definitions never admit an unlock.
 * The new MSI AD103 profile is based on Windows inventory, not a UEFI capture.
 * BAR/location mismatch refuses all MMIO reads; inventory is still recorded.
 */
#ifndef GPU_DIAGNOSTIC_PROFILE_H
#define GPU_DIAGNOSTIC_PROFILE_H
#ifdef PROBE_RTX4080S_MSI
#define PROBE_PROFILE_ID "rtx4080s-msi"
#define PROBE_DEVICE_ID 0x2702u
#define PROBE_SUBSYSTEM_ID 0x5118u
#define PROBE_CHIP_ID 0x193000a1u
#define PROBE_BAR0 0xdd000000u
#define PROBE_CACHED_ROM_MIN 512u
#define PROBE_CACHED_ROM_MAX (512u * 1024u)
#define PROBE_DIRECTORY L"RTX4080S-PROBE"
#define PROBE_BANNER L"MSI RTX 4080 SUPER diagnostic - no GPU writes or unlock.\r\n"
#else
#define PROBE_PROFILE_ID "ad104-msi"
#define PROBE_DEVICE_ID 0x2782u
#define PROBE_SUBSYSTEM_ID 0x5132u
#define PROBE_CHIP_ID 0x194000a1u
#define PROBE_BAR0 0x80000000u
#define PROBE_CACHED_ROM_MIN 150016u
#define PROBE_CACHED_ROM_MAX 150016u
#define PROBE_DIRECTORY L"AD104-PROBE"
#define PROBE_BANNER L"AD104 diagnostic v0.4.0 - no GPU writes or unlock.\r\n"
#endif
#endif
