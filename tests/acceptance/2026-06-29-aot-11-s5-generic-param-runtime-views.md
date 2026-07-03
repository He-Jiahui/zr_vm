# AOT 11-S5 - generic parameter runtime metadata views

## Scope

This slice adds read-only runtime views for zrp `GenericParam` and
`GenericParamConstraint` rows.

Affected layers:
- metadata runtime public API
- attached zrp metadata section consumption
- module metadata runtime query tests
- AOT 11 plan documentation

Affected code:
- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
- `zr_vm_core/src/zr_vm_core/metadata_runtime_generic_params.c`
- `tests/module/test_metadata_runtime_query.c`

## Baseline

Before this slice, zrp metadata already carried `GenericParam` and
`GenericParamConstraint` definition rows and pruning could preserve/remap them,
but the metadata runtime exposed no public view to read a TypeDef/MethodDef
generic parameter definition or its constraints.

Pre-change RED evidence:
- `zr_vm_metadata_runtime_query_test` failed to compile after adding the new
  runtime query coverage.
- Missing symbols included `SZrMetadataRuntimeGenericParamView`,
  `SZrMetadataRuntimeGenericParamConstraintView`,
  `ZrCore_MetadataRuntime_ReadGenericParamView()`, and
  `ZrCore_MetadataRuntime_ReadGenericParamConstraintView()`.

Known repository baseline:
- The worktree contains unrelated dirty files from other AOT 07-12 slices.
- This slice does not claim full 11-S5 completion.

## Test Inventory

Focused subsystem case:
- `tests/module/test_metadata_runtime_query.c`
  - Reads a TypeDef-owned generic parameter.
  - Reads a MethodDef-owned generic parameter.
  - Verifies owner token records, row pointers, parameter index,
    `nameStringOffset`, constraint range, and flags.
  - Reads a constraint with a validated signature blob.
  - Reads a constraint with a zero-length signature blob.
  - Rejects null runtime/output, missing zrp metadata, out-of-range parameter
    index, and out-of-range constraint index.

Boundary and negative cases:
- Owners are accepted only for `TYPE_DEF` and real MethodDef `MEMBER_DEF` rows.
- Generic parameter row indexes must fall inside the owning row's range.
- Constraint rows must point back to the resolved generic parameter index.
- Constraint type tokens must resolve to runtime type records.
- Non-empty constraint signature blobs must pass existing zrp signature blob
  structural validation.

## Tooling Evidence

RED command:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test -j 8"
```

RED output summary:
- Compile failed on missing generic-parameter view structs and read APIs.

GREEN / validation commands:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_runtime_query_test -j 8 && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build-msvc --target zr_vm_metadata_runtime_query_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_typespec_layout_test -j 8
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
.\build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_typespec_layout_test.exe
```

## Results

Passed checks:
- WSL GCC metadata runtime query: 23 tests, 0 failures.
- WSL Clang metadata runtime query: 23 tests, 0 failures.
- Windows MSVC Debug metadata runtime query: 23 tests, 0 failures.
- WSL GCC reflection token resolve: 2 tests, 0 failures.
- WSL GCC metadata runtime TypeSpec layout: 14 tests, 0 failures.
- Windows MSVC Debug reflection token resolve: 2 tests, 0 failures.
- Windows MSVC Debug metadata runtime TypeSpec layout: 14 tests, 0 failures.

Fixes made:
- Added public generic parameter and generic parameter constraint view structs.
- Added public read APIs for owner/parameter and owner/parameter/constraint
  queries.
- Added a separate implementation file to keep the existing metadata runtime
  source file from taking on another responsibility.
- Validated owner ranges, constraint ownership, resolved records, and optional
  signature blobs.

## Acceptance Decision

Accepted for the focused 11-S5 runtime-view slice.

This closes runtime inspection of `GenericParam` and `GenericParamConstraint`
rows only. MethodSpec runtime instance binding, full generic signature
canonicalization with 08 dedupe keys, runtime generic layout construction,
public generic reflection objects, cross-module token publication/rewrite, and
full trim analysis remain open.
