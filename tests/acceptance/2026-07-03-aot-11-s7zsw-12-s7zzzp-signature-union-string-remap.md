# AOT 11-S7ZSW / 12-S7ZZZP retained signature UNION string-pool sweep/remap

Completed: 2026-07-03 22:23:50 +08:00

## Scope

This slice closes a compacted `.zrp` metadata pruning gap where retained signature blobs with `UNION(valueType, baseNameStringOffset, args...)` kept the signature bytes but dropped the union base-name string slice from the compacted string pool.

## RED

- Extended `tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c` with `test_aot_c_zrp_metadata_methodspec_pruning_remaps_union_base_name_string_offset`.
- WSL GCC failed the focused MethodSpec pruning test with `Expected 780 Was 773`, proving the old retained-signature string sweep dropped the `"Option"` string referenced only by the retained `UNION` node.

## GREEN

- `backend_aot_c_zrp_metadata_string_pool.c` now treats the `UNION` base-name string offset as a retained signature string root.
- `backend_aot_c_zrp_metadata_signature.c` now rewrites that `UNION` base-name offset through the compacted string-pool remap after signature blob copy.
- The focused MethodSpec fixture now preserves `"Option"` and rewrites the copied signature to the compacted string offset.

## Verification

- WSL GCC: metadata pruning CTest matrix 5/5; TypeDef pruning 2/0; source contracts 24/0.
- WSL clang: metadata pruning CTest matrix 5/5; TypeDef pruning 2/0; source contracts 24/0.
- Windows MSVC Debug: metadata pruning CTest matrix 5/5; TypeDef pruning 2/0; source contracts 24/0.
- `git diff --check` on touched code/test files exited 0 with only LF/CRLF warnings; trailing-whitespace scan was clean.

## Status

This only closes retained signature `UNION` base-name string retention/remap. Complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, provider binding edges, and broader ABI drift/deopt closure remain open.
