---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b3 generation-checked runtime root
status: completed
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/debug.c
tests:
  - tests/debug/test_debug_introspection.c
---

# E2b3 Generation-Checked Runtime Root

## Contract

- Runtime roots are selected by `EZrDebugRuntimeRootKind`; the first published
  role is `ZR_DEBUG_RUNTIME_ROOT_ZR`.
- Acquisition returns `SZrDebugRuntimeRootBinding`, which contains the
  structured role and an opaque, nonzero token.
- Acquisition and resolution both revalidate the paused activation, frame
  generation, function, and program counter through the existing evaluation
  context validator.
- Resolution requires the exact token for the same context. A changed token or
  retired/reused frame fails with `STALE_FRAME`.
- The runtime root remains unavailable when the structured root is absent. No
  raw pointer identity, source spelling, AST, synthetic local Place, or name
  lookup may replace the missing contract.

This lower-layer slice publishes only the generation-checked core carrier.
Parser reference-origin facts and the formal Debug consumer remain later E2b/E3
work and must consume this API without inspecting the token.

##状态与产出记录

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-01 03:28 +08:00 | `completed` | Added structured runtime-root kind/binding and generation-checked acquisition/resolution APIs; extended Debug introspection coverage for successful resolution, modified-token rejection, and stale-frame rejection; updated the core Debug module contract and recorded the three-toolchain focused evidence. |

## Validation

- RED: the MSVC focused target failed to compile because the runtime-root type,
  kind, and APIs did not exist.
- GREEN: MSVC `zr_vm_debug_introspection_test` passed 2/2 with process exit 0.
- GREEN: GCC and Clang compiled the exact `debug.c` and introspection-test
  objects with process exit 0.
- The final E3 multi-toolchain acceptance matrix remains pending and is not
  implied by this lower-layer completion record.
