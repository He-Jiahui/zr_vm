# AOT 10-S4X FieldInfo Signature Primitive Child Node Semantic Name

## Scope
- AOT 10-S4 / 11-S4 / 12-S5 support slice for minimum FieldDef token `FieldInfo` reflection.
- Affected layers: runtime reflection object materialization, metadata runtime signature-node consumption, and AOT trim support documentation.
- Changed behavior: recursive signature type-node objects now expose a builtin semantic `typeName` for primitive child nodes, starting with `PRIMITIVE(INT64)` generic arguments.

## Baseline
- Previous slice 10-S4W / 11-S4AK exposed `fieldTypeSignatureNodeObject.childNodeObjects` as a structural list only.
- In `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64))`, the child object for `PRIMITIVE(INT64)` carried node/blob/payload summary but `typeName` was null.
- Existing repository baseline remains broad and dirty; this acceptance only claims the focused reflection/metadata runtime targets listed below.

## Test Inventory
- Unit/focused subsystem:
  - `tests/module/test_reflection_token_resolve.c`
    - `test_reflection_builds_field_info_signature_generic_base_type_node_object`
    - Asserts `fieldTypeSignatureNodeObject.childNodeObjects[0].typeName == "int"` for the primitive generic argument.
- Adjacent metadata/runtime regression:
  - `tests/module/test_metadata_runtime_query.c`
  - `tests/module/test_metadata_runtime_typespec_layout.c`
- Boundary and remaining cases:
  - Covered: primitive child semantic name for `ZR_VALUE_TYPE_INT64`.
  - Still pending: direct `TYPE_DEF` / `TYPE_REF` child semantic token/layout binding, cross-module provider loading, recursive field type materialization, field value read/write, complete `FieldInfo` methods, dataflow analysis, and full metadata sweep.
- Negative path:
  - Non-primitive child nodes still keep empty token/layout/name unless a later resolver supplies semantic binding.

## Tooling Evidence
- RED, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test'`
  - Result: 11 tests, 1 failure. The new primitive child `typeName` assertion failed with `Expected 12 Was 0`, proving the string field was missing.
- GREEN, WSL GCC:
  - Same focused target after implementation.
  - Result: `zr_vm_reflection_token_resolve_test` 11/0.
- Focused WSL GCC matrix:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test -j 8 && ./build-wsl-gcc/bin/zr_vm_reflection_token_resolve_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_query_test && ./build-wsl-gcc/bin/zr_vm_metadata_runtime_typespec_layout_test && ctest --test-dir build-wsl-gcc -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure'`
  - Result: reflection 11/0, metadata query 24/0, TypeSpec layout 17/0, CTest 3/3.
- Focused WSL Clang matrix:
  - Same command against `build-wsl-clang`.
  - Result: reflection 11/0, metadata query 24/0, TypeSpec layout 17/0, CTest 3/3. Existing `reflection.c` `callerName` unused warning remains.
- Focused Windows MSVC Debug matrix:
  - `cmake --build build-msvc --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --config Debug -- /m`
  - Then ran the three Debug test binaries and focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`.
  - Result: reflection 11/0, metadata query 24/0, TypeSpec layout 17/0, CTest 3/3.

## Results
- `reflection_build_signature_type_node_object_internal()` now computes an effective `typeName`.
- When the caller supplied no name and the node is `ZR_METADATA_SIGNATURE_NODE_PRIMITIVE`, it reuses `reflection_builtin_type_name()`.
- `PRIMITIVE(INT64)` child nodes now expose `typeName == "int"` while preserving empty token/layout carriers.
- No metadata ABI, TypeRef resolver, or code stripping root rule changed in this slice.

## Acceptance Decision
- Accepted for 10-S4X / 11-S4AL / 12-S5 support primitive child semantic name carrier.
- The evidence covers the changed reflection object shape across WSL GCC, WSL Clang, and Windows MSVC Debug.
- Remaining work is explicitly outside this acceptance: direct TypeDef/TypeRef child semantic token/layout binding, recursive field type binding, cross-module provider compatibility, full FieldInfo behavior, dataflow analysis, and metadata sweep.
