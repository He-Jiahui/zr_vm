---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E4b structured evaluate failure transport
status: completed
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
implementation_files:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_evaluation_effect.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_evaluation_effect_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_formal_evaluation.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol_evaluate.h
tests:
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_evaluate_failure_transport_cases.h
  - tests/debug/test_debug_agent_protocol.c
doc_type: milestone-detail
---

# E4b Structured Evaluate Failure Transport

## Contract

`ZrDebug_EvaluateWithCapabilitiesDetailed` adds an optional
`ZrDebugEvaluateFailure` output without changing the existing evaluate API.
The failure is scoped to the active paused `state_id` and carries a stable
kind/code, bounded display fields, and exact diagnostic descriptor/range when
the canonical producer supplied them.

Formal parsing captures the parser's structured-error callback and deep-copies
the diagnostic before parser teardown. Parser failures therefore preserve the
exact code, descriptor, range, cause and suggestion. Compiler structured errors
use the same projection. Capability denial, missing canonical facts and formal
execution failures receive explicit Debug-owned kinds/codes from their
structured branch; they are never classified by inspecting rendered error text.
Legacy compatibility failures have their own kind and cannot claim parser
diagnostic identity.

`zrdbg/1` serializes every evaluate failure through JSON-RPC `error.data`.
The data object includes `kind`, `stateId`, `code`, `message` and range offsets,
with descriptor/cause/suggestion when available. The evaluate handler consumes
the detailed API directly, so no protocol consumer reconstructs the error from
expression spelling, member name, accessor naming or AST pairing.

The E4 audit also reconfirmed that variable child handles already compare their
recorded `state_id` with the current stop generation before resolution. E4b
adds failure transport rather than weakening that stale-handle boundary.

## Status And Output Record

| Time | Status | Completed output |
| --- | --- | --- |
| 2026-08-02 11:26 +08:00 | `completed` | DAP evaluate failures now expose canonical structured diagnostics and stable Debug failure facts through a state-scoped JSON-RPC transport. |

## Validation

- RED: the new regression did not compile because the public failure type,
  detailed evaluate API and protocol failure serializer did not exist.
- GCC 11.4.0 and Clang 14.0.0 each built and ran the isolated static
  `zr_vm_debug_expression_diagnostics_test`: `56 Tests / 0 Failures / 0
  Ignored`, process exit 0.
- MSVC 19.44 / Visual Studio 17.14.36 rebuilt the static target and ran the
  executable as the final command: `56 Tests / 0 Failures / 0 Ignored`,
  process exit 0.
- A focused real TCP `zrdbg/1` evaluate lifecycle, temporarily isolated to the
  existing capability-context test only, passed on GCC and Clang (`1 Test / 0
  Failures / 0 Ignored`, process exit 0). It asserts `error.data` for both a
  canonical capability denial and parser `missing_right_operand` diagnostic.
