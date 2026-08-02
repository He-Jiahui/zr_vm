---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E3a DAP evaluate capability context
status: completed
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_evaluation_effect.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.h
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/debug/test_debug_agent_protocol.c
  - tests/debug/test_debug_evaluation_effect_policy_cases.h
doc_type: milestone-detail
---

# E3a DAP Evaluate Capability Context

## Contract

The DAP `evaluate` request maps its structured `context` to the existing
formal Debug evaluation capability flags before it evaluates the expression.
It does not select a second parser, evaluator, or compatibility path.

- Missing, malformed, `hover`, and unknown contexts grant no effects.
- `watch` grants `PROPERTY_GETTER` only.
- `repl` grants `PROPERTY_GETTER`, `ALLOCATION`, `CALL`, and `NATIVE_CALL`.
- No context grants `MUTATION` or `OWNER_MUTATION`.

`zr_debug_protocol_make_evaluate_result` enters the requested paused thread,
calls `ZrDebug_EvaluateWithCapabilities`, and preserves the existing structured
result shape and failure transport. The formal evaluator still requires
canonical facts and rejects an expression when its classified effects exceed
the selected capability set. There is no fallback based on context spelling,
member name, AST shape, or expression text.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 10:02 +08:00 | `completed` | DAP `evaluate.context` now selects the formal capability set: hover and watch fail closed for allocation, while repl explicitly authorizes the canonical allocation/call subset. |

## Validation

- RED: the new DAP integration case sent an array literal with `context: repl`.
  The old protocol ignored context and routed through the zero-capability
  compatibility call, so the array allocation was rejected.
- The integration test now sends the same expression in `hover`, `watch`, and
  `repl`. Hover and watch receive the existing explicit-capability error;
  repl returns an expandable array with two indexed children and the paused
  target resumes with its original result.
- MSVC 19.44 / Visual Studio 17.14.36 rebuilt the static
  `zr_vm_debug_agent_protocol_test` target and ran the runner as the final
  command with process exit 0. The runner contains six tests and no failures.
- GCC 11.4.0 and Clang 14.0.0 each built a fixed native WSL snapshot of
  `4405c28` plus the four exact E3a code/test overlays. All four overlay
  SHA-256 values matched the working tree before configuration. Each static
  `zr_vm_debug_agent_protocol_test` runner passed `6 Tests / 0 Failures /
  0 Ignored` with process exit 0.
- On the same GCC and Clang snapshots,
  `zr_vm_debug_expression_diagnostics_test` passed `52 Tests / 0 Failures /
  0 Ignored` with process exit 0, including the existing canonical effect
  classification and explicit-capability cases.
