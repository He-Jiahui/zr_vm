#ifndef ZR_VM_PARSER_EXEC_IR_EXECBC_COMPARE_TYPES_H
#define ZR_VM_PARSER_EXEC_IR_EXECBC_COMPARE_TYPES_H

#include "exec_ir_execbc_vm_internal.h"
#include "zr_vm_parser/semantic.h"

TZrBool execbc_vm_prepare_canonical_compare_types(
        const SZrExecBcProjection *projection, const SZrSemanticContext *context,
        SZrExecBcInstruction *staged, SZrExecIrDiagnostic *diagnostic);

#endif
