---
related_code:
  - zr_vm_core/include/zr_vm_core/aot_ir.h
  - zr_vm_parser/include/zr_vm_parser/aot_ir_lowering.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_aot_lowering.c
doc_type: runtime-contract
status: implemented
---

# Shared AOTIR C and LLVM lowering contract

`ZrParser_AotIr_EmitC` and `ZrParser_AotIr_EmitLlvm` consume the same validated,
relocation-free AOTIR module. The facade rejects a missing function table (or an
empty module) as `ZR_AOT_IR_INVALID_ARGUMENT` before sizing or dereferencing
lowering records. The shared lowering gate rejects both module and function
relocation rows before it derives target-specific coverage from a common
lowering record sequence. The facade distinguishes native lowering, runtime
bridges, and interpreter fallback. A runtime bridge is never reported as native
coverage; callers can disallow bridges or fallback and receive a source-linked
`ZR_AOT_IR_UNSUPPORTED` diagnostic instead.

Emitter options use the same strict boolean contract as the backend adapter;
noncanonical `TZrBool` values are rejected as `ZR_AOT_IR_INVALID_ARGUMENT`.

The result is a target contract record, not generated source or LLVM bitcode.
It carries the source hash, coverage counts and a deterministic contract hash
that includes target and strict-floating-point policy. Existing backend AOT
emitters remain the owners of physical code emission, GC root publication, EH
cleanup and loader registration; this facade provides their common schema gate.
