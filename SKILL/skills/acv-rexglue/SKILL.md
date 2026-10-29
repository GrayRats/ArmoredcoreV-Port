---
name: acv-rexglue
description: Use when building, diagnosing, or changing the Armored Core V ReXGlue runtime, SDK integration, configuration, or Xbox 360 guest-code boundary.
---

# ACV ReXGlue workflow

Read the repository [agent rules](../../../AGENTS.md), [tool router](../../../TOOLS.MD), and [current state](../../../docs/X360_PROJECT_STATE.md) before runtime, codegen, or binary work. Consult the [360tools skill](../../../tools/360tools-skill/SKILL.md) and the `xbox360-decomp` skill listed in [the source inventory](../README.md) when their domain applies.

Keep the ACV ReXGlue project separate from XenonRecomp. Verify settings and APIs in the exact SDK checkout and cite source file/line. Treat `generated/` as output; put durable changes in game configuration, project shims, or a reproducible SDK patch. For guest PPC behavior, use original bytes/Ghidra evidence and record guest VA, scope, and confidence.

Run builds and probes through `tools/decomp-logging/cli.mjs`. Preserve the canonical user config and saves. Do not infer gameplay acceptance from a successful build or startup; record the actual scene and what was observed.
