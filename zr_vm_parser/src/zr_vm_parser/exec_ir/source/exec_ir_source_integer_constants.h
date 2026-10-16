#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_INTEGER_CONSTANTS_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_INTEGER_CONSTANTS_H
#include "exec_ir/source/exec_ir_source_integer_expression_internal.h"
struct SZrCompilerState;
/* Per-call source admission plus actual compiler/runtime pool certification.
 * output is initialized/empty, independent storage; failure leaves it intact. */
TZrBool source_integer_constants_prepare(const struct SZrCompilerState *compiler,
        SSourceIntegerProof *output, SZrExecIrDiagnostic *diagnostic);
#endif
