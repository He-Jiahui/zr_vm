#ifndef ZR_VM_PARSER_EXEC_IR_STATE_MAPS_H
#define ZR_VM_PARSER_EXEC_IR_STATE_MAPS_H

#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/conf.h"

ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildStateMaps(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

/* Explicitly elaborate cleanup-block obligations and rebuild their maps.
 * Ordinary DROP instructions elsewhere retain strict semantics. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_ElaborateCleanupDrops(
        SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic);

#endif
