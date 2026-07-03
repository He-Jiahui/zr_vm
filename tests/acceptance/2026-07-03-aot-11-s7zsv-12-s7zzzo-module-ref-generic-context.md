# AOT 11-S7ZSV / 12-S7ZZZO ModuleRef retained-row generic context

Completed: 2026-07-03 22:04:51 +08:00

## Scope

This slice closes a compacted `.zrp` metadata pruning gap where a ModuleRef row was dropped even though a retained import TypeRef still referenced it. The missing edge was a TypeSpec retained only through `GenericParamConstraint`; ModuleRef retained checks now carry the same generic context as TypeSpec pruning.

## RED

- Added `tests/parser/test_aot_c_zrp_metadata_module_ref_pruning.c`.
- WSL GCC failed the focused ModuleRef pruning fixture with `Expected 1 Was 0`, proving the old path pruned the AssemblyRef/ModuleRef row.

## GREEN

- `backend_aot_c_zrp_metadata_module_ref.{h,c}` now threads `GenericParam` and `GenericParamConstraint` rows through ModuleRef retained/count/compact/remap.
- `backend_aot_c_zrp_metadata_prune.c`, `backend_aot_c_zrp_metadata_string_pool.c`, and `backend_aot_c_zrp_metadata_signature.c` pass that context to ModuleRef checks and token rewrites.
- The retained TypeRef fixture now keeps the ModuleRef row and rewrites `relatedToken` to compacted AssemblyRef RID 1.

## Verification

- WSL GCC: metadata pruning CTest matrix 5/5; TypeDef pruning 2/0; source contracts 24/0.
- WSL clang: metadata pruning CTest matrix 5/5; TypeDef pruning 2/0; source contracts 24/0.
- Windows MSVC Debug: metadata pruning CTest matrix 5/5; TypeDef pruning 2/0; source contracts 24/0.

## Status

This only closes the ModuleRef generic-context retention/remap slice. Complete metadata sweep/pruning, full trim analyzer, annotation/promotion policy, provider binding edges, and broader ABI drift/deopt closure remain open.
