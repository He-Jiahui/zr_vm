# AOT C Frame Setup Direct Policy

## Scope

- Generated AOT C frame setup now emits the call-frame diagnostic and observation-policy initialization directly.
- The generated `backend_aot_write_c_frame_setup()` output now calls `ZrLibrary_AotRuntime_ResolveGeneratedModuleContext()` for loaded-module record lookup, function-table binding, module export state, generated thunk table binding, and generated frame slot count.
- The public context type is `ZrAotGeneratedModuleContext`; generated C and the active runtime no longer expose the old generic `ZrAotGeneratedContext` / `ZrLibrary_AotRuntime_GetGeneratedContext` naming.
- `ZrLibrary_AotRuntime_ReportGeneratedContextError()` was removed from the public active runtime surface because generated C now reports the no-call-frame setup failure with `ZrCore_Debug_RunError()`.
- Affected layers: AOT C frame setup emitter, active AOT runtime public header/source, focused frame setup contract, generated-C shared-library smoke, docs.

## Baseline

- RED 1: `zr_vm_aot_c_frame_setup_contracts_test` first failed on missing `ZrCore_Debug_RunError(state,` after the contract required direct generated call-frame diagnostics and direct observation-policy setup.
- RED 2: after the generated frame setup was changed, the same contract failed on forbidden `ZrLibrary_AotRuntime_ReportGeneratedContextError` still present in the active runtime header/source.
- RED 3: the context-boundary contract then failed on missing `typedef struct ZrAotGeneratedModuleContext` after it required explicit loaded-module context naming and forbade the old generic context names.
- Existing repository baseline remains dirty with broad unrelated semantic/LSP/debug/AOT work. This acceptance only covers the focused AOT C frame setup direct-policy slice.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_frame_setup_contracts.c`.
- Generated-C compile/runtime smoke: `tests/parser/test_aot_c_shared_library_smoke.c`.
- Production source scans:
  - no `ZrLibrary_AotRuntime_ReportGeneratedContextError` remains outside archived `.codex/minimal-verify*` copies.
  - active C/header sources no longer contain `ZrLibrary_AotRuntime_GetGeneratedContext` or `ZrAotGeneratedContext`.
  - frame setup source forbids `ZrLibrary_AotRuntime_BeginGeneratedFunction`, `ZrLibrary_AotRuntime_ReportGeneratedContextError`, `ZrLibrary_AotRuntime_GetGeneratedContext`, `ZrAotGeneratedContext`, and `ZrLibrary_AotRuntime_GetObservationPolicy`.
- Boundary cases:
  - missing call frame reports from generated C and fails through `ZR_AOT_C_FAIL()`.
  - default observation policy is emitted as an explicit OR of `ZR_AOT_GENERATED_STEP_FLAG_*` values.
  - explicit AOT observation override reads `state->aotObservationMask` and `state->aotPublishAllInstructions`.
  - active line debug hook forces `frame.publishAllInstructions = ZR_TRUE`.

## Tooling Evidence

- RED 1 command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test"`
- RED 1 result:
  `Missing source contract text: ZrCore_Debug_RunError(state,`
- RED 2 command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test"`
- RED 2 result:
  `Unexpected source contract text: ZrLibrary_AotRuntime_ReportGeneratedContextError`
- RED 3 command:
  `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test"`
- RED 3 result:
  `Missing source contract text: typedef struct ZrAotGeneratedModuleContext`
- GCC focused contract:
  `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test"`
- GCC generated-C smoke:
  included in the combined GCC command above.
- Clang focused contract:
  `wsl.exe bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test -j 8 && ./build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test"`
- Clang generated-C smoke:
  included in the combined Clang command above.
- MSVC focused contract:
  `cmake --build build-msvc --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_shared_library_smoke_test --config Debug --parallel 8`
  `.\build-msvc\bin\Debug\zr_vm_aot_c_frame_setup_contracts_test.exe`
- MSVC generated-C smoke:
  `.\build-msvc\bin\Debug\zr_vm_aot_c_shared_library_smoke_test.exe`
- Production scan:
  `Get-ChildItem -Path 'zr_vm_aot','zr_vm_library','zr_vm_common','tests' -Include *.c,*.h -File -Recurse | Select-String -Pattern 'ZrLibrary_AotRuntime_GetGeneratedContext|ZrAotGeneratedContext'`

## Results

- GCC `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- GCC `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- Clang `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- MSVC `zr_vm_aot_c_frame_setup_contracts_test.exe`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_shared_library_smoke_test.exe`: target built; 8 Unix-only runtime tests ignored as expected.
- GCC and Clang still report pre-existing const-discard warnings in `zr_vm_library/src/zr_vm_library/project/project.c`. MSVC still reports pre-existing warnings in `aot_runtime.c` and `project.c`.
- Production scan found no active `ZrLibrary_AotRuntime_ReportGeneratedContextError`, `ZrLibrary_AotRuntime_GetGeneratedContext`, or `ZrAotGeneratedContext` references outside focused forbidden-string assertions and historical docs.

## Acceptance Decision

- Accepted for the AOT C frame setup direct-policy slice.
- Generated C now owns simple frame-setup diagnostics and observation policy initialization rather than routing those through AOT runtime helper wrappers.
- Remaining risks: `ZrLibrary_AotRuntime_ResolveGeneratedModuleContext()` remains an explicit generated-frame setup boundary for private loaded-module/runtime-record metadata; LLVM still has older helper-backed setup/lowering paths until parity work.
