# AOT 12-S7ZZK / 11-S7 Signature Token Orphan Rejection

## Scope

- Changed emitted zrp metadata pruning so a retained row or token record cannot keep a local `SIGNATURE` token that is missing from the retained signature token-record set.
- Added prune-failure cleanup so `backend_aot_c_prepare_embedded_zrp_metadata()` leaves output metadata cleared when pruning rejects an invalid retained signature token reference.
- Affected layers: AOT parser/codegen zrp metadata pruning, signature metadata helpers, focused pruning tests, source-contract tests, and AOT 11/12 plan records.

## Baseline

- Before this slice, `backend_aot_c_zrp_remap_retained_signature_token()` compacted matching retained signature records but accepted a missing local `SIGNATURE` token by returning success with the source token unchanged.
- A retained MethodSpec row could therefore survive pruning while its row `token` pointed to a `SIGNATURE` RID that no retained signature token record published.
- When pruning failed for any reason, `backend_aot_c_prepare_embedded_zrp_metadata()` could return false while `outMetadata->blob` and `outMetadata->length` still described the original source blob.
- Existing repository-level worktree state is dirty from ongoing AOT/LSP/build work; this acceptance record only covers the focused files for this slice.

## Test Inventory

- `tests/parser/test_aot_c_zrp_metadata_pruning.c`
  - Added a fixture with one retained MethodSpec row whose `token` is a local `SIGNATURE` RID but whose matching signature token record is absent.
  - Requires pruning to fail and leave the prepared embedded metadata output empty.
- `tests/parser/test_aot_c_source_contracts.c`
  - Locks the signature remap null-token-record guard and the prune-failure release path in `backend_aot_c_prepare_embedded_zrp_metadata()`.

## Tooling Evidence

- RED, WSL GCC:
  - `wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_pruning_test >/tmp/s7zzk_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_pruning_test'`
  - Observed first failure: 7 tests, 1 failure. The missing-signature-record fixture expected pruning to return false, but old code returned true.
  - Observed second failure after rejecting the orphan token: the same fixture expected output length 0 after failure, but `outMetadata->length` still reported the original source blob length 659.
- GREEN, WSL GCC:
  - Direct focused runs passed: zrp metadata pruning 7/0, typedef pruning 2/0, pool pruning 6/0, source contracts 24/0.
  - Focused metadata CTest selection `aot_c_zrp_metadata_(pruning|typespec_pruning|export_token_remap|pool_pruning)` passed 4/4.
- GREEN, WSL Clang:
  - Direct focused runs passed: zrp metadata pruning 7/0, typedef pruning 2/0, pool pruning 6/0, source contracts 24/0.
  - Focused metadata CTest selection passed 4/4.
- GREEN, Windows MSVC Debug:
  - Built focused targets with `cmake --build build-msvc --config Debug`.
  - Direct executable runs passed: zrp metadata pruning 7/0, typedef pruning 2/0, pool pruning 6/0, source contracts 24/0.

## Results

- `backend_aot_c_zrp_metadata_signature.c` now rejects local `SIGNATURE` tokens when no retained signature token-record list exists or when the source token is absent from that retained list.
- `backend_aot_c_zrp_metadata_prune.c` now releases prepared embedded zrp metadata output on prune failure before returning false.
- Retained MethodSpec rows and any future retained signature-token consumers now fail closed instead of preserving orphan source `SIGNATURE` RIDs.

## Acceptance Decision

- Accepted for the focused 12-S7ZZK / 11-S7 support slice.
- Remaining open: cross-module provider binding, real export manifest/table rewrite/publication, broader metadata sweep/pruning, annotation policy, full trim analyzer, and runtime ABI drift/deopt closure.
