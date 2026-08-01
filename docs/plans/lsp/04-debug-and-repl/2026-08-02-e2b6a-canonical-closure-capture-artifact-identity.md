---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b6a canonical closure-capture artifact identity
status: completed
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function_closure_identity.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_closure_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
tests:
  - tests/debug/test_debug_metadata.c
  - tests/core/test_gc_concurrent_major.c
doc_type: milestone-detail
---

# E2b6a Canonical Closure-Capture Artifact Identity

## Contract

- A closure capture has a typed sidecar row with capture index, full canonical
  TypeRef, source SymbolId, canonical TypeId, and whole declaration range.
- The legacy closure-variable artifact row remains wire compatible. Patch 41
  appends the sidecar after typed local metadata and before typed exports.
- Compilation freezes identity from an exact parent binding or pre-SemIR slot.
  Nested captures can only inherit an already frozen identity. No producer or
  consumer may recover an identity by capture name, stack slot, text, or AST.
- `ZrCore_Function_GetClosureCaptureIdentity` accepts only one complete row for
  the requested legacy capture index and clears every output when unavailable.
- The sidecar TypeRef is part of GC mark, young-reference detection, and compact
  relocation. This stage does not resolve a capture against a paused frame and
  does not enable formal Debug capture evaluation.

## 状态与产出记录

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 04:38 +08:00 | `completed` | Published the patch-41 typed closure-capture artifact sidecar; exact source SymbolId/TypeId/range capture freezing for lambdas and named nested functions; fail-closed core identity query; runtime loading, GC mark, young-reference, and compact-relocation support; binary/source regressions and three-toolchain focused acceptance. |

## Validation

- RED: the new binary-roundtrip assertion first failed because the public
  closure identity query did not exist. The MSVC fresh static run then exposed
  uninitialized compiler-only bytes in the legacy runtime capture allocation;
  zero initialization of that nonserialized tail made the contract explicit.
- GCC 11.4.0 passed fresh ABI-consistent static rebuilds with
  `zr_vm_debug_metadata_test` at 8/8 and
  `zr_vm_gc_concurrent_major_test` at 10/10, both with process exit 0.
- Clang 14.0.0 passed the same two direct executables at 8/8 and 10/10 from a
  separate fresh static command graph, both with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 passed the same fresh static targets at
  8/8 and 10/10, both with process exit 0.
- E2b6b remains pending: a generation-checked paused-frame closure resolver.
  E2b6c parser closure-origin facts and E2b6d Debug consumption remain pending
  and must not use this artifact carrier as permission for a name fallback.
