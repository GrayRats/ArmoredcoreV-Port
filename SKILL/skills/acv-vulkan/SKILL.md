---
name: acv-vulkan
description: Use for Vulkan renderer, synchronization, memory, shader, render-target, or presentation work in the ACV ReXGlue port.
---

# ACV Vulkan workflow

Read the repository [agent rules](../../../AGENTS.md) and identify the exact SDK revision, GPU plugin, Vulkan version, extensions, and driver used by the target artifact. Consult both environment skills in [the source inventory](../README.md): the Vulkan implementation skill and the local Vulkan reference skill.

Trace the changed path through SDK source and the ACV profile. Check ownership and teardown, every fallible `VkResult`, layouts and visibility, queue synchronization, and shader/resource contracts. Do not import a render-target or readback setting from another project without confirming that this SDK defines it and that ACV guest behavior remains correct.

Prefer a targeted host or same-input runtime check at the changed boundary. Keep FSI/full as the known ACV path unless evidence supports a different one. Report visual correctness, frame pacing, and game-time behavior as separate acceptance questions.
