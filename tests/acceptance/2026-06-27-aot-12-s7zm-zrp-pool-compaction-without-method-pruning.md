# 2026-06-27 AOT 12-S7ZM ZRP Pool Compaction Without Method Pruning

- Slice: 12-S7ZM, under 12-S7 emitted zrp metadata pruning.
- Goal: make existing pool compaction run even when no MethodDef row is removed.

## Acceptance

- `backend_aot_c_zrp_metadata_prune.c` no longer skips blob rebuild solely because `retainedMethodDefCount == methodDefCount`.
- The pruner builds signature/string remaps first, then skips rebuild only when table counts, constant-pool handling, signature remap, and string remap are all identity.
- A direct pool-pruning fixture keeps both MethodDefs reachable but stores duplicate retained `Shared` strings at different source offsets. Code stripping still emits an owned compacted blob and rewrites both retained row offsets to one emitted string slice.

## Evidence

- RED: WSL gcc `zr_vm_aot_c_zrp_metadata_pool_pruning_test` failed 1/4 because `ownedBlob` stayed null when no MethodDef was pruned.
- GREEN: WSL gcc, WSL clang, and Windows MSVC Debug pass pool pruning 4/0, direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, and focused CTest `aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping` 2/2.

## Remaining Work

This does not implement full trim analyzer support, annotation-driven metadata promotion/suppression, cross-module/export token handling, owner-backed constant-pool retained-slice remap, or dump/diff tooling.
