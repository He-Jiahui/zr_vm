#ifndef ZR_VM_PARSER_EXEC_IR_COPY_ALIASES_H
#define ZR_VM_PARSER_EXEC_IR_COPY_ALIASES_H

#include "../exec_ir_pass_internal.h"

/* Private SCCP rewrite. The caller validates ExecIR storage first. Budget
 * exhaustion succeeds conservatively and is reported through context. */
TZrBool ZrParser_ExecIr_RewriteCopyAliases(
        SZrExecIrFunction *function, SZrExecIrPassContext *context,
        TZrBool *changed, SZrExecIrDiagnostic *diagnostic);

#endif
