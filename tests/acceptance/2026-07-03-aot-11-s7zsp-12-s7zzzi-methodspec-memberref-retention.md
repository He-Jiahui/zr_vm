# AOT 11-S7ZSP / 12-S7ZZZI MethodSpec MEMBER_REF Retention

## Scope

- Completed 2026-07-03 19:00 +08:00.
- Covers emitted `.zrp` metadata pruning for `MethodSpec.methodToken` rows that point at imported `MEMBER_REF` tokens.
- This is a focused 11-S7/12-S7 metadata sweep slice; it does not complete the full 07~12 goal.

## RED

- Added `test_aot_c_zrp_metadata_methodspec_pruning_keeps_imported_member_ref_method_token`.
- The fixture keeps an imported `MEMBER_REF` MethodSpec row while pruning an unrelated local MethodDef.
- Temporarily removing the `MEMBER_REF` branch made WSL GCC fail with `Expected 735 Was 711`, showing the old path dropped the MethodSpec row instead of preserving the imported method reference.

## GREEN

- `backend_aot_c_zrp_remap_method_spec_row` now preserves imported `MEMBER_REF` method tokens.
- Local `MEMBER_DEF` method tokens still require retained MethodDef remap and compacted RIDs.
- The focused test verifies the MethodSpec row, signature blob, and recomputed signature hash survive after MethodDef pruning.
- The malformed FieldDef-in-MethodSpec guard from the previous method-only member-token slice remains covered by the combined pruning suite.

## Validation

- WSL GCC: source contracts 24/0, export token remap 10/0, pruning 22/0, MethodSpec pruning 1/0.
- WSL clang: source contracts 24/0, export token remap 10/0, pruning 22/0, MethodSpec pruning 1/0.
- Windows MSVC Debug: source contracts 24/0, export token remap 10/0, pruning 22/0, MethodSpec pruning 1/0.
