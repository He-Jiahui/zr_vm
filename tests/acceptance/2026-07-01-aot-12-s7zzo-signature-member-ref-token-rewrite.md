# AOT 12-S7ZZO / 11-S7 Signature MemberRef Token Rewrite

- Timestamp: 2026-07-01 22:51:15 +08:00
- Status: GREEN for this support sub-slice. The broader 07~12 goal remains open.

## Scope

Retained zrp signature blobs now rewrite embedded `MEMBER_REF` local member tokens after MethodDef/FieldDef pruning and
RID compaction. When a retained MethodDef moves from source RID2 to compacted RID1, retained signature payloads that
reference that member now publish the compacted token instead of the stale source RID.

## RED

Added `test_aot_c_zrp_metadata_pruning_rewrites_signature_member_ref_tokens`.

The fixture retains MethodDef source RID2 while MethodDef source RID1 is pruned. The retained MethodDef owns a
signature blob whose root node is `MEMBER_REF` and whose embedded token still points at source RID2. The old signature
rewrite skipped `MEMBER_REF` nodes:

- WSL GCC direct `zr_vm_aot_c_zrp_metadata_pruning_test`: 10 tests / 1 failure
- Failure: `Expected 50331649 Was 50331650`

## GREEN

`backend_aot_c_zrp_metadata_signature.c` now rewrites `MEMBER_REF` signature nodes by passing the embedded token through
the existing retained MethodDef/FieldDef token-record remap path, then writing the compacted token back before signature
validation and hash recomputation.

The source contract now locks the `backend_aot_c_zrp_rewrite_signature_member_ref_token()` helper and its
`backend_aot_c_zrp_remap_token_record(&record, ...)` call.

## Verification

- WSL GCC direct: zrp metadata pruning 10/0, TypeSpec pruning 2/0, pool pruning 7/0, export token remap 8/0, source contracts 24/0
- WSL GCC focused metadata CTest: 4/4
- WSL Clang direct: zrp metadata pruning 10/0, TypeSpec pruning 2/0, pool pruning 7/0, export token remap 8/0, source contracts 24/0
- WSL Clang focused metadata CTest: 4/4
- Windows MSVC Debug direct: zrp metadata pruning 10/0, TypeSpec pruning 2/0, pool pruning 7/0, export token remap 8/0, source contracts 24/0
- Windows MSVC Debug focused metadata CTest: 4/4
- Focused `git diff --check`: no whitespace errors; only repository CRLF normalization warnings.

## Remaining

This closes only retained signature-blob `MEMBER_REF` local member-token compaction after MethodDef/FieldDef pruning.
Cross-module target/provider binding, real export manifest/table rewrite/publication, complete metadata sweep,
annotation policy, full trim analyzer, and broader runtime ABI drift/deopt coverage remain open.
