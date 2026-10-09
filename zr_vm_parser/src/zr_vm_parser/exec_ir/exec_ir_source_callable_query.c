#include "exec_ir_source_callable_query_internal.h"

#include <string.h>

TZrBool source_callable_query(const struct SZrSemanticContext *context, TZrTypeId callableTypeId,
                              TZrMetadataToken functionToken, SSourceCallableFacts *output,
                              SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL)
        memset(diagnostic, 0, sizeof(*diagnostic));
    (void) callableTypeId;
    if (context == ZR_NULL || output == ZR_NULL) {
        if (diagnostic != ZR_NULL)
            diagnostic->code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
        return ZR_FALSE;
    }

    /* RED staging: no lookup, traversal or output publication. */
    if (diagnostic != ZR_NULL) {
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED;
        diagnostic->functionToken = functionToken;
    }
    return ZR_FALSE;
}
