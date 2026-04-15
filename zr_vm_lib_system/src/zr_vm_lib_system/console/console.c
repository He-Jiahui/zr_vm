//
// zr.system.console callbacks.
//

#include "zr_vm_lib_system/console.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdio.h>
#include <stdlib.h>

static TZrSize system_console_utf8_expected_length(TZrUInt8 firstByte) {
    if ((firstByte & 0x80u) == 0) {
        return 1;
    }
    if ((firstByte & 0xe0u) == 0xc0u) {
        return 2;
    }
    if ((firstByte & 0xf0u) == 0xe0u) {
        return 3;
    }
    if ((firstByte & 0xf8u) == 0xf0u) {
        return 4;
    }
    return 0;
}

static ZR_NO_RETURN void system_console_raise_invalid_utf8(ZrLibCallContext *context) {
    ZrCore_Debug_RunError(context != ZR_NULL ? context->state : ZR_NULL, "stdin contains invalid UTF-8");
}

static TZrBool system_console_print(ZrLibCallContext *context,
                                    SZrTypeValue *result,
                                    EZrOutputChannel channel,
                                    TZrBool appendNewLine) {
    SZrTypeValue *value;
    SZrString *text;
    TZrNativeString nativeText;
    TZrSize byteLength;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    text = value != ZR_NULL ? ZrCore_Value_ConvertToString(context->state, value) : ZR_NULL;
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeText = ZrCore_String_GetNativeString(text);
    byteLength = ZrCore_String_GetByteLength(text);
    if (!ZrCore_Utf8_IsValid(nativeText, byteLength)) {
        ZrCore_Debug_RunError(context->state, "invalid UTF-8 string");
    }

    if (appendNewLine) {
        ZrCore_Log_Printf(context->state,
                          channel == ZR_OUTPUT_CHANNEL_STDERR ? ZR_LOG_LEVEL_ERROR : ZR_LOG_LEVEL_INFO,
                          channel,
                          channel == ZR_OUTPUT_CHANNEL_STDERR ? ZR_OUTPUT_KIND_DIAGNOSTIC : ZR_OUTPUT_KIND_RESULT,
                          "%s\n",
                          nativeText);
    } else {
        ZrCore_Log_Write(context->state,
                         channel == ZR_OUTPUT_CHANNEL_STDERR ? ZR_LOG_LEVEL_ERROR : ZR_LOG_LEVEL_INFO,
                         channel,
                         channel == ZR_OUTPUT_CHANNEL_STDERR ? ZR_OUTPUT_KIND_DIAGNOSTIC : ZR_OUTPUT_KIND_RESULT,
                         nativeText);
    }

    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Console_Print(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDOUT, ZR_FALSE);
}

TZrBool ZrSystem_Console_PrintLine(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDOUT, ZR_TRUE);
}

TZrBool ZrSystem_Console_PrintError(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDERR, ZR_FALSE);
}

TZrBool ZrSystem_Console_PrintErrorLine(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDERR, ZR_TRUE);
}

TZrBool ZrSystem_Console_Read(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt8 buffer[ZR_STRING_UTF8_SIZE];
    TZrSize expectedLength;
    int firstByte;
    TZrUInt32 codePoint = 0;
    TZrSize consumedBytes = 0;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Log_FlushDefaultSinks();
    firstByte = fgetc(stdin);
    if (firstByte == EOF) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    buffer[0] = (TZrUInt8)firstByte;
    expectedLength = system_console_utf8_expected_length(buffer[0]);
    if (expectedLength == 0) {
        system_console_raise_invalid_utf8(context);
    }

    for (TZrSize index = 1; index < expectedLength; index++) {
        int nextByte = fgetc(stdin);
        if (nextByte == EOF) {
            system_console_raise_invalid_utf8(context);
        }
        buffer[index] = (TZrUInt8)nextByte;
    }

    if (!ZrCore_Utf8_DecodeCodePoint((TZrNativeString)buffer,
                                     expectedLength,
                                     &codePoint,
                                     &consumedBytes) ||
        consumedBytes != expectedLength) {
        system_console_raise_invalid_utf8(context);
    }

    ZrLib_Value_SetStringObject(context->state,
                                result,
                                ZrCore_String_Create(context->state, (TZrNativeString)buffer, expectedLength));
    return ZR_TRUE;
}

TZrBool ZrSystem_Console_ReadLine(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrChar *buffer = ZR_NULL;
    TZrSize length = 0;
    TZrSize capacity = 0;
    int nextByte;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Log_FlushDefaultSinks();
    while ((nextByte = fgetc(stdin)) != EOF) {
        TZrChar ch = (TZrChar)nextByte;

        if (ch == '\n') {
            break;
        }

        if (length + 1 >= capacity) {
            TZrSize newCapacity = capacity == 0 ? 64 : capacity * 2;
            TZrChar *newBuffer = (TZrChar *)realloc(buffer, newCapacity);

            if (newBuffer == ZR_NULL) {
                free(buffer);
                ZrCore_Debug_RunError(context->state, "not enough memory");
            }
            buffer = newBuffer;
            capacity = newCapacity;
        }

        buffer[length++] = ch;
    }

    if (nextByte == EOF && length == 0) {
        free(buffer);
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    if (length > 0 && buffer[length - 1] == '\r') {
        length--;
    }

    if (length > 0 && !ZrCore_Utf8_IsValid(buffer, length)) {
        free(buffer);
        system_console_raise_invalid_utf8(context);
    }

    ZrLib_Value_SetStringObject(context->state,
                                result,
                                ZrCore_String_Create(context->state, buffer != ZR_NULL ? buffer : "", length));
    free(buffer);
    return ZR_TRUE;
}
