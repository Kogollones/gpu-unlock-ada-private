# MSI RTX 4080 SUPER / X870E — Windows result, October 30, 2026

One owner-operated USB attempt completed the unlock transaction, retained
full-rate hardware readbacks after FLR, and returned to a working Windows
session. The signed NVIDIA driver and short CUDA numerical checks passed.
This is **one successful boot, with current throughput measured but no matched
stock baseline**. The owner's AC-cut/cold-boot procedure was not independently
observed. The first successful attempt is not an extended stability result.

## Hardware and software

| Item | Observed configuration |
|---|---|
| GPU | MSI RTX 4080 SUPER, AD103, 16 GiB |
| PCI identity | `10de:2702 / 1462:5118`, revision A1, `0000:01:00.0` |
| Board / BIOS | MSI X870E GAMING PLUS WIFI / 1.A62 |
| Other display adapter | AMD `1002:164e` integrated graphics |
| OS / NVIDIA driver | Windows 11 Pro build 26200 / 616.56 |
| VBIOS / power cap | `95.03.44.00.6a` / unchanged 320 W |
| Secure Boot | Enabled in the UEFI log and Windows query |
| CUDA environment | Python 3.12.14, PyTorch 2.8.0+cu128, CUDA runtime 12.8 |
| Background activity | Desktop and existing compute processes remained running |

## Boot transaction and Windows recovery

`RUN0001.TXT` is the unmodified USB log. Its last-write timestamp is
20:17:40 UTC; Windows reports boot at 20:17:42.500 UTC. These timestamps
corroborate the handoff. The log records:

- operator U selection and one physical attempt;
- five successful NVIDIA display-owner detach stages after the exact ownership
  preflight, with the AMD display preserved by its checks;
- the expected payload marker `0x67b00001`, required permission readbacks and
  firmware HALT;
- initial effective readout `0x410080`, `full=0`, changing to zero, `full=1`,
  after SS0 `0x88888888` / SS1 `0x8` were written;
- the same full-rate settings after one function-level reset;
- `result=0 safe=1 retained=0 started=1 quiesced=1 flrs=1 DMA_live=0` and
  `TRANSACTION OK`.

The last pre-Windows check is explicitly unlogged. Windows recovery is therefore
supported separately by the live driver, boot timestamp and successful CUDA
execution. A direct SS0/SS1 register read after NVIDIA driver initialization was
not performed.

`windows-health.json` records both display adapters as OK, error code zero,
`nvlddmkm` running, a valid Microsoft Windows Hardware Compatibility Publisher
signature, and no GPU/display/WHEA warning/error events since boot through
20:25:08 UTC. Sampled temperature was 37–40 C at light load and up to 56 C in
the short compute test; these snapshots are not continuous peak measurements.
No driver, boot, clock, voltage or power-limit changes were made by verification.

## Current cuBLAS measurements

The existing [Windows cuBLAS benchmark](../../../windows/ad104-msi/benchmark.py)
was run with `--size 4096 --samples 5 --batch 3 --warmup 0.25`. Its AD104 folder
name does not restrict this ordinary CUDA computation to that GPU model.

| Mode | Median TFLOP/s | Median ms per GEMM | Numerical checks |
|---|---:|---:|---|
| FP16 with FP32 accumulation | 186.41 | 0.7373 | Passed |
| BF16 with FP32 accumulation | 176.14 | 0.7803 | Passed |
| FP32 pedantic | 36.03 | 3.8151 | Passed |

All 65,536 elements of each 256 x 256 result and 1,024 sampled elements of
each 4096 x 4096 result matched the FP64 CPU reference exactly, with maximum
absolute error zero. The large result was also checked for non-finite values.
The inputs are exactly representable dyadic values; this finite test does not
establish correctness for every possible input. Full timings, library hashes,
output hashes and telemetry are in `cuda-correctness-and-throughput.json`.

## Comparison with the repository's RTX 4080 SUPER report

The [existing matched AD103 report](../ada2702-ada2704-matched/README.md)
contains a comparable **4096³ FP16/FP32 cuBLAS** workload, as well as a separate
register-resident instruction benchmark. The owner's linked copy is
[on GitHub](https://github.com/Kogollones/gpu-unlock-ada/blob/main/docs/evidence/ada2702-ada2704-matched/README.md).

| Measurement | Other 4080 SUPER: stock | Other 4080 SUPER: unlocked | This MSI: after USB |
|---|---:|---:|---:|
| FP16/FP32 cuBLAS, 4096³ | 98.7 TFLOP/s | 190.7 TFLOP/s | 186.41 TFLOP/s |
| FP16/FP32 register-resident instruction rate | 104.8 TFLOP/s | 209.7 TFLOP/s | Not measured |
| BF16/FP32 register-resident instruction rate | 104.8 TFLOP/s | 209.3 TFLOP/s | Not measured |

Our cuBLAS result is about **2.2% below the other card's unlocked result**.
That is consistent with the published unlocked throughput and complements the
successful USB readbacks. It is a comparison across systems, not this card's
measured percentage gain. The reference used Linux, NVIDIA 595.99.02, VBIOS
`95.03.44.00.DB`, a 250 W cap and a headless host; this test
used Windows, 616.56, VBIOS `95.03.44.00.6a`, 320 W and background processes.
Different binaries, cuBLAS versions, inputs and timing settings also prevent
treating these as one matched pair.

The reference's 1.93x cuBLAS gain and 2x instruction gain belong to its matched
stock/unlocked runs. The RTX 4090 figures in the main README belong to a larger
GPU. None is a claim that every application or game doubles in speed.

## Artifact provenance

The [private firmware bundle](../../../firmware/private-windows/rtx4080s-msi-20261030/README.md)
contains the exact signed EFI tested here and all five firmware assets.

- Candidate: `candidate-20261030-195526-925`, version `0.1.0-experimental`.
- Unsigned EFI SHA-256: `a5a76abd8dd2154405d8f9e4cba3de51e9540e0f06a94fb32c5417a671d17bae`.
- Signed EFI SHA-256: `cb33d814ad158451f207c3ce846b8a584701adf6f635c547700b46adcfc9bea2`.
- USB log SHA-256: `ee9d28e8f188b84b7192bf6220daafced6dfe254553b519c4799452426d0dece`.

The original build/signing manifests and the binary's "UNVALIDATED" banner are
preserved. Their hardware-test flags describe their pre-attempt creation time;
`hardware-result.json` records the later observation. They must not be rewritten
to imply that the offline build executed the GPU. Firmware acceptance was
observed with Secure Boot enabled on this host; the trust route is unresolved.

## Remaining validation

A same-card stock boot is needed to measure this machine's gain. Use the same
driver, benchmark bytes, cuBLAS libraries, power cap, settings and comparable
background load in both states. A clean pair should recollect both states if
those conditions cannot match this initial run. No reboot was performed by
verification. Repeated cold/warm boots, instruction probes, representative
workloads and extended stability are not established by this record.

## Files and integrity

- `RUN0001.TXT`, `provenance.json`: original log and collection hashes.
- `windows-health.json`, `gpu-before.csv`, `gpu-after.csv`,
  `gpu-health-sections.xml.txt`: Windows and GPU snapshots.
- `cuda-correctness-and-throughput.json`, `VERIFICATION.md`: original numerical
  data and the verification report written during that session.
- `candidate-manifest.json`, `signing-manifest.json`, `usb-staging-receipt.json`,
  `IMPLEMENTATION-20261030.md`: historical pre-attempt build/staging records.
- `offline-tests/`: the tested candidate's five host-test logs; 162 simulated
  C scenarios in total. `repository-validation.json` records the later offline
  rebuild and Python checks without claiming another hardware boot.
- `hardware-result.json`: structured hardware outcome and claim limits.
- `SHA256SUMS`: hashes of the archived evidence files.

Raw diagnostic captures, VBIOS dumps, captured proprietary driver code, Windows
boot-manager bytes and private signing keys are outside this evidence set.
