# AOT 07-S3/S4 Generic Numeric Zero-Frame Body Local

## Scope

- Plan slice: 07-S3/S4 pure scalar generated body and zero `registerFrameBytes` follow-up.
- Affected layers: AOT C frame descriptor gating, generic numeric generated-C smoke coverage, and source-contract coverage.
- Focused shape: proven generic numeric scalar-local functions where constants, stack copies, downstream `ADD/SUB/MUL/DIV/MOD/NEG`, and direct returns can all stay in C locals.
- Goal: when the generated method metadata reports `.registerFrameBytes = 0u` and value SemIR lowering reports `frameByteSize=0`, proven scalar-local generic numeric bodies must also omit the generated frame setup block.

## Baseline

- The existing result-copy guarded `DIV`/`MOD` matrix already generated scalar-local arithmetic and method metadata with `.registerFrameBytes = 0u`.
- The body still emitted `ZrAotGeneratedFrame frame = {0};`, `/* zr_aot_generated_frame_setup */`, `ZrCore_Function_CheckStackAndGc(...)`, and `frame.slotBase = zr_aot_slot_base;`.
- That meant 07-S3/S4 zero-frame metadata and pure scalar body generation were not aligned for generic numeric arithmetic.

## Test Inventory

- Added generated-C guard coverage in the generic numeric shared-library smoke helper:
  - requires `.registerFrameBytes = 0u`;
  - requires `value SemIR lowering frameByteSize=0`;
  - forbids `/* zr_aot_generated_frame_setup */`;
  - forbids `ZrAotGeneratedFrame frame = {0};`;
  - forbids `frame.slotBase = zr_aot_slot_base;`;
  - forbids `ZrCore_Function_CheckStackAndGc(`;
  - forbids frame-slot reset materialization for the focused copied-result bodies.
- Added source-contract needles for generic numeric frame-descriptor local-only gates covering `ADD`, `ADD_STRING`, `DIV`, `MOD`, and `NEG`.

## Production Change

- `backend_aot_c_frame_descriptor.c` now mirrors generic numeric scalar-local eligibility before deciding that a function body needs a frame descriptor.
- Proven same-kind `i64`, `u64`, and `f64` binary generic numeric operations can be frame-free when operands are written scalar locals and the destination result can skip value-slot materialization.
- Proven mixed `i64/u64` signed-result and mixed `f64` plus integer operations can be frame-free using the same conservative written-kind proof as generic numeric lowering.
- Proven generic `NEG` can be frame-free for i64, f64, and u64-to-i64 scalar-local negation.
- Runtime fallback shapes remain descriptor-backed because the new gate returns false unless scalar-local result and operand proof succeeds.

## RED/GREEN Evidence

- RED: WSL GCC generic numeric smoke built but failed 4 tests after the zero-frame guard was added:
  - f64 result-copy `MOD`;
  - f64 result-copy right `MOD`;
  - i64 result-copy `DIV`;
  - i64 result-copy right `DIV`.
- RED failure: generated C still contained `/* zr_aot_generated_frame_setup */` even though `.registerFrameBytes = 0u` was present.
- GREEN WSL GCC command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- GREEN WSL GCC result: `50 Tests 0 Failures 0 Ignored`.
- GREEN WSL Clang command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test"`.
- GREEN WSL Clang result: `50 Tests 0 Failures 0 Ignored`.
- GREEN source contract command:
  `wsl.exe -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test"`.
- GREEN source contract result: `1 Tests 0 Failures 0 Ignored`.
- GREEN MSVC Debug generic numeric command:
  `cmd.exe /c "call ""E:\Visual Studio\Common7\Tools\VsDevCmd.bat"" -arch=x64 && cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_generic_numeric_shared_library_smoke_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_generic_numeric_shared_library_smoke_test.exe"`.
- GREEN MSVC Debug generic numeric result: `50 Tests 0 Failures 50 Ignored`.
- GREEN MSVC Debug source contract result: `1 Tests 0 Failures 0 Ignored`.

## Acceptance Decision

- Accepted for the covered 07-S3/S4 proven generic numeric scalar-local zero-frame body slice.
- The focused generated C now aligns zero register frame metadata with omission of generated frame setup for the covered scalar-local generic numeric bodies.
- Remaining 07 work: dynamic/unproven operands, broader value-copy migration, GC roots/exports/frame cleanup, byte-frame narrowing outside this proven scalar-local body gate, performance counters, and complete zero-frame typed bodies.
