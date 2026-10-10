#include "exec_ir_source_finalized_module_internal.h"

#include <stdint.h>
#include <string.h>

TZrBool source_finalized_module_fail(SZrExecIrDiagnostic *diagnostic, EZrExecutionDiagnosticCode code,
                                     TZrMetadataToken token) {
    if (diagnostic != ZR_NULL) {
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = code;
        diagnostic->functionToken = token;
    }
    return ZR_FALSE;
}

TZrMetadataToken source_finalized_module_read_selected_token(const SZrExecIrModule *original) {
    uintptr_t start;
    size_t bytes;
    if (original == ZR_NULL || original->functionCount != 1u || original->functionCapacity < original->functionCount ||
        original->functions == ZR_NULL)
        return 0u;
    if ((uintmax_t) original->functionCapacity > (uintmax_t) SIZE_MAX / sizeof(*original->functions))
        return 0u;
    bytes = (size_t) original->functionCapacity * sizeof(*original->functions);
    start = (uintptr_t) original->functions;
    if ((uintmax_t) bytes > (uintmax_t) UINTPTR_MAX || start > UINTPTR_MAX - (uintptr_t) bytes)
        return 0u;
    return original->functions[0].functionToken;
}
