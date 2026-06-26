#ifndef ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_SOURCE_LOCATION_H
#define ZR_VM_PARSER_BACKEND_AOT_EXEC_IR_SOURCE_LOCATION_H

#include "zr_vm_core/function.h"

TZrUInt32 backend_aot_exec_ir_debug_line_for_instruction(const SZrFunction *function,
                                                         TZrUInt32 execInstructionIndex);
TZrUInt32 backend_aot_exec_ir_debug_line_end_for_instruction(const SZrFunction *function,
                                                             TZrUInt32 execInstructionIndex,
                                                             TZrUInt32 debugLine);
TZrUInt32 backend_aot_exec_ir_debug_column_for_instruction(const SZrFunction *function,
                                                           TZrUInt32 execInstructionIndex);
TZrUInt32 backend_aot_exec_ir_debug_column_end_for_instruction(const SZrFunction *function,
                                                               TZrUInt32 execInstructionIndex,
                                                               TZrUInt32 debugColumn);

#endif
