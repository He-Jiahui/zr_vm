# AOT 11-S7ZSN / 12-S7ZZZG TypeDef FieldDef Owner-token Retention

Date: 2026-07-03 07:42:47 +08:00

Status: completed focused refinement. This closes only the TypeDef token-record root guard for FieldDef `MEMBER_DEF` tokens in emitted `.zrp` metadata pruning; it does not claim full 11-S7, 12-S7, or 07~12 completion.

## Scope

- Added a pruning fixture where a dead TypeDef is referenced only by a token record whose `ownerToken` points at that dead TypeDef and whose member token points at a pruned FieldDef.
- Tightened TypeDef token-record root detection so FieldDef member tokens cannot reverse-retain their own owner TypeDef.
- Threaded TypeDef row context through the public TypeDef retention predicate callers that collect signature/string roots and compact TypeDef tokens.

## RED

- Command: `wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test'`
- Result: failed in `test_aot_c_zrp_metadata_pruning_drops_typedef_rooted_only_by_pruned_field_owner_token` with `Expected Non-NULL`.
- Cause: TypeDef root detection still used the legacy member-token remap path, so a token record involving a pruned FieldDef could keep the dead owner TypeDef and dead FieldDef, producing no compacted owned blob.

## GREEN

- `backend_aot_c_zrp_type_def_row_is_retained()` now receives the full TypeDef row set so root checks can validate retained member tokens with row context.
- TypeDef token-record root checks validate MethodDef member tokens against retained MethodDef rows.
- FieldDef member tokens in TypeDef-rooting records must resolve to retained FieldDef rows and cannot be used to retain the same TypeDef that owns that FieldDef.
- The new test now verifies the dead TypeDef/FieldDef pair is pruned while the live TypeDef and live FieldDef publish compacted tokens.

## Verification

- WSL GCC: source contracts 24/0, export token remap 10/0, pruning 21/0.
- WSL clang: source contracts 24/0, export token remap 10/0, pruning 21/0.
- Windows MSVC Debug: source contracts 24/0, export token remap 10/0, pruning 21/0.

## Remaining

- Complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, and broader ABI drift/deopt coverage remain open for the 07~12 goal.
