# ACV agents and skills

This folder collects project-specific agent profiles and installable skills for work on the Armored Core V ReXGlue port.

## Agent profiles

These files describe roles and point to the canonical project rules. They are instruction profiles, not separate running agents.

- [ACV project agent](agents/acv-project-agent.md) follows the repository-wide instructions in [AGENTS.md](../AGENTS.md).
- [Runtime and reverse-engineering agent](agents/runtime-agent.md) routes ReXGlue, guest PPC, and Vulkan work to the matching skill and evidence source.
- [Documentation agent](agents/documentation-agent.md) follows [docs/AGENTS.md](../docs/AGENTS.md) and the document ownership map.

## Skills

- [ACV ReXGlue workflow](skills/acv-rexglue/SKILL.md) covers the game/runtime boundary, codegen, and SDK evidence.
- [ACV Vulkan workflow](skills/acv-vulkan/SKILL.md) covers GPU changes and synchronization review.
- [ACV documentation workflow](skills/acv-docs/SKILL.md) covers project status, evidence, and document checks.
- [Used skill sources](skills/README.md) lists the external and project skills that informed these workflows, with their local dependencies.

The project instructions in the repository root remain authoritative. Skills here are compact ACV-specific entry points; they do not replace ReXGlue, Vulkan, or Xbox 360 reference material.

## Профили агентов и навыки ACV

Папка содержит инструкции ролей и навыки для работы над портом Armored Core V на ReXGlue.

- [Агент проекта ACV](agents/acv-project-agent.md) следует главным правилам в [AGENTS.md](../AGENTS.md).
- [Агент runtime и reverse engineering](agents/runtime-agent.md) выбирает подходящий workflow и источник доказательств.
- [Агент документации](agents/documentation-agent.md) следует [docs/AGENTS.md](../docs/AGENTS.md) и карте документов.
- Навыки: [ReXGlue](skills/acv-rexglue/SKILL.md), [Vulkan](skills/acv-vulkan/SKILL.md), [документация](skills/acv-docs/SKILL.md).
- [Список исходных навыков](skills/README.md) указывает использованные локальные навыки и необходимые внешние справочники.

Правила в корне репозитория остаются главными. Эти навыки дают проектные точки входа и не заменяют документацию SDK, Vulkan или Xbox 360.
