# AOT 12-S7ZZM / 11-S7 GenericParamConstraint TypeSpec Root Retention

- Timestamp: 2026-07-01 22:11:32 +08:00
- Status: GREEN for this support sub-slice. The broader 07~12 goal remains open.

## Scope

Retained `GenericParamConstraint.constraintTypeToken` values can now act as TypeSpec retention roots even when the
referenced TypeSpec row has no retained `TYPE_SPEC` token record of its own. The pruner keeps the row, compacts its RID,
remaps the constraint token, and retains the matching TypeSpec signature blob.

## RED

Added `test_aot_c_zrp_metadata_pruning_keeps_typespec_referenced_only_by_generic_param_constraint`.

The fixture keeps a generic method parameter and its constraint, removes an earlier method and orphan TypeSpec, and gives
the retained constraint the only live reference to source `TYPE_SPEC` RID2. The old pruner dropped that TypeSpec row and
`backend_aot_c_prepare_embedded_zrp_metadata()` failed:

- WSL GCC direct `zr_vm_aot_c_zrp_metadata_pruning_test`: 9 tests / 1 failure
- Failure: `Expected TRUE Was FALSE`

## GREEN

`backend_aot_c_zrp_metadata_type_spec.{h,c}` now threads GenericParam/GenericParamConstraint rows through TypeSpec
retention, compaction, remap, count, and copy helpers. A retained constraint row whose owner survives can retain its
TypeSpec constraint row before RID compaction. Signature blob remap also uses the same root context, so the kept TypeSpec
signature payload is copied and offset-remapped.

The source contract now locks the TypeSpec retention dependency on
`backend_aot_c_zrp_remap_generic_param_constraint_row(&constraintRow, ...)`.

## Verification

- WSL GCC direct: zrp metadata pruning 9/0, TypeSpec pruning 2/0, pool pruning 6/0, export token remap 8/0, source contracts 24/0
- WSL GCC focused metadata CTest: 4/4
- WSL Clang direct: zrp metadata pruning 9/0, TypeSpec pruning 2/0, pool pruning 6/0, export token remap 8/0, source contracts 24/0
- WSL Clang focused metadata CTest: 4/4
- Windows MSVC Debug direct: zrp metadata pruning 9/0, TypeSpec pruning 2/0, pool pruning 6/0, export token remap 8/0, source contracts 24/0
- Windows MSVC Debug focused metadata CTest: 4/4
- `git diff --check` focused files: no whitespace errors; only repository CRLF normalization warnings

## Remaining

This closes only GenericParamConstraint-rooted TypeSpec row/signature retention. Cross-module target/provider binding,
real export manifest/table rewrite/publication, complete metadata sweep, annotation policy, full trim analyzer, and
broader runtime ABI drift/deopt coverage remain open.
