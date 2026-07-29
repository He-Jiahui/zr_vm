#ifndef ZR_VM_CORE_FUNCTION_CALL_SPREAD_H
#define ZR_VM_CORE_FUNCTION_CALL_SPREAD_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

ZR_CORE_API TZrBool ZrCore_Function_CallSpread_TryGetArgumentCount(
        const SZrTypeValue *spreadValue,
        TZrSize *outArgumentCount);

ZR_CORE_API TZrBool ZrCore_Function_CallSpread_ExpandPrepared(
        struct SZrState *state,
        TZrStackValuePointer callWindow,
        TZrSize prefixArgumentCount,
        TZrSize *outArgumentCount);

#endif
