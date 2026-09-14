#ifndef ZR_VM_PARSER_EXEC_IR_ARRAY_LOWERING_H
#define ZR_VM_PARSER_EXEC_IR_ARRAY_LOWERING_H

#include "zr_vm_core/contiguous_view.h"
#include "zr_vm_parser/conf.h"

ZR_PARSER_API TZrBool ZrParser_ExecIr_LowerArrayIndex(
        const SZrContiguousView *view, TZrInt64 index, TZrSize *offset,
        SZrViewDiagnostic *diagnostic);

#endif
