# Repository Guidelines

## Project Structure & Module Organization

Drawer is a planned Windows-only, two-dimensional drawing and interaction test host. The repository currently contains documentation, not application code. Start with `README.md`, then read `docs/requirements.md`, `docs/architecture.md`, `docs/decisions.md`, and `docs/implementation-plan.md`.

Planned source areas are `src/` for the host and SDK, `cases/` for Lua demonstrations, `tests/` for automated checks, and `third_party/` for pinned dependency sources and licenses. Update this guide when those directories become concrete.

## Build, Test, and Development Commands

No build system or runnable commands exist yet. Implementation will use target-based CMake and CTest. Once available, document and verify configure, build, test, and launch commands in `README.md`. Do not describe planned commands as working commands.

Use an external editor for Lua cases. The application must provide F5 and a reload button; modifying a case must not require rebuilding the native host.

## Coding Style & Naming Conventions

Use four-space indentation, descriptive names, and self-contained C++ headers. Prefer `snake_case.cpp`, `UpperCamelCase` types and functions, `snake_case` variables, and trailing underscores for class members. Establish a Google-based clang-format configuration with the first code changes. Keep Lua case identifiers stable and independent of display names.

## Testing Guidelines

Use CTest with C++ checks and Lua assertions; avoid adding a testing framework unnecessarily. Name test files after the behavior under test, such as `document_test.cpp`. Cover transactions, document round trips, coordinate conversion, instance isolation, and failed reloads. There is no percentage coverage requirement. Record actual results and distinguish unverified behavior.

## Commit & Pull Request Guidelines

There is no existing Git history or established commit convention. Use concise, imperative subjects, for example `Add Lua case lifecycle`. Pull requests should describe behavior, reference relevant requirements or issues, list verification, and include screenshots for visible UI changes.

## Architecture & Documentation

Keep primary third-party dependencies to Dear ImGui and standard Lua. Preserve the confirmed Windows, mathematical-coordinate, multi-tab, and script-reload decisions. Update design documents alongside implementation; label provisional APIs clearly. Check for an existing `AGENTS.md` before generating a replacement and preserve it.
