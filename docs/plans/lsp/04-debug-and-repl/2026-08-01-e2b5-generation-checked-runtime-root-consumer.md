---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b5 generation-checked runtime-root consumer
status: completed
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_bindings.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation_execute.c
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_canonical_binding_cases.h
---

# E2b5 Generation-Checked Runtime-Root Consumer

## Contract

- Debug obtains the `ZR` root from the generation-checked core evaluation
  context and registers its kind/token through the parser runtime-root API.
- The parser allocates query-local SymbolId/TypeId. A runtime root has no
  function-local Place and no source declaration or definition range.
- Formal identifier execution dispatches from reference origin. Source locals
  require exact SymbolId/TypeId/PlaceId equality; runtime roots require exact
  structured kind/token resolution; closure capture remains fail closed.
- A source binding shadows the runtime-root surface spelling. No consumer may
  recover a missing or stale root from `zr` text, AST shape, a raw global
  pointer, or a synthetic local Place.

##状态与产出记录

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-01 07:35 +08:00 | `completed` | Completed generation-checked `ZR` runtime-root registration and formal execution; source `zr` shadowing; exact SymbolId/TypeId/PlaceId source dispatch; empty Place/declaration/definition identity enforcement for runtime roots; token and definition-range drift rejection; closure-capture fail-closed behavior; module documentation; and fixed-snapshot MSVC/GCC/Clang runner acceptance at 52/52. |

## Validation

- RED: the MSVC support-neutral snapshot ran 51 tests with 2 failures. One was
  the deliberately excluded Task4 property bootstrap; the new runtime-root
  case failed at its first canonical reference-fact assertion.
- Focused GREEN: the same snapshot passes the runtime-root case, including
  query-local identity, empty source/Place identity, core-token equality,
  definition-range and token-drift rejection, and `zr[1]` result `uint 98`.
- After adding source-shadow coverage, the snapshot runner is 52 tests with 1
  remaining deliberate Task4 property failure. The source `zr` fact retains
  its frame Place and clears runtime-root kind/token. This runner is therefore
  not recorded as a full target pass.
- GCC and Clang compile the exact changed binding, executor, and diagnostics
  test translation units with process exit 0.
- Stable Task4 property support is present in the clean baseline through
  ancestor commit `3d67352`. The final snapshot was created from
  `c09091bb295760dd39e5ce7917a622fe2b468a96` plus the 14 exact Debug code/test
  overlays used by this milestone; all 14 overlay SHA-256 hashes matched after
  copying the snapshot to WSL ext4.
- The subsequent Task4 completion commit `3c4c172` changes only the public
  bootstrap's null-argument result; E2b5 always supplies non-null compiler and
  function inputs, so its validated path is unchanged. Task4 independently
  passes its 11-case runner on GCC, Clang, and MSVC with process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 configured and built the fresh static
  target, then passed `zr_vm_debug_expression_diagnostics_test` with 52 tests,
  zero failures, zero ignored, and process exit 0.
- GCC 11.4.0 configured and built the fresh static target on the same fixed
  snapshot, then passed the same 52 tests with zero failures, zero ignored,
  and process exit 0.
- Clang 14.0.0 configured and built a separate fresh static target on the same
  fixed snapshot, then passed the same 52 tests with zero failures, zero
  ignored, and process exit 0.
- Closure-capture identity remains deliberately unavailable and is owned by a
  later independent E2 slice; this milestone does not fabricate a fallback.
