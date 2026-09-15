---
module: zr_vm_core.exec_ir.oracle
plan: docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
status: implemented-contract
---

# ExecIR oracle memory boundary

The reference ExecIR oracle keeps memory external to the value model. A
caller supplies `FZrExecIrOracleMemory` in
`zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h`; the callback receives
the instruction, operation kind, and bounded value operands. LOAD has one
address operand and must return a defined scalar value. STORE has address and
value operands and receives a null result pointer. The callback can therefore
use a deterministic fixture or a runtime-owned semantic heap without exposing
host pointers in ExecIR.

LOAD without a provider is rejected with
`ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED`. A provider failure is reported as
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_MEMORY_ERROR`, preserving function, instruction,
and source identity. A callback result with the UNDEFINED kind is rejected as
an invalid value. STORE keeps the legacy event-only behavior when no provider
is present; supplying a provider makes the store stateful.

Successful memory operations append ordered STORE/LOAD events. The event is
published only after the provider operation succeeds, so a failed access
cannot be mistaken for an observed side effect. The callback owns any
external state and should make its own rollback decision if a later oracle
allocation fails.

The focused `ssa_oracle_projections` fixture covers missing-provider
rejection, stateful store-then-load replay, event order/source identity, and a
provider failure. It is a reference-mode contract, not a claim that the
production ExecBC heap has been switched to this callback.

## Allocation boundary

`ALLOC` has the same explicit-host-boundary rule as LOAD. A caller may supply
`FZrExecIrOracleAllocate` and its `allocateUserData`; the callback receives the
bounded constructor operands and returns one defined, pointer-free oracle value
(for example, a deterministic object token). The callback must not encode a
host pointer or append an event itself. Without a provider, ALLOC fails closed
with `ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED`; a provider rejection preserves the
instruction/source identity in
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ALLOCATION_ERROR`. The oracle publishes an
`ZR_EXEC_IR_ORACLE_EVENT_ALLOCATE` event only after a valid callback result is
available, then assigns that value to the instruction's result slot.

The allocation fixture also covers a provider rejection and an undefined
provider result. These are reference-mode semantics: the callback owns any
external allocation ledger and decides how to roll back if a later oracle
operation fails.

The no-optimization ExecBC and AOT projections preserve `ALLOC` as a typed
instruction with its operand/result ranges. This is metadata transport only:
the AOT projection remains non-runnable and an eventual backend allocator must
establish its own runtime ownership and GC protocol before execution is
enabled.

## Ownership/drop boundary

`DROP` remains available without a provider, but it is a consuming operation in
the oracle value environment. The bounded DROP event is published first; once
it succeeds, every operand slot is reset to `UNDEFINED`. A later use therefore
fails with `ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE` at the consuming instruction
and source ID. This models the runtime OWN_DROP slot reset while keeping the
external destructor/ledger outside the pointer-free oracle. If a later step
fails, the prepared execution result is discarded transactionally, so the
caller never observes a partial event stream.

## Projection ownership

The no-optimization ExecBC and AOT projection records now own a copy of the
memory-token pool referenced by each instruction's `memoryIn` and `memoryOut`
ranges. LOAD is retained as a projected operation; it is not silently dropped
or relabeled as an interpreter fallback. Moving a projection into the AOT
view transfers that pool exactly once, and freeing either view releases it
along with the other copied side arrays. The AOT view remains metadata-only
(`runnable == false`) until a real backend emitter supplies executable code.
