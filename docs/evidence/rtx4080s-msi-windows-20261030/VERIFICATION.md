# RTX 4080 SUPER post-USB verification — October 30, 2026

The USB unlock transaction succeeded on this MSI RTX 4080 SUPER / X870E system. Windows loaded the signed NVIDIA 616.56 driver, the display adapter reports no device error, and the short CUDA numerical checks passed. A matched before/after speedup and long-term stability have not been measured.

## Evidence from this boot

- USB log: `E:\RTX4080S-UNLOCK\RUN0001.TXT`, copied here without modification. Log timestamp: 2026-10-30 20:17:40 UTC. Windows boot: 20:17:42.500 UTC. The close timestamps corroborate this boot's handoff.
- USB fallback and alternate EFI both match the signed candidate SHA-256 `cb33d814ad158451f207c3ce846b8a584701adf6f635c547700b46adcfc9bea2` from `candidate-20261030-195526-925`.
- Firmware reported Secure Boot enabled and recorded the operator's U selection, one physical attempt and five successful NVIDIA display-owner detach stages, with the AMD display preserved by the admission checks.
- Payload completion marker matched `0x67b00001`; required permission checks passed and the firmware halted.
- Initial speed readout: `0x410080`, `full=0`. Selectors were set to SS0 `0x88888888`, SS1 `0x8`; effective readout became zero, `full=1`.
- After one PCIe function-level reset, both selectors persisted and the effective readout remained `full=1`.
- Final logged result: `result=0 safe=1 retained=0 started=1 quiesced=1 flrs=1 DMA_live=0`, followed by `TRANSACTION OK`.
- The log explicitly says the last pre-Windows guard is unlogged. Windows recovery is corroborated by the live driver, matching boot timestamp and CUDA execution, rather than inferred solely from that success line.

## Windows driver and GPU health

- MSI NVIDIA GeForce RTX 4080 SUPER, PCI 01:00.0, `10de:2702 / 1462:5118`; Ada Lovelace, compute capability 8.9.
- NVIDIA driver 616.56 / Windows version 32.0.16.1656; `nvlddmkm` is running in WDDM mode. The installed kernel driver has a valid Microsoft Windows Hardware Compatibility Publisher signature.
- NVIDIA and AMD display adapters both report Status OK and ConfigManagerErrorCode 0. NVIDIA reports an active 2560 x 1440 display at 144 Hz.
- VBIOS `95.03.44.00.6a`; NVIDIA reports 16,376 MiB usable framebuffer memory on the 16 GiB board. Power limit remains 320 W.
- Secure Boot remains enabled.
- No nvlddmkm, Display, WHEA, DxgKrnl or GPU-specific Kernel-PnP warning/error events were found in the System log from boot through 20:25:08 UTC. There are unrelated system events; this is not a claim that all Windows logs are empty.
- PCIe reports x16 width and Gen 4 maximum. The low-load snapshot used Gen 1 and showed zero replay errors. NVIDIA documents that link generation can decrease while idle: https://docs.nvidia.com/deploy/nvidia-smi/index.html#gpu-link-information
- Sampled temperatures were 37–40 C at light load and 43–56 C in the compute test; these are snapshots, not continuously measured peaks. The post-test temperature was 40 C, with no thermal or hardware slowdown flag active.

## Short CUDA test

The existing `windows/ad104-msi/benchmark.py` was used as a generic Windows cuBLAS numerical test; its name does not restrict the compute workload to AD104. Python 3.12.14, PyTorch 2.8.0+cu128 and CUDA runtime 12.8 were already installed. No packages were installed.

Command arguments: `--size 4096 --samples 5 --batch 3 --warmup 0.25`. Each mode uses explicit FP32 output and a CPU reference with exactly representable inputs.

| Mode | Median TFLOP/s | Median GEMM time | Numerical checks |
| --- | ---: | ---: | --- |
| FP32 pedantic | 36.03 | 3.8151 ms | Passed |
| FP16 with FP32 accumulation | 186.41 | 0.7373 ms | Passed |
| BF16 with FP32 accumulation | 176.14 | 0.7803 ms | Passed |

For each mode, the entire 256 x 256 result and 1,024 sampled elements of the 4096 x 4096 result matched the CPU reference exactly, with maximum absolute error zero. The large outputs were checked for non-finite values. These finite tests do not establish universal numerical correctness.

Background desktop and existing compute processes remained running. These are current-session measurements, not a matched stock/unlocked comparison. No speedup percentage, gaming performance change, extended stability result or repeat-boot persistence is claimed. A direct post-driver SS0/SS1 register read was not performed; the full-rate hardware readbacks were recorded by the USB before Windows loaded.

## Files

- `RUN0001.TXT`: preserved original USB transaction log.
- `provenance.json`: copied-log and candidate hashes.
- `windows-health.json`: boot time, driver signature, adapter state and event-log result.
- `cuda-correctness-and-throughput.json`: full numerical checks, timing samples, library hashes and telemetry.
- `gpu-before.csv`, `gpu-after.csv`, `gpu-health-sections.xml.txt`: GPU snapshots.

This verification made no changes to firmware, boot settings, driver installation, clock settings, voltage, power limit or running applications. It copied evidence into this directory and ran ordinary CUDA compute. Nothing was installed for automatic activation at future boots.
