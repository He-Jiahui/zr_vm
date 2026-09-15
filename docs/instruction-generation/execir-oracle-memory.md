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
