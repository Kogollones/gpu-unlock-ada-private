# Implementation and staging receipt — October 30, 2026

An experimental MSI RTX 4080 SUPER / X870E profile has been implemented and
staged on USB E: (disk 2). It has not executed on the GPU. Hardware behavior,
Windows recovery, numerical correctness and performance remain unvalidated.

The profile uses the owner's reviewed diagnostic capture, exact GPU/ROM/code
identities, conservative display-ownership checks and the retained guarded
transaction. Only U at the preboot menu permits one attempt. S, Esc and timeout
select stock Windows after initial read-only checks. No automatic unlock was
installed on the internal drive; firmware settings and keys were not changed.

Offline evidence: 162 C scenarios and 14 Python tests passed. The EFI passed
PE validation, reproduced byte-for-byte and was signed with the existing local
certificate. Signature, unchanged executable bytes, tamper rejection and wrong
signer rejection were checked. All hardware/service effects were simulated;
these results do not guarantee hardware safety or firmware acceptance.

Tested candidate: `candidate-20261030-195526-925`.

- Unsigned SHA-256: `a5a76abd8dd2154405d8f9e4cba3de51e9540e0f06a94fb32c5417a671d17bae`
- Signed SHA-256: `cb33d814ad158451f207c3ce846b8a584701adf6f635c547700b46adcfc9bea2`
- Prior diagnostic SHA-256: `bab2b09b6e342520ddcc6e2104161d19e6e73a082f802ed46e3f5f4b3ec1d239`

The signed image is at `E:/EFI/BOOT/BOOTX64.EFI`, with an identical alternate at
`E:/EFI/GPUUnlock/RTX4080S-EXPERIMENTAL.EFI`. All five pinned assets and their
license are at `E:/RTX4080S-ASSETS`; readback hashes passed. The old diagnostic
is preserved at `E:/EFI/GPUUnlock/PREVIOUS-BOOTX64.EFI` and locally under
`candidate-20261030-195526-925/usb-backup-20261030-200925/BOOTX64.EFI`.
Both original diagnostic captures remain unchanged.

See the candidate's `manifest.json`, `signed/signing-manifest.json` and
`usb-staging-receipt.json` for evidence. The manifest records final source state;
the last changes after compilation were documentation and packaging/verification
scripts, not compiled C/header inputs. The original AD104 hardware profile was
retained but its private-fixture tests were not independently rerun here.

Read [the manual procedure](../../windows/rtx4080s-msi/EXPERIMENTAL.md)
before a first attempt. No reboot or GPU transaction was performed in this work.
