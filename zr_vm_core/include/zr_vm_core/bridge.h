#ifndef ZR_VM_CORE_BRIDGE_H
#define ZR_VM_CORE_BRIDGE_H

#include "zr_vm_core/conf.h"

struct SZrCallInfo;
struct SZrState;
struct SZrTypeValue;

ZR_CORE_API TZrBool ZrCore_Bridge_BoxTyped(struct SZrState *state,
                                           struct SZrCallInfo *callInfo,
                                           struct SZrTypeValue *destination,
                                           const struct SZrTypeValue *source,
                                           const struct SZrTypeValue *typeNameValue);

ZR_CORE_API TZrBool ZrCore_Bridge_UnboxTyped(struct SZrState *state,
                                             struct SZrCallInfo *callInfo,
                                             struct SZrTypeValue *destination,
                                             const struct SZrTypeValue *source,
                                             const struct SZrTypeValue *typeNameValue);

#endif
