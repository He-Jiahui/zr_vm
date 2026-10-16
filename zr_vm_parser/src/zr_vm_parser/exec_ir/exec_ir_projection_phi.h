#ifndef ZR_VM_PARSER_EXEC_IR_PROJECTION_PHI_H
#define ZR_VM_PARSER_EXEC_IR_PROJECTION_PHI_H

#include "exec_ir/exec_ir_projections.h"

TZrBool zr_projection_schedule_phi_copies(SZrExecBcProjection *projection,
                                           const SZrExecIrFunction *function,
                                           SZrExecIrDiagnostic *diagnostic);

#endif
