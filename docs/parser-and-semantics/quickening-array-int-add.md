---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.h
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
  - tests/parser/test_span_core.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/acceptance/ssa-quickening-array-int-add.md
doc_type: module-detail
---

# Typed Array<int>.add Quickening

## Purpose

The compiler can emit `KNOWN_NATIVE_MEMBER_CALL` directly for a statically
resolved member call. That route does not leave the `GET_MEMBER_SLOT` plus
`KNOWN_NATIVE_CALL` instruction pair consumed by the older Array matcher. For
the statically selected `Array<int>.add` native provider, the late quickening
pass now recognizes the fused form and emits `SUPER_ARRAY_ADD_INT` when its
callsite and operand facts are complete.

The matcher preserves the generic call when any eligibility check fails. It
does not specialize a call based only on the member spelling or receiver type.

## Required Facts

The instruction must have a two-argument callsite cache of kind `MEMBER_GET`.
The cache must belong to the same instruction and resolve to member `add`.
The native binding must pass
`compiler_native_call_binding_is_provider_contract`, describe a direct `CALL`,
and use a module relocation whose target index is that member entry. The
compiler's slot facts must identify an `Array<int>` receiver and an `int` value.

The fused instruction stores the callsite cache index and argument count in
`operand1[0]` and `operand1[1]`, and its result slot in `operandExtra`. The
matcher uses the following two frame slots as the receiver and value inputs:

| Slot | Source |
| --- | --- |
| Receiver | `operandExtra + 1` |
| Value | `operandExtra + 2` |

The replacement instruction carries those input slots and no longer refers to
the callsite cache. Before rewriting the opcode, quickening clears the cache's
binding contract and relocation location. This prevents final callsite linking
from trying to attach the old provider relocation to the superinstruction.
Guard misses leave the original instruction and ordinary slot tracking intact.

## Regression Boundary

`test_w2_super_array_add_variable_value_elides_dead_receiver_setup` compiles a
loop that calls `values.add((i * 3 + 1) % 17)`, stores the receiver in another
container, and computes the expected sum. It checks all of the following:

- execution still returns `51`;
- at least one `SUPER_ARRAY_ADD_INT` is emitted;
- no specialized instruction retains an active binding contract, relocation,
  or call-binding instruction-map entry;
- the dead `GET_STACK`/`SET_STACK`/`GET_STACK` receiver materialization chain
  has been removed.

The implementation stays inside the existing Array-int late matcher. That
matcher uses file-local member-cache, slot-alias, type-kind, and control-flow
helpers from `compiler_quickening.c`. The source file is over 11,000 lines, but
extracting one conditional branch now would leave an artificial module with
those private dependencies. A cohesive follow-up boundary is the complete
Array-int late-specialization sweep and its helper family, leaving the pass
runner to invoke that module.

This subtask covers only the guarded `Array<int>.add` call form. SSA 03.04
remains open for its other matcher and dispatch integration work.

## Verification

See [`ssa-quickening-array-int-add.md`](../../tests/acceptance/ssa-quickening-array-int-add.md)
for the observed RED, final GCC/MSVC gates, exact cache paths, and the
call-binding artifact projection regression.
