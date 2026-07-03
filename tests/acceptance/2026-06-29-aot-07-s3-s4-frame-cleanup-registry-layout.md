# AOT 07-S3/S4 + 11-S4G - frame cleanup registry layout resolver

## Scope

This slice moves generated value-frame cleanup drop layout lookup from the function prototype layout cache to the function-attached AOT code-registration layout registry.

Affected layers:
- AOT C frame cleanup source emission
- generated C include surface
- generated source-contract coverage
- AOT 07 and 11 plan records

Affected code:
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c`
- `tests/parser/test_aot_c_source_contracts.c`

## Baseline

Before this slice, `backend_aot_c_frame_cleanup.c` emitted:

```c
ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function, typeLayoutId, state)
```

That meant generated inline-struct cleanup could still consult the prototype layout cache even when the loaded AOT function had an attached code-registration layout registry. GC inline-frame scanning already had a function-level metadata runtime resolver from 11-S4G, but generated cleanup was not using it.

Known repository baseline:
- The worktree contains unrelated dirty LSP/numeric inference files.
- Value SemIR copy/field helper paths still contain prototype resolver calls; this acceptance only covers value-frame cleanup drop lookup.
- Windows value-type shared-library execution keeps its existing Unix-only runtime branch ignored.

## Test Inventory

Focused source-contract coverage:
- `tests/parser/test_aot_c_source_contracts.c`
  - Requires generated C to include `zr_vm_core/metadata_runtime.h`.
  - Requires cleanup emission to contain `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function`.
  - Rejects `ZrCore_Function_ResolvePrototypeFrameTypeLayout(frame.function` inside `backend_aot_c_frame_cleanup.c`.

Regression coverage:
- `zr_vm_aot_c_source_contracts_test`
- `zr_vm_aot_c_frame_setup_contracts_test`
- `zr_vm_aot_c_value_type_shared_library_smoke_test`

Boundary cases:
- The resolver migration is limited to cleanup drop lookup for inline-struct frame slots.
- Existing value SemIR copy/field lookup code remains out of scope.
- Non-AOT interpreter/VM prototype fallback behavior is not changed by this generated-C-only slice.

## RED

RED command:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"
```

RED output summary:
- 22 tests executed.
- 1 failure in `test_aot_c_source_emits_value_frame_cleanup_exit`.
- Failure reason: missing required source contract text `#include \"zr_vm_core/metadata_runtime.h\"`.

## GREEN

Implementation:
- `backend_aot_c_frame_cleanup.c` now emits `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, typeLayoutId)` for cleanup drop layout lookup.
- `backend_aot_c_emitter.c` now emits `#include "zr_vm_core/metadata_runtime.h"` into generated C.
- `test_aot_c_source_contracts.c` locks the new include/resolver and forbids the old cleanup-local prototype resolver emission.

GREEN / validation commands:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test -j 8 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build-msvc --config Debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test --parallel 8
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_frame_setup_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_value_type_shared_library_smoke_test.exe
```

Generated artifact checks:

```powershell
Get-ChildItem -Recurse 'build-wsl-gcc/tests_generated/aot_c_value_type_shared_library' -Filter '*.c' |
    Select-String -Pattern 'metadata_runtime.h|ZrCore_MetadataRuntime_ResolveFunctionTypeLayout'
```

The generated value-type smoke artifacts contain the new generated include and cleanup blocks with `ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(frame.function, ...)`.

## Results

Passed checks:
- WSL GCC source contracts: 22 tests, 0 failures.
- WSL GCC frame setup contracts: 1 test, 0 failures.
- WSL GCC value-type shared-library smoke: 5 tests, 0 failures.
- WSL Clang source contracts: 22 tests, 0 failures.
- WSL Clang frame setup contracts: 1 test, 0 failures.
- WSL Clang value-type shared-library smoke: 5 tests, 0 failures.
- Windows MSVC Debug source contracts: 22 tests, 0 failures.
- Windows MSVC Debug frame setup contracts: 1 test, 0 failures.
- Windows MSVC Debug value-type smoke: 5 tests, 0 failures, 1 ignored Unix-only branch.

## Acceptance Decision

Accepted for the focused 07-S3/S4 + 11-S4G support slice.

The change has direct RED/GREEN evidence, WSL GCC, WSL Clang, and Windows MSVC Debug validation, and generated artifact confirmation. It closes generated value-frame cleanup registry-backed layout lookup only. Remaining work includes value SemIR copy/field resolver migration, broader byte-frame narrowing, GC roots/exports cleanup, runtime generic layout construction, persistent cTypeId-to-token indexing, cross-module token publication/rewrite, and complete trim analysis.
