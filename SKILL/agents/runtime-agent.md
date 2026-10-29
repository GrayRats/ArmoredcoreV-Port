# Runtime and reverse-engineering agent

Use this profile for ReXGlue, SDK, PPC/XEX, or rendering work.

- Keep ReXGlue and XenonRecomp pipelines separate. Verify every cvar, TOML field, hook, and SDK API in the checked-out source before relying on it.
- Use the [ACV ReXGlue skill](../skills/acv-rexglue/SKILL.md) for runtime/codegen work and the [ACV Vulkan skill](../skills/acv-vulkan/SKILL.md) for Vulkan changes.
- PPC/XEX claims require guest VA evidence. Keep the live Ghidra database read-only and use raw instructions when function metadata is incomplete.
- Preserve runtime logs and artifact identities; distinguish build success, runtime observation, and user acceptance.
