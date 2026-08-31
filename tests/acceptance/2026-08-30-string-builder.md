---
related_code:
  - zr_vm_core/include/zr_vm_core/string_builder.h
  - zr_vm_core/include/zr_vm_core.h
  - zr_vm_core/src/zr_vm_core/string_builder.c
  - tests/core/test_string_builder.c
  - tests/CMakeLists.txt
  - tests/benchmarks/cases/string_build/zr/src/main.zr
  - tests/benchmarks/cases/string_build/c/benchmark_case.c
  - tests/benchmarks/cases/string_build/dotnet/benchmark_case.cs
  - tests/benchmarks/common/lua/benchmark_runner.lua
  - tests/benchmarks/common/qjs/benchmark_runner.js
  - tests/benchmarks/common/node/benchmark_runner.js
  - tests/benchmarks/common/python/benchmark_runner.py
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/string_builder.h
  - zr_vm_core/src/zr_vm_core/string_builder.c
plan_sources:
  - docs/plans/benchmark/optimize/03-memory-object-gc.md
  - user: 2026-08-30 string_build benchmark and Task 4 audit
tests:
  - tests/core/test_string_builder.c
doc_type: testing-guide
---

# String Builder Acceptance

## Scope

This acceptance record covers the safe native builder slice of optimization Task 4. It does not claim that the ZR `string_build` benchmark uses the builder or that the plan's 70% allocation / 20% wall-time thresholds have been met.

## Evidence

- TDD RED: `tests/core/test_string_builder.c` initially failed because `zr_vm_core/string_builder.h` did not exist.
- GREEN source validation: the builder implementation and test compile with the repository's local GCC using `-std=c11 -D_Thread_local=__thread -Wall -Wextra -Werror -fsyntax-only` and the core/test include paths.
- The builder preserves explicit byte length, embedded NUL, UTF-8 bytes,
  short-string interning, long-string hash/equality, and data copied before GC.
  Allocation exceptions use the existing runtime throw path; non-throwing
  failure paths leave builder ownership intact.
- `tests/CMakeLists.txt` registers target `zr_vm_string_builder_test` and CTest name `string_builder`.
- MSVC Debug focused binary passes 4/4. The long-string case checks hash and
  equality; only short strings are interned by the existing runtime contract.

## Limits

The current benchmark runners use their existing language-native concatenation. The ZR runtime exposes no builder binding, and no API was added to the native registry in this slice. A runtime benchmark result therefore cannot be reported until a public binding and a rebuilt benchmark target are available. WSL and full CMake acceptance remain environment-level follow-up work when the shared build is available.
