# AOT 10-S4D Generic Parameter Reflection Carrier

## Scope

This slice connects the 11-S5 GenericParam and GenericParamConstraint runtime
views to public reflection carrier APIs.

Affected layers:
- reflection public API
- reflection token resolver implementation
- attached zrp GenericParam/constraint runtime view consumption
- focused module tests
- AOT 10/11/index plan status records

Affected code:
- `zr_vm_core/include/zr_vm_core/reflection.h`
- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c`
- `tests/module/test_reflection_token_resolve.c`

## Baseline

Before this slice, 11-S5 exposed runtime-only views for zrp
`GENERIC_PARAMS` and `GENERIC_PARAM_CONSTRAINTS`. Public reflection could
resolve TypeDef/TypeSpec/FieldDef/MethodDef carriers, but it had no public
entry point for a TypeDef-owned or MethodDef-owned generic parameter, and no
carrier for a parameter constraint's type token or signature blob.

Initial RED was the focused reflection token resolver test compiling against
the intended public carrier. The build failed because these public symbols did
not exist:
- `SZrReflectionResolvedGenericParameter`
- `SZrReflectionResolvedGenericParameterConstraint`
- `ZrCore_Reflection_ResolveGenericParameter()`
- `ZrCore_Reflection_ResolveGenericParameterConstraint()`

Known repository baseline:
- The worktree contains unrelated dirty files from other active AOT/LSP slices.
- This slice does not claim full 10-S4 or 11-S5 completion.

## Test Inventory

Focused subsystem case:
- `tests/module/test_reflection_token_resolve.c`
  - Builds synthetic TypeDef and MethodDef owners with GenericParam rows.
  - Reads a TypeDef-owned generic parameter carrier.
  - Reads a MethodDef-owned generic parameter carrier.
  - Reads a GenericParamConstraint carrier backed by a TypeRef token.
  - Verifies owner records, row pointers, parameter indexes,
    `nameStringOffset`, flags, first constraint index, and constraint count.
  - Verifies constraint type token/record and validated signature blob fields.
  - Rejects out-of-range constraint indexes.

Adjacent tests:
- `zr_vm_metadata_runtime_query_test`
- `zr_vm_metadata_runtime_typespec_layout_test`

Boundary coverage:
- Reflection APIs reject null runtime/output and zero owner token through the
  same clear-and-false behavior as existing resolver APIs.
- Generic parameter and constraint data remain sourced from 11-S5 runtime
  views; reflection does not add a second index or duplicate validation path.
- Constraint type identity is carried as token/record, leaving object
  materialization to a later public reflection slice.

## Tooling Evidence

RED command:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8"
```

RED output summary:
- Compile failed on missing public generic-parameter reflection carrier types
  and APIs.

GREEN / validation commands:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build-msvc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8
.\build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe
.\build-msvc\bin\Debug\zr_vm_metadata_runtime_typespec_layout_test.exe
```

## Results

Passed checks:
- WSL GCC reflection token resolve: 3 tests, 0 failures.
- WSL Clang reflection token resolve: 3 tests, 0 failures.
- WSL GCC metadata runtime query: 23 tests, 0 failures.
- WSL GCC metadata runtime TypeSpec layout: 14 tests, 0 failures.
- Windows MSVC Debug reflection token resolve: 3 tests, 0 failures.
- Windows MSVC Debug metadata runtime query: 23 tests, 0 failures.
- Windows MSVC Debug metadata runtime TypeSpec layout: 14 tests, 0 failures.

Fixes made:
- Added public generic parameter and constraint reflection carrier structs.
- Added public resolver APIs for owner/parameter and
  owner/parameter/constraint queries.
- Reused 11-S5 metadata runtime views to populate the public carriers.
- Extended focused reflection token resolver coverage to TypeDef and MethodDef
  generic parameter owners plus a TypeRef-backed constraint.

## Acceptance Decision

Accepted for the focused 10-S4D / 11-S5 public carrier slice.

This closes public carrier exposure for GenericParam and
GenericParamConstraint metadata only. Public generic reflection objects,
MethodSpec runtime instance binding, `MakeGenericType`, runtime generic layout
construction, cross-module token publication/rewrite, and full trim analysis
remain open.
