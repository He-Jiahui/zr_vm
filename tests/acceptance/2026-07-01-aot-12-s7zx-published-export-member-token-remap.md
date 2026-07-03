# AOT 12-S7ZX / 11-S7 Published Export Member-Token Remap

- Completion time: 2026-07-01 15:57:26 +08:00
- Status: completed support sub-slice for publishing compacted MethodDef and FieldDef member tokens after emitted
  `.zrp` metadata pruning.

## Scope

This slice adds a retained-member token remap sidecar to the emitted `.zrp` metadata pruning result. After non-identity
pruning, retained MethodDef and FieldDef rows publish a `sourceToken -> targetToken` mapping that uses the compacted
row positions. The generated AOT C method-token table now consults that sidecar before emitting typed exported method
tokens, so a retained MethodDef whose source RID shifts during pruning no longer publishes a stale pre-prune RID.

This does not close the full cross-module export manifest/table publication rewrite, TypeSpec RID compaction,
cross-module target/provider binding, complete metadata sweep/pruning, complete trim analyzer, annotation-driven
warning policy, or runtime ABI drift deopt coverage.

## RED

- `tests/parser/test_aot_c_zrp_metadata_export_token_remap.c` included the planned
  `backend_aot_c_zrp_metadata_member_token.h` helper and expected a retained MethodDef token to remap from RID2 to
  compacted RID1.
- The focused Windows build failed because the helper did not exist.
- `tests/parser/test_aot_c_code_stripping.c` now locks generated C output so a typed exported method attached to a
  retained RID2 MethodDef emits `0x03000001u` instead of stale `0x03000002u`.

## GREEN

- `backend_aot_c_zrp_metadata_member_token.{h,c}` builds, looks up, and destroys the retained MethodDef/FieldDef
  member-token sidecar.
- `SZrAotCEmbeddedZrpMetadata` carries the sidecar for non-identity pruning results, and pruning releases the owned
  sidecar with the embedded metadata.
- `backend_aot_c_method_metadata.c` remaps typed exported method tokens through the sidecar and emits `0u` when a
  pruned member token is not retained.
- Source and frame setup contracts lock the new helper boundary and the emitter-to-method-token-table call shape.

## Validation

- Windows MSVC Debug direct runs passed:
  - `zr_vm_aot_c_zrp_metadata_export_token_remap_test.exe` 3/0
  - `zr_vm_aot_c_zrp_metadata_pruning_test.exe` 5/0
  - `zr_vm_aot_c_zrp_metadata_typespec_pruning_test.exe` 1/0
  - `zr_vm_aot_c_zrp_metadata_pool_pruning_test.exe` 5/0
  - `zr_vm_aot_c_source_contracts_test.exe` 24/0
  - `zr_vm_aot_c_frame_setup_contracts_test.exe` 1/0
  - `zr_vm_aot_c_code_stripping_test.exe` 10/0
- Windows MSVC Debug focused CTest selection passed 6/6 for the generated/pruned metadata set.
- WSL GCC and WSL Clang built the focused targets. Their focused CTest selections passed 6/6, and explicit direct runs
  passed `zr_vm_aot_c_source_contracts_test` 24/0 plus `zr_vm_aot_c_frame_setup_contracts_test` 1/0.
