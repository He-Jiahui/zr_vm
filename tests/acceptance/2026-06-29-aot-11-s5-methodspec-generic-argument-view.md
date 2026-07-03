# AOT 11-S5 / 10-S4E - MethodSpec Generic Argument View

## Scope

This slice adds indexed MethodSpec generic argument inspection to the metadata
runtime and exposes the same data through a public reflection carrier.

Affected layers:
- metadata runtime public API
- reflection public API
- reflection token resolver implementation
- focused module tests
- AOT 10/11/index plan status records

Affected code:
- `zr_vm_core/include/zr_vm_core/metadata_runtime.h`
- `zr_vm_core/src/zr_vm_core/metadata_runtime.c`
- `zr_vm_core/include/zr_vm_core/reflection.h`
- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c`
- `tests/module/test_metadata_runtime_query.c`
- `tests/module/test_reflection_token_resolve.c`

## Baseline

Before this slice, 11-S3M could read a MethodSpec signature view over
`GENERIC_INST(MEMBER_REF methodToken, args...)`, but consumers had no indexed
reader for a specific generic method argument. Public reflection also had no
carrier for MethodSpec argument identity.

Initial RED was the focused metadata runtime and reflection resolver tests
compiling against the intended APIs. The build failed because these symbols did
not exist:
- `SZrMetadataRuntimeMethodSpecGenericArgumentView`
- `ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView()`
- `SZrReflectionResolvedMethodSpecGenericArgument`
- `ZrCore_Reflection_ResolveMethodSpecGenericArgument()`

Known repository baseline:
- The worktree contains unrelated dirty files from other active AOT/LSP slices.
- This slice does not claim full 10-S4 or 11-S5 completion.

## Test Inventory

Focused metadata runtime case:
- `tests/module/test_metadata_runtime_query.c`
  - Builds a synthetic MethodSpec signature:
    `GENERIC_INST(MEMBER_REF methodToken, [primitive, TypeRef])`.
  - Reads argument 0 as a primitive node without a token record.
  - Reads argument 1 as a TypeRef node with token/record binding.
  - Rejects null runtime/output, wrong token kind, missing attached zrp data,
    and out-of-range argument indexes.

Focused reflection case:
- `tests/module/test_reflection_token_resolve.c`
  - Reads the same MethodSpec argument identities through
    `ZrCore_Reflection_ResolveMethodSpecGenericArgument()`.
  - Verifies methodSpec token, method token/record, signature hash, argument
    index, argument node kind/payloads, and optional argument token/record.

Adjacent regression case:
- `zr_vm_metadata_runtime_typespec_layout_test`
  - Kept in the validation set because the new reader shares the existing
    signature type-node traversal pattern with TypeSpec generic argument views.

## Tooling Evidence

RED command:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test zr_vm_reflection_token_resolve_test -j 8"
```

RED output summary:
- Compile failed on missing MethodSpec generic argument runtime view type/API.
- Compile failed on missing MethodSpec generic argument public reflection
  carrier type/API.

GREEN / validation commands:
```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_metadata_runtime_query_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"
```

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_metadata_runtime_query_test zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_typespec_layout_test"
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
- WSL GCC metadata runtime query: 24 tests, 0 failures.
- WSL GCC reflection token resolve: 4 tests, 0 failures.
- WSL GCC metadata runtime TypeSpec layout: 14 tests, 0 failures.
- WSL Clang metadata runtime query: 24 tests, 0 failures.
- WSL Clang reflection token resolve: 4 tests, 0 failures.
- WSL Clang metadata runtime TypeSpec layout: 14 tests, 0 failures.
- Windows MSVC Debug metadata runtime query: 24 tests, 0 failures.
- Windows MSVC Debug reflection token resolve: 4 tests, 0 failures.
- Windows MSVC Debug metadata runtime TypeSpec layout: 14 tests, 0 failures.

Fixes made:
- Added the MethodSpec generic argument runtime view and read API.
- Bound direct TypeDef/TypeRef MethodSpec arguments back to metadata token
  records while preserving primitive argument nodes as node-only results.
- Added the public MethodSpec generic argument reflection carrier and resolver.
- Initialized shared signature argument node locals so MSVC does not warn about
  possible uninitialized reads in TypeSpec/MethodSpec indexed argument readers.

## Acceptance Decision

Accepted for the focused 11-S5 / 10-S4E MethodSpec generic argument view and
public carrier slice.

This closes indexed inspection and public carrier exposure for MethodSpec
generic arguments only. Public generic method reflection objects, MethodSpec
runtime instance materialization, generic dictionaries, runtime generic layout
construction, cross-module token publication/rewrite, and full trim analysis
remain open.
