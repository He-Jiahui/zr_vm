//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"

static void ZrNativeStringPushStringToStack(SZrNativeStringFormatBuffer *buffer, TNativeString string, TZrSize length) {
    SZrState *state = buffer->state;
    TZrString *str = ZrStringCreate(state, string, length);
    ZrStackSetRawObjectValue(buffer->state, state->stackTop.valuePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(str));
    state->stackTop.valuePointer += 1;
    if (!buffer->isOnStack) {
        buffer->isOnStack = ZR_TRUE;
    } else {
        ZrStringConcat(state, 2);
    }
}

static void ZrNativeStringClearFormatBufferAndPushToStack(SZrNativeStringFormatBuffer *buffer) {
    ZrNativeStringPushStringToStack(buffer, buffer->result, buffer->length);
    buffer->length = 0;
}

static TChar *ZrNativeStringGetFromFormatStringBuffer(SZrNativeStringFormatBuffer *buffer, TZrSize length) {
    ZR_ASSERT(buffer->length <= ZR_STRING_FORMAT_BUFFER_SIZE);
    ZR_ASSERT(length <= ZR_STRING_FORMAT_BUFFER_SIZE);
    if (length > ZR_STRING_FORMAT_BUFFER_SIZE - buffer->length) {
        ZrNativeStringClearFormatBufferAndPushToStack(buffer);
    }
    return buffer->result + buffer->length;
}

static TZrSize ZrNativeStringNumberToStringBuffer(SZrTypeValue *value, TChar *buffer) {
    TZrSize length = 0;
    ZR_ASSERT(ZR_VALUE_IS_TYPE_NUMBER(value->type) || ZR_VALUE_IS_TYPE_NATIVE(value->type));
    switch (value->type) {
        ZR_VALUE_CASES_SIGNED_INT {
            length = ZR_STRING_SIGNED_INTEGER_PRINT_FORMAT(buffer, ZR_NUMER_TO_STRING_LENGTH_MAX,
                                                           value->value.nativeObject.nativeInt64);
        }
        break;
        ZR_VALUE_CASES_UNSIGNED_INT {
            length = ZR_STRING_UNSIGNED_INTEGER_PRINT_FORMAT(buffer, ZR_NUMER_TO_STRING_LENGTH_MAX,
                                                             value->value.nativeObject.nativeUInt64);
        }
        break;
        ZR_VALUE_CASES_FLOAT {
            length = ZR_STRING_FLOAT_PRINT_FORMAT(buffer, ZR_NUMER_TO_STRING_LENGTH_MAX,
                                                  value->value.nativeObject.nativeDouble);
            // add .0 if no decimal point
            if (buffer[ZrNativeStringSpan(buffer, ZR_STRING_DECIMAL_NUMBER_SET)] == '\0') {
                buffer[length++] = ZR_STRING_LOCALE_DECIMAL_POINT;
                buffer[length++] = '0';
            }
        }
        break;
        ZR_VALUE_CASES_NATIVE {
            length = ZR_STRING_POINTER_PRINT_FORMAT(buffer, ZR_NUMER_TO_STRING_LENGTH_MAX,
                                                    value->value.nativeObject.nativePointer);
        }
        break;
        default: {
            ZR_ASSERT(ZR_FALSE);
        }
        break;
    }
    return length;
}

static void ZrNativeStringAddStringToBuffer(SZrNativeStringFormatBuffer *buffer, TNativeString string, TZrSize length) {
    if (length <= ZR_STRING_FORMAT_BUFFER_SIZE) {
        TChar *nextBuffer = ZrNativeStringGetFromFormatStringBuffer(buffer, length);
        ZrMemoryRawCopy(nextBuffer, string, length * sizeof(TChar));
        buffer->length += length;
    } else {
        ZrNativeStringClearFormatBufferAndPushToStack(buffer);
        ZrNativeStringPushStringToStack(buffer, string, length);
    }
}

static void ZrNativeStringAddNumberToBuffer(SZrNativeStringFormatBuffer *buffer, SZrTypeValue *value) {
    TChar *numberBuffer = ZrNativeStringGetFromFormatStringBuffer(buffer, ZR_NUMER_TO_STRING_LENGTH_MAX);
    TZrSize length = ZrNativeStringNumberToStringBuffer(value, numberBuffer);
    buffer->length += length;
}

TNativeString ZrNativeStringVFormat(struct SZrState *state, TNativeString format, va_list args) {
    SZrNativeStringFormatBuffer buffer;
    buffer.isOnStack = ZR_FALSE;
    buffer.length = 0;
    buffer.state = state;
    TChar *e;
    while ((e = ZrNativeStringCharFind(format, '%')) != ZR_NULL) {
        TZrSize prefixLength = e - format;
        if (prefixLength > 0) {
            ZrNativeStringAddStringToBuffer(&buffer, format, e - format);
        }
        // get next char from '%'
        switch (*(e + 1)) {
            case 's': {
                TNativeString string = va_arg(args, TNativeString);
                if (string == ZR_NULL) {
                    TNativeString nullString = ZR_STRING_NULL_STRING;
                    ZrNativeStringAddStringToBuffer(&buffer, nullString, ZrNativeStringLength(nullString));
                } else {
                    ZrNativeStringAddStringToBuffer(&buffer, string, ZrNativeStringLength(string));
                }
            }
            break;
            case 'c': {
                TChar ch = ZR_CAST_CHAR(va_arg(args, TInt64));
                ZrNativeStringAddStringToBuffer(&buffer, &ch, sizeof(TChar));
            }
            break;
            case 'd': {
                // convert integer to string
                SZrTypeValue value;
                ZrValueInitAsInt(state, &value, va_arg(args, TInt64));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            }
            break;
            case 'u': {
                // convert unsigned to string
                SZrTypeValue value;
                ZrValueInitAsUInt(state, &value, va_arg(args, TUInt64));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            }
            break;
            case 'f': {
                // convert float to string
                SZrTypeValue value;
                ZrValueInitAsFloat(state, &value, va_arg(args, TFloat64));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            }
            break;
            case 'o': {
                // todo: handle zr object
            }
            break;
            case 'p': {
                // handle native pointer to string
                SZrTypeValue value;
                ZrValueInitAsNativePointer(state, &value, va_arg(args, TZrPtr));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            }
            break;
            case 'U': {
                // handle utf8 char
                TChar uCharBuffer[ZR_STRING_UTF8_SIZE];
                TZrSize length = ZrNativeStringUtf8CharLength(uCharBuffer, ZR_CAST_UINT64(va_arg(args, TInt64)));
                ZrNativeStringAddStringToBuffer(&buffer, uCharBuffer + ZR_STRING_UTF8_SIZE - length, length);
            }
            break;
            case '%': {
                ZrNativeStringAddStringToBuffer(&buffer, "%", sizeof(TChar));
            }
            break;
            default: {
                // todo: handle exception
            }
            break;
        }
        format = e + 2;
    }
    TZrSize suffixLength = ZrNativeStringLength(format);
    if (suffixLength > 0) {
        ZrNativeStringAddStringToBuffer(&buffer, format, ZrNativeStringLength(format));
    }
    ZrNativeStringClearFormatBufferAndPushToStack(&buffer);
    ZR_ASSERT(buffer.isOnStack == ZR_TRUE);
    return ZR_CAST_STRING_TO_NATIVE(
        ZR_CAST_STRING(ZrValueGetRawObject(ZrStackGetValue(state->stackTop.valuePointer - 1))));
}

TNativeString ZrNativeStringFormat(struct SZrState *state, TNativeString format, ...) {
    va_list args;
    va_start(args, format);
    TNativeString result = ZrNativeStringVFormat(state, format, args);
    va_end(args);
    return result;
}

void ZrStringTableNew(SZrGlobalState *global) {
    SZrStringTable *stringTable = &global->stringTable;
    // stringTable->bucketCount = 0;
    // stringTable->elementCount = 0;
    // stringTable->capacity = 0;
    // stringTable->buckets = ZR_NULL;
    ZrHashSetNew(&stringTable->stringHashSet);
}

void ZrStringTableInit(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = &global->stringTable;
    // stringTable
    ZrHashSetInit(global, &stringTable->stringHashSet, ZR_STRING_TABLE_INIT_SIZE_LOG2);
    // this is the first string we created
    global->memoryErrorMessage = ZR_STRING_LITERAL(state, ZR_ERROR_MESSAGE_NOT_ENOUGH_MEMORY);
    ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->memoryErrorMessage));
    // fill api cache with valid string
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STR_CACHE_N; i++) {
        for (TZrSize j = 0; j < ZR_GLOBAL_API_STR_CACHE_M; j++) {
            global->stringHashApiCache[i][j] = global->memoryErrorMessage;
        }
    }
}

static TZrString *ZrStringObjectCreate(SZrState *state, TNativeString string, TZrSize length) {
    SZrGlobalState *global = state->global;
    TZrString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(TZrString);

    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (TZrString *) ZrRawObjectNew(state, ZR_VALUE_TYPE_STRING, totalSize);
        ZrMemoryRawCopy(constantString->stringDataExtend, string, length);
        ((TNativeString) constantString->stringDataExtend)[length] = '\0';
        constantString->shortStringLength = (TUInt8) length;
        constantString->nextShortString = ZR_NULL;
    } else {
        totalSize += sizeof(TNativeString);
        constantString = (TZrString *) ZrRawObjectNew(state, ZR_VALUE_TYPE_STRING, totalSize);
        TNativeString *pointer = (TNativeString *) &(constantString->stringDataExtend);
        *pointer = (TNativeString) ZrMemoryRawMalloc(global, length + 1);
        ZrMemoryRawCopy(*pointer, string, length);
        *pointer[length] = '\0';
        constantString->shortStringLength = ZR_VM_LONG_STRING_FLAG;
        constantString->longStringLength = length;
    }

    ZrHashRawObjectInit(&constantString->super, ZR_VALUE_TYPE_STRING, ZrHashCreate(global, string, length));
    return constantString;
}

static TZrString *ZrStringCreateShort(SZrState *state, TNativeString string, TZrSize length) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = &global->stringTable;
    TUInt64 hash = ZrHashCreate(global, string, length);
    TZrString *object = ZR_CAST(TZrString *, ZrHashSetGetBucket(&stringTable->stringHashSet, hash));
    ZR_ASSERT(string != ZR_NULL);
    for (; object != ZR_NULL; object = ZR_CAST(TZrString *, object->super.next)) {
        if (object->shortStringLength == length && ZrMemoryRawCompare(ZrStringGetNativeStringShort(object), string,
                                                                      length * sizeof(TChar)) == 0) {
            if (ZrRawObjectIsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(object))) {
                ZrRawObjectMarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(object));
            }
            return object;
        }
    } {
        // create a new string
        TZrString *newString = ZrStringObjectCreate(state, string, length);
        ZrHashSetAdd(global, &stringTable->stringHashSet, &newString->super);
        return newString;
    }
}

static ZR_FORCE_INLINE TZrString *ZrStringCreateLong(SZrState *state, TNativeString string, TZrSize length) {
    ZR_ASSERT(string != ZR_NULL);
    TZrString *newString = ZrStringObjectCreate(state, string, length);
    return newString;
}


ZR_CORE_API TZrString *ZrStringCreate(SZrState *state, TNativeString string, TZrSize length) {
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        return ZrStringCreateShort(state, string, length);
    } {
        // create a long string
        return ZrStringCreateLong(state, string, length);
    }
}

TZrString *ZrStringCreateTryHitCache(SZrState *state, TNativeString string) {
    SZrGlobalState *global = state->global;
    TUInt64 addressHash = ZR_CAST_UINT64(string) % ZR_GLOBAL_API_STR_CACHE_N;
    TZrString **apiCache = global->stringHashApiCache[addressHash];
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STR_CACHE_M; i++) {
        if (ZrNativeStringCompare(ZR_CAST_STRING_TO_NATIVE(apiCache[i]), string) == 0) {
            return apiCache[i];
        }
    }
    // replace cache
    for (TZrSize i = ZR_GLOBAL_API_STR_CACHE_M - 1; i > 0; i--) {
        apiCache[i] = apiCache[i - 1];
    }
    apiCache[0] = ZrStringCreate(state, string, ZrNativeStringLength(string));
    return apiCache[0];
}

TBool ZrStringEqual(TZrString *string1, TZrString *string2) {
    if (string1 == string2) {
        return ZR_TRUE;
    }
    if (string1->shortStringLength != string2->shortStringLength) {
        return ZR_FALSE;
    }
    if (string1->shortStringLength == ZR_VM_LONG_STRING_FLAG) {
        // this is a long string
        if (string1->longStringLength != string2->longStringLength) {
            return ZR_FALSE;
        }
        return ZrMemoryRawCompare(*ZrStringGetNativeStringLong(string1), *ZrStringGetNativeStringLong(string2),
                                  string1->longStringLength * sizeof(TChar)) == 0;
    }

    // short string
    if (string1->super.hash != string2->super.hash) {
        return ZR_FALSE;
    }
    return ZrMemoryRawCompare(ZrStringGetNativeStringShort(string1), ZrStringGetNativeStringShort(string2),
                              string1->shortStringLength * sizeof(TChar)) == 0;
}

void ZrStringConcat(struct SZrState *state, TZrSize count) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(count);
}


