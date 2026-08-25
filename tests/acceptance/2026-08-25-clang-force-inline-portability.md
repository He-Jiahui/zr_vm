# Clang Force-Inline Portability Acceptance

## Status

- Date: 2026-08-25 (UTC+08:00)
- Status: `completed`
- Scope: Clang C11 Debug static linkage for public-header force-inline helpers

## RED

The task-specific cache was configured with Clang 14, Debug,
`BUILD_STATIC_LIB=ON`, `BUILD_SHARED_LIB=OFF`, and `BUILD_TESTS=ON`. Building
`zr_vm_ownership_intrinsic_member_separation_test` compiled all 627 build steps
and failed only at the final executable link. The linker reported unresolved
references including `ZrCore_Array_Get`, `ZrCore_Array_Push`,
`ZrCore_String_CreateFromNative`, and memory helpers.

Archive inspection showed that the Clang objects contained unresolved uses but
no definition for these public-header helpers. The GCC build did not emit those
references because its `ZR_FORCE_INLINE` branch uses `always_inline`.

## Root Cause

Commit `1655c1b` correctly moved `__clang__` detection before `__GNUC__`, but
that made the pre-existing Clang branch active. Its plain C11 `inline` macro did
not guarantee inlining in Debug builds and the project has no separate
out-of-line definitions for those helpers.

## GREEN

Rebuilding the same target after aligning the Clang macro with the GNU
`always_inline` contract completed all 612 incremental actions. The final
`zr_vm_ownership_intrinsic_member_separation_test` executable link returned
exit code 0, with none of the former unresolved `ZrCore_*` symbols.

Direct execution reached Unity and ran all 30 tests. Twenty-nine passed and the
only failure was the intentional ownership TDD assertion that legacy numeric
builtin id 8 must stop behaving as DETACH. This separates the restored Clang
linkage boundary from the still-pending ownership enum cleanup.

An additional `zr_vm_semantic_facts_test` build was requested as supplementary
evidence, but its CMake `VerifyGlobs.cmake` step remained blocked in WSL P9 file
enumeration for about 30 minutes before entering compilation. It was cancelled
and is not counted as acceptance evidence; no build process remained afterward.
