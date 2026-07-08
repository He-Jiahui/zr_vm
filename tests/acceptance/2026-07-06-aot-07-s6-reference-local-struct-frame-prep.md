# AOT 07-S6 reference-local struct frame preparation

Timestamp: 2026-07-06 06:19:50 +08:00

## Scope

This slice reshapes generated conversion reference locals from independent `SZrRawObject *zr_aot_oN` variables into a per-function reference-local frame:

- file-scope `SZrAotReferenceLocals_<flatIndex>` typedefs contain one `SZrRawObject *oN` field per `TO_STRING` / `TO_OBJECT` destination
- each generated function with reference locals instantiates `SZrAotReferenceLocals_<flatIndex> zr_aot_ref_locals = { ZR_NULL };`
- `TO_STRING` / `TO_OBJECT` conversion writeback and immediate string/object truthiness now use `zr_aot_ref_locals.oN`

This prepares the generated shape for later `offsetof(SZrAotReferenceLocals_<flatIndex>, oN)` root-map entries. It does not emit generated `LOCAL_ADDRESS` root maps or push a separate local-address root frame.

## RED

The focused generated-C smokes and source contracts were strengthened to require:

- `typedef struct SZrAotReferenceLocals_0`
- `SZrRawObject *o2;`
- `SZrAotReferenceLocals_0 zr_aot_ref_locals = { ZR_NULL };`
- conversion assignments to `zr_aot_ref_locals.o2`
- string casts through `ZR_CAST_STRING(state, zr_aot_ref_locals.o2)`
- object truthiness from `zr_aot_ref_locals.o2 != ZR_NULL`
- absence of `SZrRawObject *zr_aot_o2 = ZR_NULL;`
- source-level emitter wiring for `backend_aot_write_c_reference_local_structs(file, &functionTable);`

Initial WSL GCC direct results after rebuilding the updated tests:

- `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8 tests / 2 failures
- `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9 tests / 2 failures
- `zr_vm_aot_c_logical_contracts_test`: 4 tests / 1 failure
- `zr_vm_aot_c_source_contracts_test`: 24 tests / 1 failure

## GREEN

`backend_aot_c_reference_locals.c` now scans `TO_STRING` / `TO_OBJECT` destinations once per emitted function and writes:

- `typedef struct SZrAotReferenceLocals_<flatIndex> { ... } SZrAotReferenceLocals_<flatIndex>;`
- one `SZrRawObject *oN;` field per unique destination slot
- a function-local `zr_aot_ref_locals` instance when the function has reference locals

The top-level C emitter writes the reference-local typedefs after function forward declarations and before later file-scope metadata/thunk sections. Conversion lowering writes validated runtime conversion results into `zr_aot_ref_locals.oN`, and immediate string/object truthiness reads the same field instead of the old bare local.

## Verification

- WSL GCC:
  - build: focused smoke and contract targets passed
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
  - `zr_vm_aot_c_logical_contracts_test`: 4/0
  - `zr_vm_aot_c_source_contracts_test`: 24/0
- WSL Clang:
  - build: focused smoke and contract targets passed
  - `zr_vm_aot_c_generic_logical_not_numeric_local_smoke_test`: 8/0
  - `zr_vm_aot_c_generic_jump_if_bool_local_smoke_test`: 9/0
  - `zr_vm_aot_c_logical_contracts_test`: 4/0
  - `zr_vm_aot_c_source_contracts_test`: 24/0
- Windows MSVC Debug:
  - build: focused smoke and contract targets passed
  - logical-not smoke: 8 expected Unix-only ignores / 0 failures
  - jump-if smoke: 9 expected Unix-only ignores / 0 failures
  - `zr_vm_aot_c_logical_contracts_test`: 4/0
  - `zr_vm_aot_c_source_contracts_test`: 24/0
- Formatting:
  - scoped `git diff --check` exited 0 with only existing LF/CRLF warnings
  - new reference-local files have no trailing whitespace

## Open Items

- Generated `LOCAL_ADDRESS` root-map emission for `zr_aot_ref_locals.oN`.
- A separate local-address root frame or ABI extension; the current AOT root-frame push has a single base and cannot safely mix VM stack byte roots with C-local address roots.
- Broader GC pressure/root-correctness stress.
- Exports/frame cleanup, in/out writeback, performance counters, and complete 07-S6/07~12 acceptance.

## Large-File Note

`backend_aot_c_emitter.c` is 1275 lines after this slice; the edit there is limited to one include and one file-scope structure emission call. The larger lowering files were touched only to replace the already-localized conversion/truthiness expressions. The new `backend_aot_c_reference_locals.c` module stays small and keeps this responsibility out of `backend_aot_c_scalar_locals.c`.
