# 2026-06-24 AOT 12-S8B Full-AOT Dynamic Value Access Closure

## Scope

12-S8B extends the full-AOT closure guard from dynamic calls to dynamic value-access deopt:

- Default hybrid AOT C output can still emit `zr_aot_value_dynamic_deopt_bridge`.
- `requireFullAot` rejects dynamic member/index value access that would need a deopt bridge.
- The writer returns failure and removes the partial generated C file.

This does not cover dynamic iterator deopt bridges, reflection, dynamic generic instantiation, or a complete
mark-and-sweep closure report.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  generalizes the full-AOT pre-emission scan from dynamic-call closure to dynamic-deopt closure.
- The guard treats SemIR `META_GET`, `META_SET`, `DYN_INDEX_GET`, and `DYN_INDEX_SET` as requiring dynamic
  value-access deopt in full-AOT mode.
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
  keeps the default hybrid value-access deopt-bridge smoke and adds a full-AOT rejection fixture for member and index
  access.

## RED / GREEN

RED:

- The new full-AOT dynamic value-access fixture failed because `ZrParser_Writer_WriteAotCFileWithOptions()` returned
  success for a member-access deopt boundary.

GREEN:

- Default hybrid dynamic member/index generation still emits and compiles the value-access deopt bridges.
- Full-AOT dynamic member/index generation returns `ZR_FALSE`.
- The rejected full-AOT paths leave no generated C artifact behind.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'`
  - 4 tests, 0 failures

## Acceptance Decision

Accepted as 12-S8B only. Dynamic member/index value-access deopt fallback is no longer allowed in full-AOT mode, but
broader full-AOT closure diagnostics remain open.
