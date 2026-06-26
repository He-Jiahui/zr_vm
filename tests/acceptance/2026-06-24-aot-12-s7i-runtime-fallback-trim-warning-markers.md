# 2026-06-24 AOT 12-S7I Runtime Fallback Trim Warning Markers

## Scope

12-S7I publishes the first trim-warning marker surface for hybrid AOT C modules that keep runtime fallback boundaries:

- `trim_warnings.runtimeFallbackCount`
- `trim_warning.runtimeFallback[index] function=<flatIndex> instruction=<instructionIndex> reason=dynamic-runtime`

The warning scan reuses the full-AOT closure check: if an instruction would make `requireFullAot` reject the module, the
hybrid output reports it as a runtime fallback warning.

This is not a complete trim analyzer. It does not yet provide annotation dataflow, source-span diagnostics, warning
classification, warning suppression, zrp metadata trimming, or release symbol stripping.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
  counts runtime fallback warnings after code stripping and full-AOT validation, emits the aggregate count, and writes
  one `trim_warning.runtimeFallback[...]` marker per fallback instruction.
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`
  verifies the hybrid SemIR dynamic-call deopt bridge still emits and compiles while also reporting the warning marker.

## RED / GREEN

RED:

- `zr_vm_aot_c_dynamic_deopt_bridge_smoke_test` failed when the hybrid dynamic deopt bridge smoke required the runtime
  fallback warning marker.

GREEN:

- Hybrid dynamic deopt bridge output reports one runtime fallback warning for function 0 instruction 0.
- Hybrid dynamic deopt bridge generation and shared-library compilation still pass.
- Full-AOT dynamic call and dynamic value-access rejection paths remain green.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_dynamic_deopt_bridge_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'`
  - 4 tests, 0 failures

## Acceptance Decision

Accepted as 12-S7I only. Runtime fallback warnings are now visible in generated AOT C, but full trim analysis,
annotation-driven preservation, source-span diagnostics, zrp metadata section/table/pool statistics, and release symbol
stripping remain open.
