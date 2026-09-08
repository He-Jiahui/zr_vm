---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/function_identity.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_graph.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/call_binding_signature.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_graph.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/execution/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/src/zr_vm_core/call_binding_member.c
  - zr_vm_core/src/zr_vm_core/function_identity.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
plan_sources:
  - user: 2026-09-06 W2 / Call Binding M1 静态调用绑定与可重定位目标表
tests:
  - tests/core/test_call_binding_runtime.c
  - tests/library/test_call_binding_module.c
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_call_binding_relocation.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_typed_call_binding.c
  - tests/acceptance/2026-09-06-call-binding-m1.md
doc_type: module-detail
---

# Call Binding Runtime

Call Binding is the runtime contract shared by statically resolved VM calls,
native calls, AOT calls, accessors, meta-functions, virtual methods, interface
slots, and typed function values. The contract carries metadata tokens,
structural signature hashes, module identity, layout version/hash, operation,
and a dispatch slot. The resolved target is a separate tagged runtime witness;
it can contain a VM function, native callback, AOT entry, or a GC-traced
callable object.

## Binding and validation

Compiler facts initialize the cache entry before linking. The linker validates
the token tables, signature row, module hash, owner layout, relocation kind,
instruction coordinate, and target index. It builds a per-function instruction
map so a callsite cache is selected directly by instruction index. A failed
check invalidates the witness and reports a structured status such as
`signature-mismatch`, `layout-mismatch`, `target-not-found`, or
`stale-generation`.

Direct VM and native targets are resolved once and then checked by generation.
Virtual and interface entries keep their slot contract and select a concrete
descriptor after receiver shape/type validation. Getter, setter, and meta
operations use the same contract while preserving receiver provenance,
ownership cleanup, exceptions, and inline-struct writeback. Typed function
values retain a zero-target signature contract because the live value can be a
VM closure, native callback, or AOT callable.

## Function graph identity

Function constants that are also inline children are rebound using
`ZrCore_Function_HasSameDefinition` or an explicit `CALLABLE_CHILD` metadata
alias. The identity includes instruction bytes, constant-pool shape and literal
values (plus shared storage identity when available), source identity,
parameter count, and complete source/debug spans. This avoids
conflating same-name functions declared on one source line, a case that is
especially important when a binary provider contains both `answer()` and
`Math.answer()`.

The graph visitor follows constants and children with cycle protection. It is
used by linking, generation invalidation, GC marking/rewriting, and AOT table
construction. GC treats the callable witness and owner prototype as managed
edges; compaction rewrites those edges without exposing a process address in a
persistent record.

## Reload behavior

Removing or replacing a module advances the generation through the complete
function graph and clears resolved witnesses. Removal also clears the module
registry's hot string-pair cache before freeing the hash node. An old witness
fails with a structured stale-generation diagnostic. A subsequent import links
the replacement provider by token and contract; static sites do not fall back
to a member-name lookup.

## Test coverage

`call_binding_runtime` covers direct, virtual, interface, typed, generation,
GC, and diagnostic behavior. `call_binding_module` covers source and binary
module functions, static type methods, captured module state, same-line
same-name binary methods, and reload invalidation. Native registry, artifact,
AOT projection, and property suites cover the other target kinds and shared
cache paths. Opcode-prefix and SIMD instruction design remain later milestones.
