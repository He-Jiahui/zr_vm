# AOT 10-S4T FieldInfo Signature Layout Consistency

## Scope
- Added the minimum public `FieldInfo.fieldTypeSignatureMatchesLayout` carrier.
- Affected layers: core reflection object materialization, metadata runtime layout consumers, and module-level focused tests.

## Baseline
- Before this slice, `FieldInfo` exposed signature-derived primitive, direct `TYPE_DEF`, and bound `TYPE_REF` type carriers.
- It did not expose whether the signature-derived type layout matched the FieldDef layout single truth.
- Existing repository baseline warning remains: WSL Clang reports `reflection.c` `callerName` set but unused.

## Test Inventory
- Focused unit/subsystem: `tests/module/test_reflection_token_resolve.c`.
- Regression dependencies: `tests/module/test_metadata_runtime_query.c` and `tests/module/test_metadata_runtime_typespec_layout.c`.
- Boundary cases:
  - Primitive signature type `bool` with FieldDef layout-derived type `int` reports `fieldTypeSignatureMatchesLayout == false`.
  - Direct local `TYPE_DEF` signature resolving to layout id `42` reports `true`.
  - Current-runtime bound `TYPE_REF` resolving through the target TypeDef layout id `42` reports `true`.
- Negative cases:
  - Missing semantic signature layout or mismatched signature/layout identity does not report a match.

## Tooling Evidence
- WSL GCC focused RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - Observed `10 Tests 3 Failures 0 Ignored`; all three failures were `Expected Non-NULL` at the new bool field helper because `fieldTypeSignatureMatchesLayout` was not written yet.
- WSL GCC focused GREEN/regression:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-gcc -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'`
- WSL Clang focused regression:
  - Same command against `build-wsl-clang`.
- Windows MSVC Debug focused regression:
  - `cmake --build build-msvc --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8; ...; ctest --test-dir build-msvc -C Debug -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure`

## Results
- WSL GCC passed `reflection_token_resolve` 10/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0, and focused CTest 3/3.
- WSL Clang passed the same counts and focused CTest 3/3. Existing `callerName` unused warning remains.
- Windows MSVC Debug passed the same counts and focused CTest 3/3.
- Implementation now sets `fieldTypeSignatureMatchesLayout` only when the signature-derived layout is present, the FieldDef layout is present, the layout id is non-none, and both the layout id and registry layout pointer match.

## Acceptance Decision
- Accepted for 10-S4T / 11-S4AH / 12-S5 support.
- This only closes the read-only signature/layout consistency carrier. It does not implement field value read/write, recursive wrapper/generic type-node materialization, cross-module provider loading, complete `FieldInfo` method surface, `@dynamically_accessed` dataflow, DESCRIPTION promotion, or complete metadata sweep.
