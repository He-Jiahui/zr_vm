---
title: Optimization build profiles and incremental IR cache
---

# Optimization build profiles and incremental IR cache

The compiler-facing profile contract is defined by
`zr_vm_parser/compile_optimization_profile.h`.  A profile has independent
build, numeric, target/backend, frame-budget, and pipeline dimensions.  The
`ApplyPreset` helper is only shorthand for filling those dimensions; preset
expansion never enables fast-math implicitly.

`Normalize` validates the dimensions and produces an effective immutable
policy.  Invalid combinations are reported with a typed diagnostic, for
example host JIT on a mobile target, fast-math requested under strict numeric
permission, or LTO/PGO requested for a non-LLVM backend.  The normalized
policy hash is stable and contains no pointers or runtime generation values,
so it can be included in an artifact or cache key.  `Describe` is the dry-run
surface used by a CLI to show the effective build settings and hash.

The cache contract in `zr_vm_parser/compile_ir_cache.h` builds a portable
SHA-256 key from source/dependency bytes, contract and compiler ABI hashes,
pass pipeline, target, numeric/layout policy, imported profile, and the
normalized profile hash.  Runtime addresses are deliberately excluded.  A
lookup is accepted only for a complete published payload.  Writers receive a
transaction token; publishing first copies and validates the candidate, then
replaces the entry in one operation.  A failed or cancelled transaction never
creates an entry, and `MarkCorrupt` removes only the affected key so the next
build can reconstruct it without clearing the workspace.

The focused contract test is `tests/parser/test_ssa_build_profiles.c`.  It
covers default dry-run output, conflict diagnostics, key invalidation when a
contract changes, deterministic publication, cancellation, and single-entry
corruption recovery.  The owning build system should register it as
`zr_vm_ssa_build_profiles_test` / `ssa_build_profiles` and run the strict
GCC/Clang commands from the 11.01 plan before folding it into the full SSA
differential matrix.
