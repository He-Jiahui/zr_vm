# AOT 11-S7ZSQ / 12-S7ZZZJ MethodSpec MEMBER_REF Token Record Guard

## Scope

- Completed 2026-07-03 19:29 +08:00.
- Covers emitted `.zrp` metadata pruning for `MethodSpec.methodToken` rows that point at imported `MEMBER_REF` tokens whose token record is missing after source metadata sweep inputs.
- This is a focused 11-S7/12-S7 metadata sweep slice; it does not complete the full 07~12 goal.

## RED

- Added `test_aot_c_zrp_metadata_methodspec_pruning_drops_orphan_imported_member_ref_method_token`.
- The fixture keeps a MethodSpec row and `SIGNATURE` token record that reference an imported `MEMBER_REF`, but omits the imported member token record itself.
- WSL GCC failed with `Expected 504 Was 639`, showing the old path kept the MethodSpec row, signature token record, and signature blob even though the imported method reference had no surviving token record.

## GREEN

- `backend_aot_c_zrp_remap_method_spec_row` now requires imported `MEMBER_REF` method tokens to have a matching token record.
- Retained token-record remap now rejects `MEMBER_REF` references in token/related/owner/target fields when the referenced imported member token record is absent.
- Signature remap, MethodSpec count, MethodSpec copy, and MethodSpec signature rewrite paths now share the same token-record guard.
- The focused test verifies orphan MethodSpec rows, their `SIGNATURE` token records, and their signature blobs are dropped while retained local MethodDefs still compact normally.

## Validation

- WSL GCC: source contracts 24/0, export token remap 10/0, pruning 22/0, MethodSpec pruning 2/0.
- WSL clang: source contracts 24/0, export token remap 10/0, pruning 22/0, MethodSpec pruning 2/0.
- Windows MSVC Debug: source contracts 24/0, export token remap 10/0, pruning 22/0, MethodSpec pruning 2/0.
