#ifndef ZR_VM_PARSER_EXEC_IR_INTERNAL_H
#define ZR_VM_PARSER_EXEC_IR_INTERNAL_H

#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/semantic_ir.h"

TZrBool zr_parser_exec_ir_mark_place_values(
        const SZrSemanticIrFunction *semantic,
        SZrExecIrFunction *output,
        TZrExecIrValueId firstPlaceValue,
        SZrExecIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_EXEC_IR_INTERNAL_H */
