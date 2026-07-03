# AOT 12-S7ZZH / 11-S7 Member Token Remap Sidecar Token Shape Guard

时间：2026-07-01 19:57:11 +08:00

## Scope

- Changed feature: generated zrp metadata member-token remap sidecar construction.
- Affected layers: AOT parser/backend metadata pruning support, generated metadata sidecar validation, focused parser tests, source-contract guardrails, and AOT 11/12 plan records.
- This slice tightens generated-side validation before a `SZrAotMemberTokenRemap` table can be published into generated C.

## Baseline

- `12-S7ZZE` already made the runtime reject published remap ABI entries whose source or target token is not a nonzero `MEMBER_DEF`.
- `12-S7ZZG` already made the generated sidecar builder reject duplicate source tokens.
- The generated sidecar append path still accepted retained MethodDef or FieldDef rows with invalid source token shapes, such as `TYPE_DEF` or `MEMBER_DEF` with RID 0, and could therefore construct an invalid remap table before runtime descriptor validation.

完整 11-S7 / 12-S7 仍未关闭；cross-module provider binding、真实 export manifest/table rewrite/publication、完整 metadata sweep/pruning、完整 trim analyzer、annotation policy 和更完整的 runtime ABI drift/deopt coverage 仍待后续。

## Test Inventory

- Focused subsystem tests: `tests/parser/test_aot_c_zrp_metadata_export_token_remap.c`.
- Source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Boundary/failure cases:
  - retained MethodDef source token with `TYPE_DEF` table tag is rejected;
  - retained MethodDef source token with `MEMBER_DEF` table tag and RID 0 is rejected;
  - duplicate source token rejection from `12-S7ZZG` remains covered;
  - valid retained MethodDef and FieldDef source tokens still publish compacted member-token remaps.

## Tooling Evidence

- RED 1:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_zrp_metadata_export_token_remap_test && ./build-wsl-gcc/bin/zr_vm_aot_c_zrp_metadata_export_token_remap_test`
  failed with `test_aot_c_zrp_metadata_export_token_sidecar_rejects_non_member_source_tokens:FAIL: Expected FALSE Was TRUE`.
- RED 2:
  the same WSL GCC focused test then failed with
  `test_aot_c_zrp_metadata_export_token_sidecar_rejects_zero_rid_source_tokens:FAIL: Expected FALSE Was TRUE`.
- GREEN:
  WSL GCC direct export-token remap 6/0 and source contracts 24/0.
- GREEN:
  WSL Clang direct export-token remap 6/0 and source contracts 24/0.
- GREEN:
  Windows MSVC Debug direct export-token remap 6/0 and source contracts 24/0.
- Registered CTest:
  `aot_c_zrp_metadata_export_token_remap` passed 1/1 on WSL GCC, WSL Clang, and Windows MSVC Debug.

## Results

- `backend_aot_c_zrp_member_token_is_member_def()` now requires token nonzero, table tag `MEMBER_DEF`, and RID nonzero.
- `backend_aot_c_zrp_member_token_remap_append()` rejects invalid source or target member-token shapes before writing any entry.
- Failed sidecar builds free temporary entries and leave `memberTokenRemapEntries`, `memberTokenRemapCount`, and `ownedMemberTokenRemapEntries` empty.
- Source contracts lock the nonzero RID predicate, generated-side source/target shape checks, and the duplicate source guard.

## Acceptance Decision

Accepted for this support slice. Generated member-token remap sidecars now enforce the same basic nonzero `MEMBER_DEF` token-shape invariant before ABI publication that runtime validation already enforces after publication.

Remaining work is unchanged: cross-module provider target binding, real export manifest/table rewrite/publication, full metadata sweep/pruning, full trim analyzer, annotation policy, and broader ABI drift/deopt coverage.
