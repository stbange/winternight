# Tasks

These tasks deliver and verify the concept documents only. The future showcase
stages in the game concept are a roadmap, not implemented features in this
change. Runtime specs are deliberately skipped because behavior is unchanged.

## 1. Ground the direction

- [x] 1.1 Inspect the foundation change, source, tests and CI; verify the engine guide distinguishes current cell / vertex formats, retained chunks, debris recycling and replay hash limits from proposed follow-ups.
- [x] 1.2 Expand `docs/concept/game-concept.md`; verify the requested feast / abduction / escape / rescue premise is preserved and narrative names, scale and later scenes remain provisional.

## 2. Make the visuals reviewable

- [x] 2.1 Expand `docs/concept/visual-style.md`; verify composition, readable darkness, materials, palette and mobile / XR presentation have concrete review criteria.
- [x] 2.2 Add the four-panel `boards/winter-night.svg`; verify it parses, renders with legible captions, uses local-only resources and is labelled as an illustration rather than an engine capture.

## 3. Explain the engine and its evidence

- [x] 3.1 Add `docs/concept/voxel-engine.md`; verify representation alternatives, deformation, snow, blood, streaming, platform constraints, arithmetic and unmeasured budgets are explained.
- [x] 3.2 Add `docs/concept/validation-plan.md`; verify every major claim maps to an existing check or an explicitly proposed scenario, and >=95% core line coverage is distinguished from platform integration testing.
- [x] 3.3 Link the package from `README.md`; verify the new entries point to the intended documents and original implementation history remains available.

## 4. Validate the package

- [x] 4.1 Build the unchanged portable core and run CTest plus gcovr; verify all 11 tests pass and line coverage is at least 95% (observed 99.3%; branch coverage 90.5%).
- [x] 4.2 Validate all OpenSpec changes with `validate --strict --all`, check local document links / SVG references and `git diff --check`, and verify the diff contains only the authorized documentation package.

## Workflow follow-up

- Review the concept with the user. No runtime apply step is needed for these documents.
- Any future engine or scene work needs its own scoped spec delta, core unit tests maintaining >=95% coverage, and game / browser tests for new visible behavior.
- Keep the original foundation change open while its existing hardware / performance tasks remain unresolved.
