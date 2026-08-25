---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_core/include/zr_vm_core/array.h
  - zr_vm_core/include/zr_vm_core/string.h
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
plan_sources:
  - user: 2026-08-25 complete ownership/object-member acceptance on GCC, Clang, and MSVC
  - docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md
  - docs/plans/lsp/optimize/2026-08-24-plan03-task02-visible-symbols.md
tests:
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/acceptance/2026-08-25-clang-force-inline-portability.md
doc_type: module-detail
---

# Compiler Inline Portability

## Purpose

`zr_vm` defines small public-header helpers through `ZR_FORCE_INLINE`. Those
helpers are used by the core, parser, libraries, and test executables, so their C
linkage behavior is part of the build contract rather than a local optimization.
GCC, Clang, and MSVC Debug builds must all produce link-complete static targets.

## Compiler Detection

Clang defines both `__clang__` and `__GNUC__`. Detection therefore checks
`__clang__` first and assigns `ZR_COMPILER_CLANG`; GNU detection follows. MSVC
uses `_MSC_VER`. This order keeps compiler-specific diagnostics and attributes
accurate instead of treating Clang as GCC.

## Force-Inline Contract

GNU and Clang use:

```c
__attribute__((always_inline)) inline
```

MSVC uses `__forceinline`. The GNU-style attribute is required for the existing
non-static header definitions. With plain C11 `inline` at Debug optimization
levels, Clang may emit an external reference without emitting an out-of-line
definition. A static executable then fails to link on helpers such as
`ZrCore_Array_Get` and `ZrCore_String_CreateFromNative`.

The Clang branch deliberately matches the established GNU branch. Converting
all public helpers to `static inline` would change linkage across a much larger
API surface and is not needed to restore the existing contract.

## Failure Boundary

The acceptance boundary is a real Clang 14 C11 Debug static executable, not a
preprocessor-only assertion. The focused ownership runner includes core array,
string, parser, runtime, and system-module consumers. Its successful link proves
that the header helpers do not leave unresolved external symbols. Its Unity
result is evaluated separately from the link gate.

## Test Coverage

`tests/parser/test_ownership_intrinsic_member_separation.c` is built with
`BUILD_STATIC_LIB=ON`, `BUILD_SHARED_LIB=OFF`, and Clang 14 in Debug mode. The
pre-fix build compiled all 627 steps but failed at the final link with unresolved
`ZrCore_*` helpers. Rebuilding after the macro correction completed all 612
incremental actions and linked the same executable with exit code 0. Direct
execution reached all 30 Unity cases; 29 passed and the sole failure was the
separate, intentional DETACH-removal TDD assertion. The final ownership
acceptance matrix repeats the same static toolchain boundary under GCC, Clang,
and MSVC.

## Scope

This contract changes no language syntax, runtime ownership behavior, exported
function signature, or artifact schema. It only restores link completeness for
the compiler branch that became reachable when Clang detection was corrected.
