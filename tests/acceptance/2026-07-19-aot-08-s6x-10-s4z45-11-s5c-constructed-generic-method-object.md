# 2026-07-19 AOT 08-S6X / 10-S4Z45 / 11-S5C Constructed Generic Method Object

## Scope

This slice materializes an exact, existing MethodSpec resolution as a GC-managed constructed generic method reflection
object. It does not synthesize MethodSpec metadata, decode script argument objects, dispatch script
`MakeGenericMethod`, or add cross-module method binding.

## Implementation Contract

- `ZrCore_Reflection_BuildConstructedGenericMethodObject()` re-runs exact MethodSpec resolution and compares every carrier
  identity field before allocating the public object graph.
- The result exposes the actual MethodDef name, MethodSpec and generic MethodDef tokens, signature hash, recursive generic
  argument objects, attached runtime, and a `genericMethodDefinition` object link.
- `isGenericMethod`, `isGenericMethodDefinition`, and `isConstructedGenericMethod` distinguish the closed object from its
  open definition.
- Invalid state/runtime/carrier, mutated hash, or mutated request arguments fail closed.
- Reflection object key/value creation and temporary GC pinning are shared by generic type and method object builders.

## TDD Evidence

- RED: MSVC compiled the declaration and test, then linked with exactly one missing symbol:
  `ZrCore_Reflection_BuildConstructedGenericMethodObject`.
- GREEN: the focused dynamic generic executable reports 29 tests and 0 failures under MSVC 19.44, GCC 11.4, and
  Clang 14.0.
- The constructed method test roots the result, performs full GC, and then re-reads both `genericArguments` and
  `genericMethodDefinition` descendants.

## Regression Evidence

- Focused CTest on each compiler: `metadata_runtime_query`, `metadata_runtime_method_binding`,
  `reflection_token_resolve`, `metadata_runtime_binding_compatibility`, `metadata_runtime_typespec_layout`, and
  `reflection_dynamic_generic_instance`: 6/6.
- Shared regression on each compiler: GC 66/0, instruction execution 31/0, instruction table 95/0.
- New reflection object/helper sources emit no GCC or Clang diagnostics. Existing computed-goto, unused-function, and
  const-qualifier warnings remain outside this slice.
- Clang evidence was accepted only after source comparison against the workspace immediately before execution.
- The pre-existing `HEAD` profile-enum gap remains outside this slice. Validation used the concurrent 07 worktree enum;
  no profile source or header is included in this sub-milestone.

## Acceptance Decision

Accepted as 08-S6X / 10-S4Z45 / 11-S5C. Constructed generic method reflection-object materialization is closed without
changing metadata schema or code-registration ABI. Script `MakeGenericMethod`, argument-object decoding/dispatch,
cross-module method binding, invoke thunks, and full-AOT reflection closure remain open.
