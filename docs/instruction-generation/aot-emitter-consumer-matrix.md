---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_adapter.h
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_llvm_function_body.c
plan_sources:
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_aotir_contract.c
  - tests/parser/test_ssa_aot_backend_adapters.c
  - tests/parser/test_ssa_aotir_state_map.c
doc_type: migration-inventory
status: inventory-only
---

# AOT emitter input inventory

The production C and LLVM writers both start with
`backend_aot_exec_ir_build_module`. Its `SZrAotExecIrModule` copies SemIR
opcode/type/effect rows and indexes into `SZrFunction.instructionsList`.
The LLVM function writer directly decodes each legacy instruction into an
instruction context; the C function writer does the same within its dispatch.
The older module is therefore not the shared `SZrAotIrModule` contract.

| Family | Current C / LLVM input and consumer | Shared input and remaining gap |
| --- | --- | --- |
| Scalar/constants | `backend_aot_c_scalar_semir.c` reads SemIR type rows alongside legacy slots; `backend_aot_c_constant_consumers.c` walks GET_CONSTANT / STACK / branch sequences; `backend_aot_llvm_lowering_constants.c` reads the legacy constant pool. | ExecIR values, type/layout IDs and source-backed constant payloads must replace opcode-pattern and pool lookups. The current projection owns slot metadata but does not transport constant payloads. |
| Control | `backend_aot_exec_ir.c` derives blocks from legacy instruction offsets; `backend_aot_c_function_body.c` and `backend_aot_llvm_lowering_branch_control.c` decode JUMP variants and targets. | Projected block/successor ranges and scheduled edge phi moves exist; the emitter needs an edge-aware lowering of those fields, including critical-edge copies. |
| Calls/native | `backend_aot_c_function_body.c`, `backend_aot_c_lowering_calls.c`, and `backend_aot_llvm_lowering_calls.c` use legacy call operands, cache shape and runtime call helpers; the old module separately derives callsite kind from opcode/cache. | ExecIR call/invoke operands, result ranges, `bindingRow`, effect/memory tokens and target ABI are the semantic source. A real callsite ABI and native-import legalization still need to be attached before either emitter switches. |
| Member/property | `backend_aot_c_function_body.c` decodes GET_MEMBER and META_GET/SET variants; `backend_aot_llvm_lowering_member_access.c` and `backend_aot_llvm_lowering_meta_access.c` dispatch property/reference opcodes. | Typed place, receiver, member binding and guard/dispatch identity need one shared AOTIR producer; legacy cache opcodes cannot be the semantic source. |
| Arrays/index | `backend_aot_c_function_body.c` decodes CREATE_ARRAY and SUPER_ARRAY variants; `backend_aot_llvm_lowering_index_access.c` selects runtime helpers by quickened opcode. | Projected operands/results are available, but container element layout, bounds/ownership and helper choice need shared legalization. |
| Strings | `backend_aot_c_function_body.c` dispatches ADD_STRING, TO_STRING and string comparisons; constant consumers inspect string objects from the legacy pool. | AOTIR requires an owned/lifetime-safe constant description and explicit string operation semantics; a numeric opcode alone is insufficient. |
| Inline/aggregate | `backend_aot_c_function_body.c` handles CREATE_INLINE_ARRAY and place binding; `backend_aot_exec_ir_return_layout.c` derives inline return layout from legacy frame metadata. | The shared target layout must specify inline field/return storage and root relocation. Logical state maps alone do not define physical inline fields. |
| Exception/cleanup | `backend_aot_c_function_body.c` and `backend_aot_llvm_lowering_exception_control.c` decode TRY, THROW and END_FINALLY; `backend_aot.c` derives may-throw step flags from legacy opcodes. | ExecIR handler edges, payload provider, effect checkpoints and owned state maps exist; physical EH landing pads, cleanup transitions and frame recovery are not emitted. |
| Async/suspend | No dedicated AWAIT/YIELD/SUSPEND lowering appears in the legacy C/LLVM function dispatch; call boundaries use a runtime resume-fallthrough sentinel. | The shared SUSPEND event exists in the oracle/projection, but neither emitter has a continuation ABI or physical resume lowering. Treat this family as unsupported until supplied. |

`SZrAotIrProjection` is a per-function, non-runnable owned view of ExecIR
CFG/instructions/slots/source spans/logical state map. The independently
defined `SZrAotIrModule` is a validated, caller-owned module descriptor;
`backend_aot_ir_adapter` consumes it only to report lowering/coverage facts
(`descriptorOnly`, `artifactAvailable = false`). No producer currently
converts the owned projection into that module, and the archived production
emitters still consume the legacy module. Connecting those representations
must preserve the state-map entry and side-pool semantics instead of reducing
them to a presence flag or hash. A C/LLVM artifact claim requires replacing
the legacy reads family by family and validating an actual emitted artifact.
