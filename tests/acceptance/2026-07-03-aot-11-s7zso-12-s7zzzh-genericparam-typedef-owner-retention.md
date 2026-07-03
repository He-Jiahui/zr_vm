# AOT 11-S7ZSO / 12-S7ZZZH GenericParam TypeDef Owner Retention

## Scope

- Completed 2026-07-03 08:13 +08:00.
- Covers emitted `.zrp` metadata pruning for `GenericParam.ownerToken` rows that point at `TYPE_DEF` tokens.
- This is a focused 11-S7/12-S7 metadata sweep slice; it does not complete the full 07~12 goal.

## RED

- Added `test_aot_c_zrp_metadata_pruning_drops_generic_params_owned_by_pruned_type_defs`.
- WSL GCC initially failed with `Expected Non-NULL` in that test, showing the old GenericParam TypeDef owner check could keep a dead TypeDef/self-root path and avoid producing the expected compacted blob.

## GREEN

- `backend_aot_c_zrp_remap_generic_param_owner_token` now validates and remaps `TYPE_DEF` owners through retained TypeDef remap instead of accepting any TypeDef token.
- GenericParam retention/count/range and GenericParamConstraint remap/count/range paths now carry TypeDef/token-record/generic-constraint context.
- TypeDef retention no longer treats a TypeDef-owned GenericParam as an independent root for the same TypeDef.
- String-pool, signature, TypeSpec and prune copy paths now use the retained-row-aware GenericParam helpers.

## Validation

- WSL GCC: source contracts 24/0, export token remap 10/0, pruning 22/0.
- WSL clang: source contracts 24/0, export token remap 10/0, pruning 22/0.
- Windows MSVC Debug: source contracts 24/0, export token remap 10/0, pruning 22/0.
