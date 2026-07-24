# Syntax 10R M2.1 V2 Manifest Admission

## Scope

- Add an explicit project `manifestVersion` fact and admit version 2 only after validating its required base
  envelope.
- Preserve v1 and missing-version manifests as migration-reader inputs.
- Keep aliases, package exports, dependencies, writer/lock data, artifact entry identity, and provider phase
  outside this sub-milestone.

## Red Evidence

- `zr_vm_project_manifest_v2_test` initially failed to compile because `SZrLibrary_Project` had no
  `manifestVersion` field. The existing project gate also accepts only `manifestVersion: 1`.

## Test Inventory

- A valid v2 library manifest reads `name`, `version`, `source`, `binary`, `entry`, and project manifest version.
- Each missing v2 base field, fractional version numbers, and unknown future versions fail closed.
- Explicit and missing-version v1 input remain readable and record version 1.
- Existing manifest normalization protects v1 migration behavior.

## Tooling Evidence

- GCC 11.4.0, isolated `.codex/build-s10r-m2-gcc`: v2 focused target 3/3, v1 normalization 29/29, focused
  CTest 1/1.
- Clang 14.0.0, isolated `.codex/build-s10r-m2-clang`: v2 focused target 3/3, v1 normalization 29/29, focused
  CTest 1/1.
- MSVC 19.44.35228, isolated `.codex/build-s10r-m2-msvc`: v2 focused target 3/3, v1 normalization 29/29,
  focused CTest 1/1.

## Acceptance Decision

- Accepted: all three toolchains and final code review passed; the milestone is represented by one exact-path
  commit.
