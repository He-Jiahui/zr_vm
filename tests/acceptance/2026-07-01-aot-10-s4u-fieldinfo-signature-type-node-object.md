# AOT 10-S4U FieldInfo Signature Type Node Object

## Scope
- Added the minimum public `FieldInfo.fieldTypeSignatureNodeObject` carrier.
- Affected layers: core reflection object materialization, metadata runtime signature-node consumers, and module-level focused tests.

## Baseline
- Before this slice, `FieldInfo` exposed flat signature type-node fields plus primitive, direct `TYPE_DEF`, bound `TYPE_REF`, and signature/layout consistency carriers.
- It did not expose those validated signature type-node fields as a nested object that later recursive semantic type binding can extend.
- Existing repository baseline warning remains: WSL Clang reports `reflection.c` `callerName` set but unused.

## Test Inventory
- Focused unit/subsystem: `tests/module/test_reflection_token_resolve.c`.
- Regression dependencies: `tests/module/test_metadata_runtime_query.c` and `tests/module/test_metadata_runtime_typespec_layout.c`.
- Boundary cases:
  - Primitive signature type `bool` exposes a `signatureTypeNode` object with node/blob/payload fields, type name `bool`, no token/layout, and `matchesLayout == false`.
  - Direct local `TYPE_DEF` signature exposes the same object shape with token `TEST_FIELD_TYPE_DEF_TOKEN`, layout id `42`, size `16`, type name `int`, and `matchesLayout == true`.
  - Current-runtime bound `TYPE_REF` signature exposes the TypeRef token identity, target layout id `42`, size `16`, type name `int`, and `matchesLayout == true`.
- Negative/non-goals:
  - This object is only a read-only carrier for the already validated top-level field type-node.
  - Recursive child objects, wrapper/generic argument expansion, field read/write, and cross-module provider loading remain out of scope.

## Tooling Evidence
- WSL GCC focused RED:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - Observed `10 Tests 3 Failures 0 Ignored`; all three FieldInfo signature scenarios failed with `Expected Non-NULL` because `fieldTypeSignatureNodeObject` was not written yet.
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
- Implementation now builds `fieldTypeSignatureNodeObject` from the existing validated top-level field type-node and mirrors the resolved token/layout/type-name/match fields without changing token/layout resolution.

## Acceptance Decision
- Accepted for 10-S4U / 11-S4AI / 12-S5 support.
- This only closes the read-only top-level signature type-node object carrier. It does not implement field value read/write, recursive wrapper/generic type-node materialization, cross-module provider loading, complete `FieldInfo` method surface, `@dynamically_accessed` dataflow, DESCRIPTION promotion, or complete metadata sweep.
