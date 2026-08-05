---
related_code:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
tests:
  - tests/debug/test_debug_agent_protocol.c
  - tests/acceptance/2026-08-05-lsp-04-e4c-children-handles.md
doc_type: module-detail
---

# Debug Variable Handle Generations

## Purpose

`zrdbg/1` exposes expandable values through numeric `variablesReference` handles. These
handles are capabilities for one paused debugger state, not reusable scope identifiers or
raw runtime pointers. A client that resumes and reaches another stop must not be able to
use a handle issued before the resume to inspect current data.

## Behavior Model

`ZrDebug_AgentStart` seeds the per-agent allocator at
`ZR_DEBUG_VARIABLE_HANDLE_BASE`. Each expandable preview, including formal `evaluate`
results, receives the next numeric handle while recording the current `stopStateId` in its
private snapshot entry.

Clearing a paused state releases its snapshot array but does not reset the allocator. A
second stop therefore receives a different number even when it creates the same shape of
expandable value. Once the unsigned allocator wraps below the reserved handle range,
allocation returns unavailable instead of reusing an earlier handle.

`ZrDebug_ReadVariables` reserves all numbers at or above
`ZR_DEBUG_VARIABLE_HANDLE_BASE` for registered handles. A missing value in that range
fails closed and cannot fall through to arithmetic `frameId * 10 + scopeKind` decoding.
This preserves the resume-invalidation contract even for a deep call stack whose scope
number could otherwise collide with an old handle.

## Protocol Boundary

The `variables` request continues to use the existing `scopeId` field. Low values are
current frame scopes. High values are only valid if they resolve to an active handle for the
current paused state. Missing, cleared, stale, and exhausted values return the established
`-32002` `failed to read variables` response; no display text, member name, AST, or source
position is used as a fallback identity.

## Test Coverage

`test_debug_agent_invalidates_children_handle_after_resume` starts suspended, evaluates an
array, resumes to a breakpoint, evaluates another array, and then requests the first handle.
It requires different handle values and the structured `-32002` rejection after the second
stop. The protocol suite was run under GCC, Clang, and MSVC.

## Plan Sources

This implements LSP 04 E4 result transport's requirement that children handles are invalid
after resume. It is intentionally limited to paused-state handle lifecycle; expression
typing, canonical result transport, and REPL generation rules remain in their respective
E4a/E4b/E5 records.
