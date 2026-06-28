# 2026-06-27 AOT 12-S7ZL ZRP String Pool Duplicate Slice Compaction

- Slice: 12-S7ZL, under 12-S7 trim warnings and size statistics / emitted zrp metadata pruning.
- Goal: compact retained zrp string-pool slices by content, not only by old offset.

## Acceptance

- `backend_aot_c_zrp_metadata_string_pool.c` keeps one emitted string slice when two retained rows point to different old offsets that contain identical NUL-terminated bytes.
- Each old offset still has a remap entry, so TypeDef/MethodDef/FieldDef/GenericParam/ModuleRef string offset rewriting can resolve both the original and duplicate source offsets.
- The direct pool-pruning fixture covers a retained TypeDef name and retained FieldDef name both equal to `Shared` but stored at different old offsets; after pruning, the string pool is `Shared\0Example\0Kept\0`, and both rows point to the same new offset.

## Evidence

- RED: WSL gcc `zr_vm_aot_c_zrp_metadata_pool_pruning_test` failed 1/3 after the new duplicate-slice fixture expected length 540 but the old offset-only remap produced 547.
- GREEN: WSL gcc, WSL clang, and Windows MSVC Debug pass pool pruning 3/0, direct zrp pruning 5/0, code stripping 5/0, source contracts 21/0, and focused CTest `aot_c_zrp_metadata_pool_pruning|aot_c_code_stripping` 2/2.

## Remaining Work

This does not implement full trim analyzer support, annotation-driven metadata promotion/suppression, cross-module/export token handling, owner-backed constant-pool retained-slice remap, or dump/diff tooling.
