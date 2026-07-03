# AOT 11-S4R-union - generated union ownership offset table

## Scope

11-S4R-union extends generated `SZrTypeLayout` descriptors so union layouts with owner payload fields carry the same generated ownership-offset table shape already used for struct owner fields.

Affected layers:
- AOT C type-layout descriptor generation
- generated code-registration type-layout metadata
- parser AOT generated-C smoke coverage
- AOT 10/11 plan documentation

Affected code:
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`

## Baseline

Before this slice, `backend_aot_c_type_layout_can_emit_ownership_offsets()` rejected union layouts even when the union layout already had `ownershipFieldCount > 0` and the owner payload byte offset was derivable from the generated `SZrTypeLayoutField` table.

Pre-change RED evidence:
- `zr_vm_aot_c_value_type_shared_library_smoke_test` failed after adding a `Shared<Box>` union payload fixture.
- Failure: `test_aot_c_generated_union_type_layout_emits_ownership_offsets_for_owner_payloads:FAIL: Expected Non-NULL`.
- The missing generated text was `static const TZrUInt32 ZrOwnershipOffsets_`.

Known repository baseline:
- The worktree contains unrelated dirty LSP/numeric inference files and generated build directories.
- Windows value-type shared-library execution is Unix-only and remains ignored by design.
- Existing Clang/MSVC warnings in parser/library code are not introduced by this slice.

## Test Inventory

Focused subsystem cases:
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  - Adds a union `Resource` with payload `Open(handle: Shared<Box>)`.
  - Requires generated C to contain union descriptor `.kind = 2u`.
  - Requires `ZrOwnershipOffsets_<typeLayoutId>[]`.
  - Requires `/* zr_aot_ownership_offsets layout=... count=1 */`.
  - Requires `.ownershipFieldCount = 1u`.
  - Requires `.ownershipFieldOffsets = ZrOwnershipOffsets_<typeLayoutId>`.
  - Rejects `zr_aot_ownership_offsets_failed`.

Regression cases:
- Existing struct owner-field offset generation remains covered by the same smoke binary.
- Existing type-layout source contracts remain covered by `aot_c_type_layout_contracts`.
- Existing generated-C source contracts remain covered by `zr_vm_aot_c_source_contracts_test`.

Boundary and negative cases:
- Union owner payload offsets are accepted only when the existing validation can derive a bounded `SZrTypeValue` ownership offset.
- Zero ownership-field layouts still emit no ownership offset table.
- Unsafe or unsupported offsets still keep `.ownershipFieldOffsets = ZR_NULL` and emit the failure marker.
- GC descriptor generation remains union-conservative; this slice does not make union GC descriptors active-tag-aware.

## Tooling Evidence

RED command:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_value_type_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

RED output summary:
- 5 tests executed.
- 1 failure in `test_aot_c_generated_union_type_layout_emits_ownership_offsets_for_owner_payloads`.
- Failure reason: expected generated `ZrOwnershipOffsets_` text was missing.

GREEN / validation commands:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_value_type_shared_library_smoke_test -j 8 && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_source_contracts_test -j 8 && ctest --test-dir build-wsl-gcc -R 'aot_c_type_layout_contracts|aot_c_source_contracts|aot_c_value_type_shared_library_smoke' --output-on-failure --parallel 3"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test -j 8 && ctest --test-dir build-wsl-clang -R '^aot_c_type_layout_contracts$' --output-on-failure && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
. 'C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1'
cmake --build build-msvc --config Debug --target zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test --parallel 8
ctest --test-dir build-msvc -C Debug -R '^aot_c_type_layout_contracts$' --output-on-failure
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_value_type_shared_library_smoke_test.exe
```

Generated artifact evidence:
- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/ownership_offsets/src/owner_union.c` contains `static const TZrUInt32 ZrOwnershipOffsets_1[]`.
- The same generated file contains `.kind = 2u`.
- The same generated file contains `.ownershipFieldOffsets = ZrOwnershipOffsets_1`.
- The same generated file does not contain `zr_aot_ownership_offsets_failed`.

## Results

Passed checks:
- WSL GCC direct value-type smoke: 5 tests, 0 failures.
- WSL GCC source contracts: 22 tests, 0 failures.
- WSL GCC CTest `aot_c_type_layout_contracts`: 1/1 passed.
- WSL Clang source contracts: 22 tests, 0 failures.
- WSL Clang direct value-type smoke: 5 tests, 0 failures.
- WSL Clang CTest `aot_c_type_layout_contracts`: 1/1 passed.
- Windows MSVC Debug source contracts: 22 tests, 0 failures.
- Windows MSVC Debug value-type smoke: 5 tests, 0 failures, 1 ignored Unix-only branch.
- Windows MSVC Debug CTest `aot_c_type_layout_contracts`: 1/1 passed.

Fixes made:
- Removed the union-specific exclusion from ownership-offset descriptor emission.
- Kept the existing bounded offset validation and `SZrTypeValue` size checks.
- Preserved union active payload semantics in `SZrTypeLayoutField.activeTag` and tag metadata.

## Acceptance Decision

Accepted for the focused 11-S4R-union slice.

The change has direct RED/GREEN evidence and WSL GCC, WSL Clang, and Windows MSVC Debug focused validation. It closes generated union ownership-offset publication only. Remaining work includes public `FieldInfo` object materialization, field value marshaling, runtime generic layout construction, persistent cTypeId-to-token indexing, cross-module token publication/rewrite, and complete trim analysis.
