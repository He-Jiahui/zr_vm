//
// zr.system.env callbacks.
//

#include "zr_vm_lib_system/env.h"

#include "zr_vm_core/string.h"

#include <stdlib.h>

TZrBool ZrSystem_Env_GetVariable(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrString *nameString = ZR_NULL;
    const TZrChar *value;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrLib_CallContext_ReadString(context, 0, &nameString) || nameString == ZR_NULL) {
        return ZR_FALSE;
    }

    value = getenv(ZrCore_String_GetNativeString(nameString));
    if (value == ZR_NULL) {
        ZrLib_Value_SetNull(result);
    } else {
        ZrLib_Value_SetString(context->state, result, value);
    }
    return ZR_TRUE;
}
