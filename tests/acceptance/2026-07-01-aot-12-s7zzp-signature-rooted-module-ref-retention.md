# AOT 12-S7ZZP / 11-S7 Signature-Rooted ModuleRef Retention

- Timestamp: 2026-07-01 23:36:12 +08:00
- Status: GREEN for this support sub-slice. The broader 07~12 goal remains open.

## Scope

Retained zrp signature blobs now act as live roots for ModuleRef rows when they contain `ASSEMBLY_REF` payloads. A
ModuleRef referenced only by a retained signature blob, without any retained `TYPE_REF` or `MEMBER_REF` token-record
root, now keeps its row plus module name and requested/min/max version strings through pruning and compaction.

## RED

Added `test_aot_c_zrp_metadata_pool_pruning_retains_module_refs_rooted_only_by_signature_blobs`.

The fixture retains a signature blob whose root is `FIELD_SIG(ASSEMBLY_REF source RID2)` while no retained import token
record references source RID2. The old ModuleRef retention path only looked at retained `TYPE_REF`/`MEMBER_REF`
token-record roots, so the row was pruned before signature rewrite:

- WSL GCC direct `zr_vm_aot_c_zrp_metadata_pool_pruning_test`: 8 tests / 1 failure
- Failure: `Expected TRUE Was FALSE`

## GREEN

`backend_aot_c_zrp_metadata_prune.c` now builds the signature remap before counting retained ModuleRefs and building
the string-pool remap. `backend_aot_c_zrp_metadata_module_ref.c` scans retained signature blobs for
`ZR_METADATA_SIGNATURE_NODE_ASSEMBLY_REF`, and the same source signature context is threaded into ModuleRef retention,
string-pool retention, and signature rewrite. The retained signature payload is then rewritten to the compacted
AssemblyRef RID.

The source contract locks the retained signature-blob AssemblyRef scanner and the signature header include used by the
ModuleRef pruning helper.

## Verification

- WSL GCC direct: zrp metadata pruning 10/0, TypeSpec pruning 2/0, pool pruning 8/0, export token remap 8/0, source contracts 24/0
- WSL GCC focused metadata CTest: 4/4
- WSL Clang direct: zrp metadata pruning 10/0, TypeSpec pruning 2/0, pool pruning 8/0, export token remap 8/0, source contracts 24/0
- WSL Clang focused metadata CTest: 4/4
- Windows MSVC Debug direct: zrp metadata pruning 10/0, TypeSpec pruning 2/0, pool pruning 8/0, export token remap 8/0, source contracts 24/0
- Windows MSVC Debug focused metadata CTest: 4/4

## Remaining

This closes only retained signature-blob `ASSEMBLY_REF` payloads as ModuleRef live roots. Cross-module target/provider
binding, real export manifest/table rewrite/publication, complete metadata sweep, annotation policy, full trim analyzer,
and broader runtime ABI drift/deopt coverage remain open.
