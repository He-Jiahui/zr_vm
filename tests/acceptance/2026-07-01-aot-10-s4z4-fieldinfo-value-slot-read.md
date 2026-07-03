# AOT 10-S4Z4 FieldInfo Value-Slot Read

## Scope

- Plan slices: `10-S4Z4 / 11-S4AP / 12-S5 support`.
- Goal: add a runtime reflection boundary that reads a FieldDef token's inline `VALUE_SLOT` field value by using the FieldInfo token, metadata runtime layout binding, and owner layout field offset.
- Affected layers: core reflection API, metadata-runtime layout consumers, module reflection tests, and AOT plan documentation.
- Non-goals: field write, raw POD/nested inline struct field marshaling, object-instance method surface, cross-module FieldRef/TypeRef provider loading, trim dataflow, and metadata sweep.

## Baseline

- RED before implementation: `tests/module/test_reflection_token_resolve.c` called `ZrCore_Reflection_ReadFieldInfoTokenValue(...)`; WSL GCC failed at build/link because the public API did not exist.
- Existing baseline warnings remain outside this slice: Clang/GCC report generated-dispatch label-extension warnings, and Clang reports the existing `reflection.c` `callerName` unused warning.

## Test Inventory

- Focused positive case: a synthetic owner layout with one `ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT` field at byte offset `32` stores an `SZrTypeValue` initialized to int `314159`; reading through the FieldDef token copies that value into the output slot.
- Failure-path cases: null state, null runtime, non-field token, and short inline storage all return `ZR_FALSE`.
- Boundary cases: the implementation resets `outValue` before attempting a read, checks owner layout presence, requires the matching offset/type-layout field, requires the `VALUE_SLOT` flag, and verifies `byteOffset + byteSize` stays inside caller-provided inline storage.
- Parent coverage: metadata runtime query and TypeSpec layout tests cover FieldDef row/layout views and token-to-layout resolver behavior reused by the reflection read boundary.

## Tooling Evidence

- WSL GCC RED:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j2 && ./build-wsl-gcc/tests/module/zr_vm_reflection_token_resolve_test"`
  - Build/link failed with missing `ZrCore_Reflection_ReadFieldInfoTokenValue`.
- WSL GCC GREEN:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j2 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test"`
  - `12 Tests 0 Failures 0 Ignored`
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test"`
  - `24 Tests 0 Failures 0 Ignored`; `17 Tests 0 Failures 0 Ignored`
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc/tests -R '^(reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout)$' --output-on-failure"`
  - `100% tests passed, 0 tests failed out of 3`
- WSL Clang GREEN:
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-clang --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j2 && ./build-wsl-clang/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-clang/bin/zr_vm_metadata_runtime_typespec_layout_test"`
  - `12/0`, `24/0`, `17/0`
  - `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-clang/tests -R '^(reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout)$' --output-on-failure"`
  - `100% tests passed, 0 tests failed out of 3`
- Windows MSVC Debug GREEN:
  - `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test`
  - `.\build-msvc\bin\Debug\zr_vm_reflection_token_resolve_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_metadata_runtime_query_test.exe`
  - `.\build-msvc\bin\Debug\zr_vm_metadata_runtime_typespec_layout_test.exe`
  - `ctest --test-dir build-msvc\tests -C Debug -R "^(reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout)$" --output-on-failure`
  - `12/0`, `24/0`, `17/0`, focused CTest `3/3`

## Results

- Added `ZrCore_Reflection_ReadFieldInfoTokenValue(...)` to the public reflection API.
- The implementation reuses `ZrCore_Reflection_ResolveToken(...)` and the existing FieldDef layout binding result, locates the owner `SZrTypeLayoutField` by offset and type-layout id, then copies the inline `SZrTypeValue` with `ZrCore_Value_Copy(...)`.
- Unsupported storage shapes return `ZR_FALSE` instead of guessing a marshaling policy.

## Acceptance Decision

Accepted for the `VALUE_SLOT` read-only boundary.

The slice is accepted because the original missing-API failure is fixed, the positive value-copy path and failure paths are covered, and all focused lower-layer/parent tests passed across WSL GCC, WSL Clang, and Windows MSVC Debug.

Remaining risks are intentionally out of scope: write support, raw POD/nested struct marshaling, object-level FieldInfo method dispatch, cross-module provider compatibility, `@dynamically_accessed` dataflow, DESCRIPTION promotion, and complete metadata sweep.
