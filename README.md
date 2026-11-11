# Winternight

A retro-3D fantasy action game on a fully deformable voxel engine.

> A warrior crosses a snow-buried forest under the moon to a feast in a castle
> whose windows glow with firelight. A necromancer takes the castle and carries
> the mage away on a dragon of bone. The warrior must break free and follow.

**Status:** concept phase. No engine code yet. The story is a working draft.

## Concept documents

- [Game concept](docs/concept/game-concept.md): story outline, characters, pillars, gameplay
- [Visual style guide](docs/concept/visual-style.md): retro rendering, palette, lighting, snow, blood, destruction
- [OpenSpec change `establish-winternight-concept`](openspec/changes/establish-winternight-concept/):
  - [proposal.md](openspec/changes/establish-winternight-concept/proposal.md): why and scope
  - [design.md](openspec/changes/establish-winternight-concept/design.md): engine architecture and technology decisions
  - [specs/](openspec/changes/establish-winternight-concept/specs/): requirements for each capability
  - [tasks.md](openspec/changes/establish-winternight-concept/tasks.md): milestone 1 plan (engine + Prologue vertical slice)

## Technology

| Area | Choice |
|------|--------|
| Language | C11 |
| Platform | SDL3 |
| Graphics | OpenGL 3.3 core / OpenGL ES 3.0 / WebGL2 (one shared shader subset) |
| Targets | Windows, Linux, Web (Emscripten, touch controls on mobile), WebXR |
| Build | CMake presets |
| Tests | Unity unit tests (≥ 95% core line coverage, gcovr), headless deterministic game tests with golden images, Playwright browser tests (touch emulation, WebXR emulation via IWER) |
| Specs | [OpenSpec](https://github.com/Fission-AI/OpenSpec) (`openspec/`) |

## Working with OpenSpec

```bash
npx @fission-ai/openspec list
```

```bash
npx @fission-ai/openspec show establish-winternight-concept
```

```bash
npx @fission-ai/openspec validate --strict --all
```

Claude Code users can also use the `/opsx:*` commands in `.claude/commands/opsx/`.
