# Skill source inventory

The following skills were consulted during ACV work. Their full upstream copies are not vendored here unless the project already maintains them; this avoids stale or incomplete duplicates.

| Skill | Canonical source in this workspace | Use in ACV |
|---|---|---|
| `360tools` | [`tools/360tools-skill/SKILL.md`](../../tools/360tools-skill/SKILL.md) | ReXGlue and Xbox 360 tool selection, build boundaries, and asset handling |
| `xbox360-decomp` | `~/.agents/skills/xbox360-decomp/SKILL.md` | PPC/XEX evidence, Ghidra discipline, and static-recomp routing; requires its adjacent `resources/` |
| Vulkan implementation | `~/.codex/skills/vulkan/SKILL.md` | Vulkan ownership, results, synchronization, and renderer review |
| Vulkan local reference | `~/.agents/skills/vulkan/SKILL.md` | Lookup in the local Khronos reference checkout under that skill's `references/` |
| `skill-creator` | `~/.codex/skills/.system/skill-creator/SKILL.md` | Used to structure and validate this folder; it is authoring guidance, not a game-development dependency |

Install the environment-provided skills separately when they are unavailable. Vulkan reference repositories are intentionally not copied into this project: the local checkout is hundreds of megabytes and must match the target SDK/driver. Project-specific entry points are [ACV ReXGlue](acv-rexglue/SKILL.md), [ACV Vulkan](acv-vulkan/SKILL.md), and [ACV docs](acv-docs/SKILL.md).
