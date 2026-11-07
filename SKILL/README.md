# ACV agents and skills

This folder collects project-specific agent profiles and installable skills for work on the Armored Core V ReXGlue port.

## Agent profiles

These files describe roles and point to the canonical project rules. They are instruction profiles, not separate running agents.

- [ACV project agent](agents/acv-project-agent.md) follows the repository-wide instructions in `kb:AGENTS` (`python3 tools/acv-context/kb.py get AGENTS`).
- [Runtime and reverse-engineering agent](agents/runtime-agent.md) routes ReXGlue, guest PPC, and Vulkan work to the matching skill and evidence source.
- [Documentation agent](agents/documentation-agent.md) follows the documentation rules in `kb:AGENTS` and the [documentation workflow](skills/acv-docs/SKILL.md).

## Skills

- [ACV ReXGlue workflow](skills/acv-rexglue/SKILL.md) covers the game/runtime boundary, codegen, and SDK evidence.
- [ACV Vulkan workflow](skills/acv-vulkan/SKILL.md) covers GPU changes and synchronization review.
- [ACV knowledge base](skills/acv-kb/SKILL.md) reads, searches, and changes project documents stored in SQLite (`tools/acv-context/knowledge.sqlite3`).
- [ACV documentation workflow](skills/acv-docs/SKILL.md) covers project status, evidence, and document checks.
- [Used skill sources](skills/README.md) lists the external and project skills that informed these workflows, with their local dependencies.

The project instructions (`kb:AGENTS`) remain authoritative. Addresses such as `kb:PLAN_GUI#anchor` are read with `python3 tools/acv-context/kb.py get SLUG#ANCHOR`. Skills here are compact ACV-specific entry points; they do not replace ReXGlue, Vulkan, or Xbox 360 reference material.

## Профили агентов и навыки ACV

Папка содержит инструкции ролей и навыки для работы над портом Armored Core V на ReXGlue.

- [Агент проекта ACV](agents/acv-project-agent.md) следует главным правилам в `kb:AGENTS`.
- [Агент runtime и reverse engineering](agents/runtime-agent.md) выбирает подходящий workflow и источник доказательств.
- [Агент документации](agents/documentation-agent.md) следует правилам документации в `kb:AGENTS`.
- Навыки: [ReXGlue](skills/acv-rexglue/SKILL.md), [Vulkan](skills/acv-vulkan/SKILL.md), [база знаний](skills/acv-kb/SKILL.md), [документация](skills/acv-docs/SKILL.md).
- [Список исходных навыков](skills/README.md) указывает использованные локальные навыки и необходимые внешние справочники.

Правила в корне репозитория остаются главными. Эти навыки дают проектные точки входа и не заменяют документацию SDK, Vulkan или Xbox 360.
