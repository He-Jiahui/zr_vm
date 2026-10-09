#include "compiler_source_retention_internal.h"

#include <string.h>

/* Safe refusal for the actual returned-source C RED gate. */
TZrBool compiler_retain_source_scalar_aot(SZrCompilerState *cs, SZrFunction *selected,
        SZrSourceCompileDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->stage = ZR_SOURCE_STAGE_COMPILE;
        diagnostic->result = cs == ZR_NULL || selected == ZR_NULL
                ? ZR_SOURCE_RESULT_INVALID_ARGUMENT : ZR_SOURCE_RESULT_UNSUPPORTED;
    }
    return ZR_FALSE;
}
