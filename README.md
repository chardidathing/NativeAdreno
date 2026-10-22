# NativeAdreno

Native XNU Adreno driver. Owns GMU startup, the CP ring, BOs, native 4 KiB SMMU contexts, fences and cache maintenance.

Current state: GPU commands and the scanout copy run on sheng hardware (Adreno 740). Shader compilation is slow and there is no complete precompiled UI pack. The Adreno 650 of the Xperia 1 II (pdx203) is selected by chip id through `NativeAdrenoFamily.h` and has not run on hardware yet; `docs/a650.md` has the tables, their provenance and what the device repository adds.

Linked into XNU by `unixnu build kernel`. `tests/run-host.py` builds and runs the host tests with a plain clang.
