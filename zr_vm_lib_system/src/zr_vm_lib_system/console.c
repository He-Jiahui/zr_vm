//
// zr.system.console callbacks.
//

#include "zr_vm_lib_system/console.h"

#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>

static TZrBool system_console_print_to_stream(ZrLibCallContext *context,
                                              SZrTypeValue *result,
                                              FILE *stream,
                                              TZrBool appendNewLine) {
    SZrTypeValue *value;
    SZrString *text;

    if (context == ZR_NULL || result == ZR_NULL || stream == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    text = value != ZR_NULL ? ZrCore_Value_ConvertToString(context->state, value) : ZR_NULL;
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    fputs(ZrCore_String_GetNativeString(text), stream);
    if (appendNewLine) {
        fputc('\n', stream);
    }
    fflush(stream);
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Console_Print(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print_to_stream(context, result, stdout, ZR_FALSE);
}

TZrBool ZrSystem_Console_PrintLine(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print_to_stream(context, result, stdout, ZR_TRUE);
}

TZrBool ZrSystem_Console_PrintError(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print_to_stream(context, result, stderr, ZR_FALSE);
}

TZrBool ZrSystem_Console_PrintErrorLine(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print_to_stream(context, result, stderr, ZR_TRUE);
}
