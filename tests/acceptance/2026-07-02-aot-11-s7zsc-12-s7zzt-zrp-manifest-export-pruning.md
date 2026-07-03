# AOT 11-S7ZSC / 12-S7ZZT `.zrp` Manifest Export Pruning Rewrite

Date: 2026-07-02 10:42:54 +08:00

Status: completed existing-row pruning rewrite support slice. Writer-side generation/publication of persistent manifest
export rows, full metadata sweep/pruning, and full trim analysis remain open.

## Scope

- Emitted `.zrp` metadata pruning now treats the `manifestExports` section as structured rows instead of raw bytes.
- Existing manifest export rows have `targetStringOffset` rewritten through the compacted string pool.
- Method and field exports rewrite `memberToken` through the retained member-token map; type exports rewrite `typeToken`
  through TypeDef token compaction.
- Invalid kind/flag/token shapes, missing target strings, and missing compacted token remaps fail closed.

## RED

- WSL GCC focused `zr_vm_aot_c_zrp_metadata_pruning_test` failed 11/1 after adding a fixture with one removed MethodDef,
  one retained MethodDef, one FieldDef, and two persistent manifest export rows.
- The old pruner raw-copied the `manifestExports` section, so the retained method export still pointed at source string
  offset 25 instead of compacted offset 17.

## GREEN

- Added `backend_aot_c_zrp_metadata_manifest_export.{h,c}` for manifest export section copy/rewrite.
- Exposed the shared string-offset remapper from the string-pool pruning module.
- Wired `backend_aot_c_zrp_metadata_prune.c` to copy `manifestExports` through the helper and to clean up prepared
  remaps/blobs on failure.
- Updated Windows shared-DLL focused test source lists so MSVC links the new helper.

## Verification

- WSL GCC direct: `zr_vm_aot_c_zrp_metadata_pruning_test` 11/0,
  `zr_vm_aot_c_zrp_metadata_export_token_remap_test` 10/0, `zr_vm_aot_c_code_stripping_test` 10/0.
- WSL clang direct: same focused set passed 11/0, 10/0, and 10/0.
- Windows MSVC Debug direct: same focused set passed 11/0, 10/0, and 10/0.

## Notes

This record proves pruning/rewrite of existing persistent `.zrp` manifest export rows only. It does not claim generated
AOT output already writes declaration-derived manifest export rows into the persistent `.zrp` section.
