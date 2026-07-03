# AOT 12-S7ZZC / 11-S7 Signature TypeDef Token Rewrite

- Completion time: 2026-07-01 18:09:50 +08:00
- Status: completed support sub-slice for rewriting direct `TYPE_DEF` tokens embedded inside retained signature blob
  payloads after emitted `.zrp` metadata pruning compacts TypeDef RIDs.

## Scope

This slice follows TypeDef RID compaction. Retained signature blob slices are still copied through the compacted
signature blob pool, but their internal direct `TYPE_DEF` type nodes now receive the same compacted TypeDef token
rewrite as token records and direct definition-row owner fields.

The rewrite walks validated `METHOD_SIG`, `FIELD_SIG`, and standalone type-node signature blobs, recursively handles
nested arrays, tuples, generic instances, ownership, union, and nullable nodes, rewrites only direct `TYPE_DEF` node
tokens, and validates the final blob before later row/token hash recomputation observes it.

This does not close `TYPE_REF` cross-module/provider rebinding, cross-module export manifest/table publication,
annotation-driven metadata policy, the full metadata sweep, or the full trim analyzer.

## RED

- `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c` added a retained FieldDef whose `FIELD_SIG` payload embeds a
  direct `TYPE_DEF` token for source TypeDef RID2 while source TypeDef RID1 is orphaned.
- The focused WSL GCC run built the target and failed because the copied signature blob still contained source RID2:
  `Expected 33554433 Was 33554434`.

## GREEN

- `backend_aot_c_zrp_metadata_signature.{h,c}` now exposes and implements retained signature TypeDef token rewriting.
- `backend_aot_c_zrp_metadata_prune.c` invokes that rewrite after copying compacted pools and before MethodSpec
  signature method-token rewriting / row copy, so token record signature hashes are recomputed from final bytes.
- The new recursive walker preserves non-TypeDef token payloads, rejects malformed or unretained direct TypeDef tokens,
  and keeps the existing MethodSpec `GENERIC_INST(MEMBER_REF methodToken, args...)` rewrite path intact.

## Validation

- RED: WSL GCC direct run of `zr_vm_aot_c_zrp_metadata_typedef_pruning_test` failed 1/2 with stale embedded TypeDef RID2.
- GREEN direct runs:
  - WSL GCC `zr_vm_aot_c_zrp_metadata_typedef_pruning_test`: 2/0.
  - WSL Clang `zr_vm_aot_c_zrp_metadata_typedef_pruning_test`: 2/0.
  - Windows MSVC Debug `zr_vm_aot_c_zrp_metadata_typedef_pruning_test.exe`: 2/0.
- Focused metadata/AOT CTest after rebuilding stale targets:
  - WSL GCC `zrp_metadata|aot_c_zrp_metadata|metadata_module_hash`: 8/8.
  - WSL Clang `zrp_metadata|aot_c_zrp_metadata|metadata_module_hash`: 8/8.
  - Windows MSVC Debug `zrp_metadata|aot_c_zrp_metadata|metadata_module_hash`: 8/8.
