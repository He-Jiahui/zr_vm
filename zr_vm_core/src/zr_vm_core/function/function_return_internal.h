#ifndef ZR_VM_CORE_FUNCTION_RETURN_INTERNAL_H
#define ZR_VM_CORE_FUNCTION_RETURN_INTERNAL_H

#include "zr_vm_core/function.h"

/* Call after TryCopyInlineFrameReturnValue declines. An inline source is boxed
 * only for a non-inline destination, while the callee payload is still alive.
 * The caller supplies scratch storage and restores temporary roots after
 * delivery (including any callee frame drops). */
const struct SZrTypeValue *function_get_frame_return_source_value(
        struct SZrState *state,
        const struct SZrCallInfo *callInfo,
        TZrStackValuePointer returnSource,
        TZrStackValuePointer returnDestination,
        struct SZrTypeValue *materializedValue);

#endif
