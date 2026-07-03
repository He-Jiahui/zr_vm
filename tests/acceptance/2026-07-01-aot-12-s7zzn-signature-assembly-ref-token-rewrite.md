# AOT 12-S7ZZN / 11-S7 Signature AssemblyRef Token Rewrite

- Timestamp: 2026-07-01 22:34:08 +08:00
- Status: GREEN for this support sub-slice. The broader 07~12 goal remains open.

## Scope

Retained zrp signature blobs now rewrite embedded `ASSEMBLY_REF` tokens after ModuleRef pruning and RID compaction. When
a live ModuleRef row moves from source RID2 to compacted RID1, retained signature payloads that reference that assembly
now publish the compacted token instead of the stale source RID.

## RED

Added `test_aot_c_zrp_metadata_pool_pruning_rewrites_signature_assembly_ref_tokens`.

The fixture keeps a `TYPE_REF` token record that owns a signature blob whose root node is `ASSEMBLY_REF` RID2. The
ModuleRef table also contains an orphan RID1, so pruning compacts the live assembly ref to RID1. The old signature
rewrite skipped `ASSEMBLY_REF` nodes:

- WSL GCC direct `zr_vm_aot_c_zrp_metadata_pool_pruning_test`: 7 tests / 1 failure
- Failure: `Expected 67108865 Was 67108866`

## GREEN

`backend_aot_c_zrp_rewrite_retained_signature_type_def_tokens()` now threads TypeSpec and ModuleRef rows into its
rewrite context. Signature rewriting keeps the existing direct TypeDef token rewrite and adds an `ASSEMBLY_REF` branch
that reads the embedded token, remaps it through `backend_aot_c_zrp_remap_module_ref_token()`, and writes the compacted
token back before signature validation and hash recomputation.

The source contract now locks the ModuleRef context and signature rewrite helper call.

## Verification

- WSL GCC direct: zrp metadata pruning 9/0, TypeSpec pruning 2/0, pool pruning 7/0, export token remap 8/0, source contracts 24/0
- WSL GCC focused metadata CTest: 4/4
- WSL Clang direct: zrp metadata pruning 9/0, TypeSpec pruning 2/0, pool pruning 7/0, export token remap 8/0, source contracts 24/0
- WSL Clang focused metadata CTest: 4/4
- Windows MSVC Debug direct: zrp metadata pruning 9/0, TypeSpec pruning 2/0, pool pruning 7/0, export token remap 8/0, source contracts 24/0
- Windows MSVC Debug focused metadata CTest: 4/4

## Remaining

This closes only retained signature-blob `ASSEMBLY_REF` token compaction after ModuleRef pruning. Cross-module
target/provider binding, real export manifest/table rewrite/publication, complete metadata sweep, annotation policy,
full trim analyzer, and broader runtime ABI drift/deopt coverage remain open.
