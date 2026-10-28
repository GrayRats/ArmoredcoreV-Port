# ACV — current plan

Updated: 2026-10-28. [Русская версия](PLAN.RU.MD).

## Current build

[ACV-Linux-release-1.0/start.sh](../../ACV-Linux-release-1.0/start.sh) contains
TRG Launcher, the statically recompiled game, game data and required libraries.
This is a native Linux build through ReXGlue, not fully recovered game source.

## Completed

- Mission fixes and parallel readback copy accepted by the user and included in the release.
- UltraWide FOV/HUD and edge-frustum correction implemented; the user confirmed the fix helps.
- Profiles, backups/import/restore, settings and launcher workflows implemented.
  Preview is read-only; refresh retains edits; graphics reset keeps launcher edits unsaved.
- Texture-cache budgets, retention and bounded archive read-ahead integrated.
- Russian window title fixed; FPS limits 30/60/120 and 36 resolution presets with group filters available.
- CAS/FSR1 integrated without effect tests; FSR2/3 removed from the interface.
- Current package, launcher workflows and startup checks pass; original config,
  saves and personal settings are preserved.

## Remaining within the resource-loading work

- Reproduce delayed loading or premature unloading in a specific mission/action.
  The accepted frustum correction fixes edge visibility; resource lifetime is a separate issue.
- Identify the remaining resource-build, decompression or lifetime bottleneck;
  apply retention/preloading changes with bounded memory, then verify repeated
  transitions and memory use. Faster loading and model/location lifetime are not yet proven.

Native file/folder dialogs are integrated; interactive desktop selection is not manually verified.
Stable 60 FPS and a full gameplay save/load round trip remain unverified; they are
outside the currently selected work. Do not test CAS/FSR effects; resolution checks
are limited to 2560×1080. Gameplay profiling starts only after 90 seconds.

## Details

[Implementation queue](../PLAN_GUI.MD) · [Launcher/package](LAUNCHER_PACKAGE.md) ·
[Streaming](RESOURCE_STREAMING_20261027.md) · [Frustum fix](ENGINE_OFFSCREEN_20261028.md).

Previous plan text is preserved with SHA-256 in [the archive](../logs/plan-compression-20261028T175039Z/manifest.json).
