---
related_code:
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_parser/include/zr_vm_parser/iteration_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
implementation_files:
  - zr_vm_lib_iteration/CMakeLists.txt
  - zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/module.c
  - zr_vm_lib_iteration/src/zr_vm_lib_iteration/runtime/descriptor.c
  - zr_vm_parser/include/zr_vm_parser/iteration_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/enumerator_binding.c
plan_sources:
  - docs/plans/syntax/2026-07-20-13-iterator-enumerator-yield-design.md
  - docs/plans/syntax/13-iterator-enumerator-yield/m1-enumerator-protocol-implementation-plan.md
tests:
  - tests/iterator/test_enumerator_protocol.c
  - tests/library/test_official_provider_convergence.c
  - tests/container/test_container_type_inference.c
  - tests/parser/test_numeric_foreach_cardinality_dataflow.c
  - tests/parser/test_numeric_loop_assignment_dataflow.c
  - tests/acceptance/2026-08-05-syntax-10c-official-provider-convergence.md
doc_type: module
---

# `zr.iteration` Enumerator Protocol

`zr.iteration` is the sole public owner of the iteration TypeIds:
`Iterable<T>`, `Enumerator<T>`, `Iterator<T>`, and `AsyncIterator<T>`.
Concrete libraries publish their capability metadata against these canonical names;
they do not redeclare protocol owner types.

The native descriptor explicitly declares Runtime phase and
`zr.iteration:v1:canonical-iterator-protocols` as its public contract hash.
Syntax 10C validates that the official inventory, descriptor phase, protocol
owners, reflection projection, and LSP consumer all observe this same provider.

## Public Contract

- `Iterable<T>.getEnumerator()` has the `ITERABLE_INIT` member role and returns
  `zr.iteration.Enumerator<T>`.
- `Enumerator<T>.moveNext()` has the `ITERATOR_MOVE_NEXT` role and `current` has
  the `ITERATOR_CURRENT_FIELD` role.
- `Iterator<T>` is the compiler-backed, non-value-constructible carrier that
  implements `Enumerator<T>`.
- `AsyncIterator<T>` publishes async `moveNext`, `current`, and `close` roles so
  later async lowering has a stable descriptor surface.

`Array<T>`, `Map<K,V>`, `Set<T>`, and `LinkedList<T>` keep their native callbacks
and capability bits. Their descriptor `implements` rows reference
`zr.iteration.Iterable<...>` and their iterator factory metadata returns
`zr.iteration.Enumerator<...>`.

## Compiler Binding

Ordinary `for` resolves an element type through
`ZrParser_EnumeratorBinding_ResolveElementType`. The bridge consumes only an
`ITERATOR` or `ITERABLE` protocol projection with its resolved generic argument.
It has no Array-base-type, `ARRAY_LIKE`, concrete type-name, member-spelling, or
source-text fallback. Array inference publishes an `ITERABLE` capability fact,
which preserves static `ITER_INIT`, `ITER_MOVE_NEXT`, and `ITER_CURRENT` lowering.

Scope exit owns loop-binding lifetime cleanup. The protocol introduction does not
add a separate object boxing adapter or a new iterator cleanup opcode.

## M1 Boundary

M1 establishes descriptor ownership and synchronous `for` binding only. It does
not add `yield`, generator syntax, `iterator fn`, async loop lowering, frames, or
boxing adapters. `AsyncIterator<T>` is descriptor-only surface for a later phase.
