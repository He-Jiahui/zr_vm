//
// Created by HeJiahui on 2025/6/15.
//

#ifndef ZR_VM_CORE_EXECUTION_H
#define ZR_VM_CORE_EXECUTION_H
#include "zr_vm_core/conf.h"
struct SZrState;
struct SZrCallInfo;
struct SZrTypeValue;
ZR_CORE_API void ZrCore_Execute(struct SZrState *state, struct SZrCallInfo *callInfo);
ZR_CORE_API TZrBool ZrCore_Execution_Add(struct SZrState *state,
                                         struct SZrCallInfo *callInfo,
                                         struct SZrTypeValue *destination,
                                         const struct SZrTypeValue *opA,
                                         const struct SZrTypeValue *opB);
ZR_CORE_API TZrBool ZrCore_Execution_ToObject(struct SZrState *state,
                                              struct SZrCallInfo *callInfo,
                                              struct SZrTypeValue *destination,
                                              const struct SZrTypeValue *source,
                                              const struct SZrTypeValue *typeNameValue);
ZR_CORE_API TZrBool ZrCore_Execution_ToStruct(struct SZrState *state,
                                              struct SZrCallInfo *callInfo,
                                              struct SZrTypeValue *destination,
                                              const struct SZrTypeValue *source,
                                              const struct SZrTypeValue *typeNameValue);


#endif //ZR_VM_CORE_EXECUTION_H
