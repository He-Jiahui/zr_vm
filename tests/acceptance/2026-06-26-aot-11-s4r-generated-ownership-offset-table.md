# AOT 11-S4R - generated ownership offset table emission

## Scope

11-S4R extends generated `SZrTypeLayout` descriptors so struct layouts with owned inline `SZrTypeValue` fields carry a generated ownership-offset table instead of leaving `.ownershipFieldOffsets` null.

Affected layers:
- AOT C type-layout descriptor generation
- generated code-registration type-layout metadata
- parser AOT generated-C smoke coverage
- plan/module documentation

Affected code:
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_type_layouts.c`
- `tests/parser/test_aot_c_type_layout_contracts.c`
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`

## Baseline

Generated type-layout descriptors already emitted `ownershipFieldCount`, but always wrote `.ownershipFieldOffsets = ZR_NULL`. A runtime consumer could see that a layout had owned fields but could not follow a generated offset table from the descriptor.

Pre-change RED evidence:
- `zr_vm_aot_c_type_layout_contracts_test` failed after requiring ownership-offset writer source contracts.
- The missing contract was `backend_aot_c_type_layout_can_emit_ownership_offsets(`.

Known repository baseline:
- The worktree contains many unrelated existing modified and untracked files/build directories.
- Windows value-type/shared-library smoke tests keep the Unix shared-library execution branch ignored by design.

## Test Inventory

Focused subsystem cases:
- `tests/parser/test_aot_c_type_layout_contracts.c`
  - Requires the generated type-layout source to contain the ownership-offset emission helper.
  - Requires generated `ZrOwnershipOffsets_%u[]` table shape.
  - Requires generated descriptors to point `.ownershipFieldOffsets` to the table when emission is possible.
- `tests/parser/test_aot_c_value_type_shared_library_smoke.c`
  - Compiles a struct containing `Unique<string>`.
  - Requires generated C to contain `ZrOwnershipOffsets_`.
  - Requires `ownershipFieldCount = 1u`.
  - Requires descriptor `.ownershipFieldOffsets = ZrOwnershipOffsets_`.
  - Rejects `zr_aot_ownership_offsets_failed`.

Regression cases:
- `tests/parser/test_aot_c_source_contracts.c`
- Existing GC descriptor and TypeDef/TypeSpec token-table smoke coverage remain in the same focused binaries.

Boundary and negative cases:
- Struct owner fields are accepted when offsets are either present in `ownershipFieldOffsets` or derivable from managed fields carrying `OWNERSHIP_VALUE`.
- Unsupported/unsafe layouts keep `ZR_NULL` and write an explicit failure comment.
- Union ownership-offset table emission remains conservative and is not claimed by this slice.

## Tooling Evidence

Tool versions:
- WSL GCC: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0`
- WSL clang: `Ubuntu clang version 14.0.0-1ubuntu1.1`
- MSVC: `19.44.35227.0`

RED command:
```powershell
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_type_layout_contracts_test -j1 && ./build-wsl-gcc/bin/zr_vm_aot_c_type_layout_contracts_test"
```

RED output summary:
- `test_aot_c_type_layouts_emit_generated_struct_static_asserts:FAIL: Expected Non-NULL`
- Missing source contract text: `backend_aot_c_type_layout_can_emit_ownership_offsets(`

GREEN commands:
```powershell
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_source_contracts_test -j2 && ctest --test-dir build-wsl-gcc -R '^aot_c_type_layout_contracts$' --output-on-failure && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
wsl -d Ubuntu-22.04 -- bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_source_contracts_test -j2 && ctest --test-dir build-wsl-clang -R '^aot_c_type_layout_contracts$' --output-on-failure && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_value_type_shared_library_smoke_test"
```

```powershell
cmake --build build-msvc --config Debug --target zr_vm_aot_c_type_layout_contracts_test zr_vm_aot_c_value_type_shared_library_smoke_test zr_vm_aot_c_source_contracts_test --parallel 2
ctest --test-dir build-msvc -C Debug -R '^aot_c_type_layout_contracts$' --output-on-failure
.\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe
.\build-msvc\bin\Debug\zr_vm_aot_c_value_type_shared_library_smoke_test.exe
```

Generated artifact evidence:
- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/ownership_offsets/src/owner_struct.c` contains `/* zr_aot_ownership_offsets layout=0 count=1 */`.
- The same generated file contains `static const TZrUInt32 ZrOwnershipOffsets_0[]`.
- The generated `SZrTypeLayout` descriptor contains `.ownershipFieldCount = 1u` and `.ownershipFieldOffsets = ZrOwnershipOffsets_0`.

## Results

Passed checks:
- WSL GCC CTest: `aot_c_type_layout_contracts` passed 1/1.
- WSL GCC direct executables: source contracts 19/0, value-type shared-library smoke 4/0.
- WSL clang CTest: `aot_c_type_layout_contracts` passed 1/1.
- WSL clang direct executables: source contracts 19/0, value-type shared-library smoke 4/0.
- Windows MSVC Debug CTest: `aot_c_type_layout_contracts` passed 1/1.
- Windows MSVC Debug direct executables: source contracts 19/0, value-type smoke 3/0/1 ignored.

Fixes made:
- Added generated ownership-offset table emission for eligible struct layouts.
- Added descriptor wiring so `.ownershipFieldOffsets` points at `ZrOwnershipOffsets_<typeLayoutId>`.
- Preserved `ZR_NULL` for zero-count, union, and unsafe/unsupported offset cases.

## Acceptance Decision

Accepted for 11-S4R.

The change has direct RED/GREEN evidence and focused gcc, clang, and MSVC validation. It closes generated ownership-offset table emission for struct owner fields, but not union owner-offset table emission, persistent cTypeId-to-token indexing, runtime generic layout construction, cross-module token-table policy, MethodSpec materialization, or public reflection entity support.
