# AOT 12-S7ZZB / 11-S7 TypeDef RID Compaction

- Completion time: 2026-07-01 17:44:55 +08:00
- Status: completed support sub-slice for compacting retained TypeDef rows and direct TypeDef token references after
  emitted `.zrp` metadata pruning.

## Scope

This slice follows the TypeDef trailing orphan sweep by allowing retained TypeDef rows to move across interior holes.
When earlier TypeDef rows are pruned, retained TypeDef rows now publish compacted `TYPE_DEF` RIDs, token records are
rewritten to the compacted TypeDef token, and direct row-owner references are remapped for MethodDef, FieldDef,
GenericParam, and GenericParamConstraint rows.

String-pool and signature-pool collection now use actual TypeDef retention instead of a retained prefix, so strings and
signature slices from orphan TypeDef rows no longer keep the after-trim metadata blob in prefix shape.

This does not close cross-module export manifest/table publication rewrite, cross-module target/provider binding,
embedded TypeDef tokens inside retained signature blob payloads, complete metadata sweep/pruning, complete trim analyzer,
annotation-driven warning policy, or runtime ABI drift deopt coverage.

## RED

- `tests/parser/test_aot_c_zrp_metadata_typedef_pruning.c` added an interior-orphan fixture where source TypeDef RID1 is
  orphaned and source TypeDef RID2 survives through a retained MethodDef and retained token records.
- The focused Windows test expected the live TypeDef to publish compacted `TYPE_DEF` RID1, token-record owner fields and
  MethodDef.ownerTypeToken to reference that compacted token, and the string pool to drop the orphan type strings.
- The old retained-prefix model identity-exited and returned no owned pruned blob, so the RED failed with
  `Expected Non-NULL`.

## GREEN

- `backend_aot_c_zrp_metadata_type_def.{h,c}` now counts retained TypeDef rows instead of a retained prefix and exposes
  compacted TypeDef token/remap/copy helpers.
- Token-record count/copy remaps direct TypeDef tokens after member-token pruning and before TypeSpec/ModuleRef remap.
- MethodDef.ownerTypeToken, FieldDef.ownerTypeToken, GenericParam.ownerToken, and
  GenericParamConstraint.constraintTypeToken are remapped when they carry direct TypeDef tokens.
- TypeDef copy now writes only retained rows and assigns compacted row tokens; string/signature pool remap uses the same
  retained-row predicate.

## Validation

- Windows MSVC Debug direct runs passed:
  - `zr_vm_aot_c_zrp_metadata_typedef_pruning_test.exe` 1/0
  - `zr_vm_aot_c_zrp_metadata_pruning_test.exe` 6/0
  - `zr_vm_aot_c_zrp_metadata_typespec_pruning_test.exe` 2/0
  - `zr_vm_aot_c_zrp_metadata_pool_pruning_test.exe` 6/0
  - `zr_vm_aot_c_source_contracts_test.exe` 24/0
- WSL GCC configured, built, and direct-ran the same five focused targets: 1/0, 6/0, 2/0, 6/0, and 24/0.
- WSL Clang configured, built, and direct-ran the same five focused targets: 1/0, 6/0, 2/0, 6/0, and 24/0.
- `git diff --check` exited 0, with only the repository's existing LF/CRLF warnings.
