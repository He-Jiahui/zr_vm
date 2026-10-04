#ifndef ZR_VM_PARSER_EXEC_IR_BUILD_COMPARE_H
#define ZR_VM_PARSER_EXEC_IR_BUILD_COMPARE_H

#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

EZrExecIrOpcode zr_parser_exec_ir_map_semantic_opcode(
        const SZrSemanticIrInstruction *instruction);
TZrBool zr_parser_exec_ir_lower_compare_metadata(
        const SZrSemanticIrFunction *source, const SZrSemanticIrInstruction *in,
        SZrExecIrInstruction *out, const SZrExecIrFunction *function,
        TZrExecIrBlockId block, SZrExecIrDiagnostic *diagnostic);

#endif
