# AOT 11-S7ZSK / 12-S7ZZZD FieldDef Member-Token Retained-Row Guard

Date: 2026-07-03 06:24:32 +08:00
Status: Completed focused refinement

## Scope

- Refine emitted `.zrp` metadata pruning so `MEMBER_DEF` tokens that point at FieldDef rows are retained and compacted
  by the surviving FieldDef set, not by the raw source FieldDef index.
- Cover the case where a pruned FieldDef appears before a live FieldDef in the shared MethodDef/FieldDef member-token
  space, so dead field token records are dropped and live field tokens receive compacted retained RIDs.
- Keep manifest export member-token remapping on the same retained-row guard as token-record pruning.

## RED

- Command: `wsl -e bash -lc "cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test"`
- Result: `test_aot_c_zrp_metadata_pruning_drops_pruned_field_def_member_tokens_before_live_fields` failed with
  `Expected 640 Was 736`.
- Cause: the previous FieldDef member-token remapper checked only the source FieldDef index and compacted by that raw
  index, so a pruned FieldDef token record survived and a later retained FieldDef token could keep a stale RID.

## GREEN

- Added retained member-token remap wrappers for token records and manifest export rows.
- Updated the retained FieldDef path to require `backend_aot_c_zrp_field_def_row_is_retained()` and compact through
  `backend_aot_c_zrp_compacted_retained_field_def_token()`.
- Switched pruning count/copy paths and persistent manifest export remap to use the retained member-token wrappers.
- Expanded export-token remap tests so the sidecar builder receives the type/token/generic context required by retained
  FieldDef compaction.
- Updated MSVC test target sources for the focused export-token remap test so Windows shared-library builds link the
  retained FieldDef helper implementation and its metadata helper dependencies.

## Verification

- WSL GCC: `zr_vm_aot_c_zrp_metadata_pruning_test` passed 18/0;
  `zr_vm_aot_c_source_contracts_test` passed 24/0;
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` passed 10/0.
- WSL clang: `zr_vm_aot_c_zrp_metadata_pruning_test` passed 18/0;
  `zr_vm_aot_c_source_contracts_test` passed 24/0;
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` passed 10/0.
- Windows MSVC Debug: `zr_vm_aot_c_zrp_metadata_pruning_test` passed 18/0;
  `zr_vm_aot_c_source_contracts_test` passed 24/0;
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` passed 10/0.

## Remaining

- Complete metadata sweep/pruning is still open beyond this focused FieldDef token-retention guard.
- Full trim analyzer, annotation/promotion policy, and broader ABI drift/deopt closure remain open for later AOT 07~12
  slices.
