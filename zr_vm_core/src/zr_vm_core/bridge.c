#include "zr_vm_core/bridge.h"

#include "zr_vm_core/execution.h"

TZrBool ZrCore_Bridge_BoxTyped(struct SZrState *state,
                               struct SZrCallInfo *callInfo,
                               struct SZrTypeValue *destination,
                               const struct SZrTypeValue *source,
                               const struct SZrTypeValue *typeNameValue) {
    return ZrCore_Execution_ToObject(state, callInfo, destination, source, typeNameValue);
}

TZrBool ZrCore_Bridge_UnboxTyped(struct SZrState *state,
                                 struct SZrCallInfo *callInfo,
                                 struct SZrTypeValue *destination,
                                 const struct SZrTypeValue *source,
                                 const struct SZrTypeValue *typeNameValue) {
    return ZrCore_Execution_ToStruct(state, callInfo, destination, source, typeNameValue);
}
