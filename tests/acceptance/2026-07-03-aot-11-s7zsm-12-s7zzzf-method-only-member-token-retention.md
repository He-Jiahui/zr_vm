# AOT 11-S7ZSM / 12-S7ZZZF Method-only Member-token Retention

Date: 2026-07-03 07:22:01 +08:00

Status: completed focused refinement. This closes only the MethodDef-only member-token guard for GenericParam owners and MethodSpec method tokens in emitted `.zrp` metadata pruning; it does not claim full 11-S7, 12-S7, or 07~12 completion.

## Scope

- Added a pruning fixture where a retained FieldDef survives after MethodDef pruning, while a GenericParam owner and a MethodSpec method token incorrectly point at that FieldDef `MEMBER_DEF` token.
- Tightened method-only member-token remapping so GenericParam `MEMBER_DEF` owners and MethodSpec `methodToken` values must resolve to retained MethodDef rows.
- Preserved normal FieldDef token-record and FieldDef row compaction behavior for retained fields.

## RED

- Command: `wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test'`
- Result: failed in `test_aot_c_zrp_metadata_pruning_drops_field_def_method_only_member_tokens` with `Expected TRUE Was FALSE`.
- Cause: legacy `backend_aot_c_zrp_remap_member_def_token()` accepted retained FieldDef tokens for MethodSpec/GenericParam method-only slots, so the malformed MethodSpec stayed retained and then failed the retained-signature-token requirement.

## GREEN

- Added static `backend_aot_c_zrp_remap_method_def_token()` in `backend_aot_c_zrp_metadata_remap.c`.
- `backend_aot_c_zrp_remap_method_spec_row()` now rejects non-MethodDef `MEMBER_DEF` tokens instead of accepting FieldDef rows.
- `backend_aot_c_zrp_remap_generic_param_owner_token()` still accepts TypeDef owners, but its `MEMBER_DEF` branch now requires a retained MethodDef owner.
- The new test now verifies MethodDef and FieldDef rows remain compacted, while the malformed GenericParam, MethodSpec, and MethodSpec signature blob are dropped.

## Verification

- WSL GCC: source contracts 24/0, export token remap 10/0, pruning 20/0.
- WSL clang: source contracts 24/0, export token remap 10/0, pruning 20/0 after rerunning with a longer timeout.
- Windows MSVC Debug: source contracts 24/0, export token remap 10/0, pruning 20/0.

## Remaining

- Complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, and broader ABI drift/deopt coverage remain open for the 07~12 goal.
