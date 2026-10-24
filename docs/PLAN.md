# ACV — Current Status

Current as of 2026-10-24. The detailed implementation queue is in
[PLAN_GUI.MD](../PLAN_GUI.MD). This file keeps the short facts about the native Linux
line and the user's decisions.

[Русская версия](PLAN.RU.MD)

## Goal and boundaries

The goal is to run Armored Core V natively on Linux through ReXGlue. The original ISO is
read-only; executables and temporary data live in `tmp/`, logs and evidence in `logs/`.
Do not mix ReXGlue with the independent XenonRecomp branch, and do not treat the generated
PPC C++ as a recovered source.

The regular diagnostic run uses [acv.config](../tools/native-rexglue/acv.config) and the
ARTEM/ARTEM profile. The diagnostic ELF is
`tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe`.
Settings, profiles and the launcher implementation follow [PLAN_GUI.MD](../PLAN_GUI.MD).

## Confirmed

- The user confirmed the fix for black fur textures. The established cause of the shader
  fetch problem is a wrong `exp_adjust` dword; the accepted GPU plugin `90d8ef1a…` is kept.
  [Technical evidence](NATIVE_REXGLUE_LINUX.md#texture-rendering-fix-confirmed-by-user-2026-10-24).
- Resolve/readback and resource lifetime received synchronization, range checks, error
  handling and deferred release of staging resources. Tests and the build passed. By the
  user's decision, the remaining DLC/readback signals are accepted as known: missing sound
  payloads, the optional `system.properties`, the absence of gameplay readback in the
  startup smoke test, and non-forced eviction under load.
- Four required `dlc_image:` containers are extracted from the retail `dvdbnd` and mounted
  read-only. Five remaining sound payloads are missing from the available ISO; accepting the
  requests as known does not mean more data is available.

## Documents and archive

- [PLAN_GUI.MD](../PLAN_GUI.MD) — the current detailed implementation queue and its
  completion criteria.
- [PROGRESS](PROGRESS.md) — completed stages and limitations.
- [STATE](X360_PROJECT_STATE.md) — short pointer for resuming work.
- [NATIVE_REXGLUE_LINUX](NATIVE_REXGLUE_LINUX.md) — runtime evidence.
- [AGENTS](../AGENTS.md) — working rules.

The previous version of this plan is preserved in
[`docs/fine/PLAN_worktree_before_compression_20261024.md`](fine/PLAN_worktree_before_compression_20261024.md);
the checksum and the original Git blob are recorded in
[`PLAN_worktree_before_compression_20261024.json`](fine/PLAN_worktree_before_compression_20261024.json).
An earlier plan merge is described in [`planning-merge.json`](fine/planning-merge.json).
