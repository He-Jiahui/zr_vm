# 2026-06-24 AOT 12-S7I Runtime Fallback Warning Reason Classification

## Scope

This refines 12-S7I runtime fallback trim-warning markers by classifying the warning reason:

- `dynamic-call`
- `dynamic-value-access`
- `dynamic-iterator`
- `reflection`

The warning remains a generated C marker only. It does not yet provide source spans, suppression policy, annotation
dataflow, zrp metadata trimming, or release symbol stripping.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  adds `EZrAotRuntimeFallbackReason` and routes the full-AOT fallback scan through
  `backend_aot_c_runtime_fallback_reason_for_instruction()`.
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
  verifies SemIR dynamic-call fallback reports `reason=dynamic-call`, and dynamic member/index value-access fallback
  reports `reason=dynamic-value-access`.

## RED / GREEN

RED:

- `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` failed when the dynamic value-access hybrid smoke required
  `reason=dynamic-value-access` instead of the generic runtime fallback marker.

GREEN:

- Dynamic-call fallback emits `reason=dynamic-call`.
- Dynamic member/index value-access fallback emits `reason=dynamic-value-access`.
- Full-AOT rejection paths continue to pass.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'`
  - 4 tests, 0 failures

## Acceptance Decision

Accepted as a 12-S7I refinement only. Runtime fallback warnings now distinguish the main fallback categories, but full
trim analysis, source-span diagnostics, suppression, zrp metadata section/table/pool statistics, and release symbol
stripping remain open.
