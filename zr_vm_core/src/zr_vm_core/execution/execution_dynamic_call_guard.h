#ifndef ZR_VM_CORE_EXECUTION_DYNAMIC_CALL_GUARD_H
#define ZR_VM_CORE_EXECUTION_DYNAMIC_CALL_GUARD_H

#include "zr_vm_core/value.h"

/* Match generic precall admission without staging a receiver or invoking it. */
static ZR_FORCE_INLINE TZrBool execution_dynamic_call_is_noncallable(
        struct SZrState *state, SZrTypeValue *value) {
    if (value == ZR_NULL) return ZR_TRUE;
    switch (value->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_NATIVE_POINTER:
        case ZR_VALUE_TYPE_NATIVE_DATA:
            return ZR_FALSE;
        default:
            return ZrCore_Value_GetMeta(state, value, ZR_META_CALL) == ZR_NULL;
    }
}

#endif
