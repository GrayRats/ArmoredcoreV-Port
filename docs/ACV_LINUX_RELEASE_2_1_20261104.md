# ACV Linux release 2.1 — 2026-11-04

## Package

[Launch script](../ACV-Linux-release-2.1/start.sh) is the separate v2.1
package. It contains the seven Vulkan optimization paths and the persistent
pipeline-cache integration for the lazy render-target transfer pipelines.
Package metadata marks this build as a candidate (`accepted_baseline=false`).
The accepted v2.0 package and runtime baseline were left unchanged.

## Build and package checks

- Rebuilt243 native SDK translation units with stale PCH/objects disabled; linked
  `librexruntime.so` and `librexgpu-xenos.so`. Build record:
  [build.json](tmp/acv-vulkan-release-2.1-build/build.json).
- Package verifier: 189 payload files, TRG frontend, no Qt/ICU, all shared-library
  dependencies resolve. `start.sh` passes `sh -n`. The dependency manifest lists
  16 external system libraries.
- Runtime artifact SHA-256:
  - `librexruntime.so`: `0ccf6d74a485d192093fc2256d6b8b25ac0939c31728e51a6743160feefd966d`
  - `librexgpu-xenos.so`: `025121c7d6cc6d5de49874c1916775eb9b002a851c26d2d54e0d80497ce4a153`
  - Reused unchanged `acv_linux_probe`: `2fc173a9e81a60c5130da0501e47a7d4ce3df722780e348ee55f85d36788652b`
  - Reused unchanged `libamd_fidelityfx_vk.so`: `0933159a2f30c101a53d48509a7606a5dbdecaae53e5b6218c0a7ca2e4474069`
- Package hashes: `manifest.json` `ea8e41e043a40cd32ea689a8e12128e06ae2c49243315f6f16188cafcbcd6431`;
  `acv-package.json` `d43a5af24d69e22af350722f8890fdeee9e3872857ddc0aef6ac96583e04a1f3`;
  `start.sh` `d50dcd1aed097aa1eb631ef1c208ca01b3c9c09407d69f419546f690309b9c7`.
- README and release-note payload hashes are
  `043023f2a33383716a3127d4c15c6532e71a83e0cbcbbe07007d23dff69c8ee3` and
  `cddeeb7a36e92ffd8bc19af30a3599570edef9e28bd06f51f6bddfca7c5eb1cc`.
- The canonical `acv.config` hash matches the workspace config. The package
  reuses the same XEX hash as v2.0; no user profile or save storage was copied or
  modified. v2.0 preservation hashes are recorded in
  [release2-preservation.json](logs/release21-20261104/release2-preservation.json).
- The package-level verifier reports success:
  [verification output](logs/release21-20261104/checkpoint.json).

## Acceptance limits

No v2.1 game/runtime launch, GPU image check, save/load test, gameplay test or
performance comparison was run. The compile and package checks do not establish
that gameplay is correct, that images match, or that stutter/FPS improved. Treat
v2.1 as a compiled package candidate until those checks are completed.

The package README and release notes carry the same status. User settings,
release2.0 and `RUNTIME_BASELINE.json` were not promoted or replaced.


## Rebuild 2026-11-06

Пакет пересобран из дерева SDK с Windows-портированием (243 TU, оба `.so` слинкованы).
Прежние проверенные библиотеки сохранены в `tmp/acv-backups/release-2.1-before-rebuild-20261106/`.

- `librexruntime.so`: `8d92c24f94c1186aad47c43eb0d1d377d406b5a2a62b0208e316d31a0a191e02`
  (было `0ccf6d74a485d192093fc2256d6b8b25ac0939c31728e51a6743160feefd966d`).
- `librexgpu-xenos.so`: `d72210ba0fbfff7cd8d55fcea1316c4f7ac934e3ae9c46c6ba9c07e590d3252c`
  (было `025121c7d6cc6d5de49874c1916775eb9b002a851c26d2d54e0d80497ce4a153`).
- Host-тесты ASan/UBSan: 31 C++ и 2 Python прошли; GPU-тесты на GTX 1660 SUPER с
  `detect_leaks=0` (утечки только внутри драйвера NVIDIA). `sdk_*` тесты не запускались.
- `verify_package.py`: 189 файлов, libraries resolve, без Qt/ICU; `start.sh` проходит `sh -n`.
- Не проверено: запуск игры на пересобранных библиотеках, геймплей, картинка, FPS.
