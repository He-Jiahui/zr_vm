---
related_code:
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/include/zr_vm_core.h
  - zr_vm_core/include/zr_vm_core/string_builder.h
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/string_builder.c
  - tests/core/test_string_builder.c
  - tests/benchmarks/cases/string_build/zr/src/main.zr
  - tests/benchmarks/cases/string_build/c/benchmark_case.c
  - tests/benchmarks/cases/string_build/dotnet/benchmark_case.cs
  - tests/benchmarks/cases/string_build/java/benchmark_case.java
  - tests/benchmarks/cases/string_build/rust/mod.rs
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
  - tests/acceptance/2026-08-30-string-builder.md
doc_type: module-detail
---

# String Builder

## Purpose

`SZrStringBuilder` is a native temporary buffer for assembling one runtime string without creating an immutable VM string for every append. The buffer is owned by the caller, stores explicit byte length, and is frozen through `ZrCore_String_Create` so short-string interning, long-string hashing, and GC ownership remain centralized in the existing string implementation.

## Behavior Model

- `ZrCore_StringBuilder_Init` attaches a builder to one `SZrState` and optionally allocates an initial capacity.
- `ZrCore_StringBuilder_AppendNative` copies exactly `length` bytes. A null source is accepted only for a zero-length append, so embedded NUL bytes are preserved rather than treated as terminators.
- `ZrCore_StringBuilder_AppendString` reads the current native bytes and byte length from an immutable `SZrString`, then copies them into the builder. The copy completes before the function returns, so a later GC move cannot invalidate builder contents.
- Capacity grows geometrically and includes one trailing NUL byte for diagnostic/native consumers. The logical length never includes that terminator.
- `ZrCore_StringBuilder_Freeze` creates one immutable `SZrString`, then disposes the native buffer. On a non-throwing creation failure, the builder remains available for retry and ownership is unchanged; allocation exceptions follow the runtime's normal exception path.
- `ZrCore_StringBuilder_Dispose` is idempotent for a zeroed or already-frozen builder.

## Design And Rationale

The builder deliberately does not expose mutable storage as an `SZrString` and does not add a rope representation. This keeps all VM strings immutable and preserves the existing interning contract: freezing bytes that already exist in the string table returns the canonical object. Native allocation uses `ZR_MEMORY_NATIVE_TYPE_STRING`; it is temporary side storage, not a GC object, and therefore cannot be traversed or moved by the collector.

The current public native binding registry has no builder object or append/freeze methods. Consequently, the `string_build` ZR case still exercises ordinary immutable `+` operations. Wiring a builder into ZR would require a deliberate library API and ownership/exception contract; this change does not invent an unreachable binding or claim a benchmark improvement.

## Edge Cases And Constraints

- Append accepts arbitrary bytes, including embedded NUL. UTF-8 validity remains the responsibility of callers that request code-point operations.
- Length and capacity additions are checked for `TZrSize` overflow before allocation.
- A failed capacity allocation leaves the previous buffer and length intact.
- The builder must not outlive its attached global state. Callers must dispose it before destroying that state.
- `Freeze` consumes the builder on success; callers must use the returned immutable string and not reuse the disposed builder without reinitializing it.

## Test Coverage

`tests/core/test_string_builder.c` covers binary fragments and embedded NUL, UTF-8 byte preservation, interning/hash identity, copying an immutable string before a full collection, and invalid-input behavior. CMake registers it as `string_builder` when core tests are enabled.

## Plan Sources

This module is the safe native portion of Task 4 in `docs/plans/benchmark/optimize/03-memory-object-gc.md`. The plan's benchmark acceptance thresholds remain open until allocation profiles identify the VM string cost and a callable ZR/library binding exists.

## Open Issues Or Follow-Up

The next implementation slice should add a documented library/native binding with explicit owner and exception behavior, then update the ZR `string_build` case and all comparison runners in one semantic change. Before accepting the 70% allocation and 20% wall-time targets, run the persistent benchmark protocol with memory profiling and compare checksum-equivalent outputs across all runners.
