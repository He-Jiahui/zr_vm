# 2026-06-24 AOT 12-S8A Full-AOT Dynamic Call Closure

## Scope

12-S8A adds the first full-AOT closure guard for dynamic call deopt:

- Default hybrid AOT C output can still emit `ZrLibrary_AotRuntime_CallDynamicDeoptBridge`.
- `requireFullAot` rejects a dynamic call site when no static callable target can be proven.
- The writer returns failure and removes the partial generated C file.

This does not cover dynamic member/index/iterator deopt bridges, reflection, dynamic generic instantiation, or a complete
mark-and-sweep closure report.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  scans the filtered function table and ExecIR before C body emission when `requireFullAot` is set.
- The guard treats SemIR `DYN_CALL` / `DYN_TAIL_CALL`, explicit `DYN_CALL` / `DYN_TAIL_CALL`, and `SUPER_DYN_*`
  call opcodes as requiring deopt unless callable provenance resolves a static callee.
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
  keeps the default hybrid deopt-bridge smoke and adds a full-AOT rejection fixture.

## RED / GREEN

RED:

- The new full-AOT dynamic-call fixture failed because `ZrParser_Writer_WriteAotCFileWithOptions()` returned success.

GREEN:

- Default hybrid dynamic-call generation still emits and compiles the dynamic deopt bridge.
- Full-AOT dynamic-call generation returns `ZR_FALSE`.
- The rejected full-AOT path leaves no generated C artifact behind.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'`
  - 3 tests, 0 failures

## Acceptance Decision

Accepted as 12-S8A only. Dynamic call deopt fallback is no longer allowed in full-AOT mode unless a static callable
target is proven, but broader full-AOT closure diagnostics remain open.
