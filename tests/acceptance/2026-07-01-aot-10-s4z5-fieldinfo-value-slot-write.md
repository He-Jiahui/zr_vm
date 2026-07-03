# AOT 10-S4Z5 FieldInfo Value-Slot Write

## Scope

- Plan slices: `10-S4Z5 / 11-S4AQ / 12-S5 support`.
- Goal: add the symmetric runtime reflection boundary that writes a FieldDef token's inline `VALUE_SLOT` field value by using the same metadata runtime layout binding, owner layout field offset, and caller-provided inline storage range as the read boundary.
- Affected layers: core reflection API, metadata-runtime layout consumers, module reflection tests, and AOT plan documentation.
- Non-goals: raw POD/nested inline struct field marshaling, object-instance `FieldInfo.SetValue` method dispatch, cross-module FieldRef/TypeRef provider loading, trim dataflow, and metadata sweep.

## Baseline

- RED before implementation: `tests/module/test_reflection_token_resolve.c` called `ZrCore_Reflection_WriteFieldInfoTokenValue(...)`; Windows MSVC Debug failed at build/link with an undefined `ZrCore_Reflection_WriteFieldInfoTokenValue` symbol.
- The first WSL GCC RED command timed out before emitting the expected missing-symbol output because unrelated WSL build processes were still active in other build directories. The focused Windows RED preserved the intended missing-API failure.
- Existing baseline warnings remain outside this slice: GCC/Clang report generated-dispatch label-extension warnings, Clang reports the existing `reflection.c` `callerName` unused warning, and MSVC reports existing dispatch/object warnings.

## Test Inventory

- Focused positive case: a synthetic owner layout with one `ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT` field at byte offset `32` starts with int `11`, writes int `271828` through the FieldDef token, then reads the same slot back through `ZrCore_Reflection_ReadFieldInfoTokenValue(...)`.
- Failure-path cases: null state, null runtime, non-field token, null inline storage, null input value, and short inline storage all return `ZR_FALSE`.
- Boundary cases: read and write share the same owner-field resolver, requiring owner layout presence, matching offset/type-layout id, `VALUE_SLOT` flag, `sizeof(SZrTypeValue)` capacity, and `byteOffset + byteSize` inside caller-provided inline storage.
- Parent coverage: metadata runtime query and TypeSpec layout tests cover FieldDef row/layout views and token-to-layout resolver behavior reused by the reflection write boundary.

## Tooling Evidence

- Windows MSVC Debug RED:
  - `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test`
  - Failed with warning C4013 and LNK2019/LNK1120 for undefined `ZrCore_Reflection_WriteFieldInfoTokenValue`.
- Windows MSVC Debug GREEN:
  - `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test`
  - `.\build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_metadata_runtime_typespec_layout_test.exe`
  - `13 Tests 0 Failures 0 Ignored`; `24 Tests 0 Failures 0 Ignored`; `17 Tests 0 Failures 0 Ignored`
  - `ctest --test-dir build-msvc\tests -C Debug -R "^(reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout)$" --output-on-failure`
  - `100% tests passed, 0 tests failed out of 3`
- WSL GCC GREEN:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"`
  - `13/0`, `24/0`, `17/0`
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc/tests -R '^(reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout)$' --output-on-failure"`
  - `100% tests passed, 0 tests failed out of 3`
- WSL Clang GREEN:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_typespec_layout_test"`
  - `13/0`, `24/0`, `17/0`
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang/tests -R '^(reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout)$' --output-on-failure"`
  - `100% tests passed, 0 tests failed out of 3`

## Results

- Added `ZrCore_Reflection_WriteFieldInfoTokenValue(...)` to the public reflection API.
- Factored the FieldDef token + owner-layout `VALUE_SLOT` validation into a shared internal helper used by both read and write.
- The write boundary copies the caller-provided `SZrTypeValue` into the inline field slot with `ZrCore_Value_Copy(...)`.
- Unsupported storage shapes return `ZR_FALSE` instead of guessing a POD/nested marshaling policy.

## Acceptance Decision

Accepted for the `VALUE_SLOT` write boundary.

The slice is accepted because the original missing-API failure is fixed, the write path is covered with a read-back assertion and failure guards, and all focused lower-layer/parent tests passed across WSL GCC, WSL Clang, and Windows MSVC Debug.

Remaining risks are intentionally out of scope: raw POD/nested struct marshaling, object-level `FieldInfo.SetValue` dispatch, cross-module provider compatibility, `@dynamically_accessed` dataflow, DESCRIPTION promotion, and complete metadata sweep.
