# 12-S7ZZA / 11-S7 TypeDef Trailing Orphan Sweep

Timestamp: 2026-07-01 17:10:03 +08:00

## Scope

This support slice tightens emitted `.zrp` metadata pruning for TypeDef rows. The pruner now computes the retained
TypeDef prefix up to the last TypeDef row with a retained root, rewrites the TypeDef section layout, and lets trailing
orphan TypeDef rows plus their string/signature pool slices disappear from the after-trim embedded metadata blob.

This intentionally does not compact interior TypeDef RIDs or rewrite TypeDef token references. Interior holes remain
open for a later TypeDef RID compaction/remap slice.

## RED

`tests/parser/test_aot_c_zrp_metadata_pruning.c` added a fixture with one live TypeDef/MethodDef and one trailing
TypeDef with no retained token-record, method, field, or type-generic-param root. The old pruner left the metadata blob
at 580 bytes instead of the expected 510 bytes because it raw-copied the TypeDef section and retained the orphan type
strings.

## GREEN

`backend_aot_c_zrp_metadata_type_def.{h,c}` now identifies the last retained TypeDef root using retained token records,
retained MethodDef owners, FieldDef owners, and TypeDef-owned generic params. The prune header, TypeDef copy path,
string-pool remap, and signature-pool remap consume the retained TypeDef prefix count. Source contracts and Windows
manual test target source lists were updated to include the new helper module.

## Validation

- Windows MSVC Debug direct runs: `aot_c_zrp_metadata_pruning` 6/0, `aot_c_zrp_metadata_pool_pruning` 6/0,
  `aot_c_zrp_metadata_typespec_pruning` 2/0, `aot_c_zrp_metadata_export_token_remap` 3/0,
  `aot_c_zrp_metadata_size_deltas` 2/0, `aot_c_code_stripping` 10/0, `aot_c_source_contracts` 24/0,
  `aot_c_frame_setup_contracts` 1/0.
- Windows focused CTest: 6/6.
- WSL GCC focused build passed; focused CTest 6/6; source contracts 24/0; frame setup contracts 1/0.
- WSL Clang focused build passed; focused CTest 6/6; source contracts 24/0; frame setup contracts 1/0.
- `git diff --check` exited 0, with only the repository's existing LF/CRLF warnings.
