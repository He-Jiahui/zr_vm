//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"

static void ZrNativeStringPushStringToStack(SZrNativeStringFormatBuffer *buffer, TNativeString string, TZrSize length) {
    SZrState *state = buffer->state;
    SZrString *str = ZrStringCreate(state, string, length);
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
            length = ZR_STRING_SIGNED_INTEGER_PRINT_FORMAT(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                           value->value.nativeObject.nativeInt64);
        }
        break;
        ZR_VALUE_CASES_UNSIGNED_INT {
            length = ZR_STRING_UNSIGNED_INTEGER_PRINT_FORMAT(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                             value->value.nativeObject.nativeUInt64);
        }
        break;
        ZR_VALUE_CASES_FLOAT {
            length = ZR_STRING_FLOAT_PRINT_FORMAT(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                  value->value.nativeObject.nativeDouble);
            // add .0 if no decimal point
            if (buffer[ZrNativeStringSpan(buffer, ZR_STRING_DECIMAL_NUMBER_SET)] == '\0') {
                buffer[length++] = ZR_STRING_LOCALE_DECIMAL_POINT;
                buffer[length++] = '0';
            }
        }
        break;
        ZR_VALUE_CASES_NATIVE {
            length = ZR_STRING_POINTER_PRINT_FORMAT(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                    value->value.nativeObject.nativePointer);
        }
        break;
        default: {
            ZR_ASSERT(ZR_FALSE);
        } break;
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
    TChar *numberBuffer = ZrNativeStringGetFromFormatStringBuffer(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX);
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
            } break;
            case 'c': {
                TChar ch = ZR_CAST_CHAR(va_arg(args, TInt64));
                ZrNativeStringAddStringToBuffer(&buffer, &ch, sizeof(TChar));
            } break;
            case 'd': {
                // convert integer to string
                SZrTypeValue value;
                ZrValueInitAsInt(state, &value, va_arg(args, TInt64));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            } break;
            case 'u': {
                // convert unsigned to string
                SZrTypeValue value;
                ZrValueInitAsUInt(state, &value, va_arg(args, TUInt64));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            } break;
            case 'f': {
                // convert float to string
                SZrTypeValue value;
                ZrValueInitAsFloat(state, &value, va_arg(args, TFloat64));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            } break;
            case 'o': {
                // todo: handle zr object
            } break;
            case 'p': {
                // handle native pointer to string
                SZrTypeValue value;
                ZrValueInitAsNativePointer(state, &value, va_arg(args, TZrPtr));
                ZrNativeStringAddNumberToBuffer(&buffer, &value);
            } break;
            case 'U': {
                // handle utf8 char
                TChar uCharBuffer[ZR_STRING_UTF8_SIZE];
                TZrSize length = ZrNativeStringUtf8CharLength(uCharBuffer, ZR_CAST_UINT64(va_arg(args, TInt64)));
                ZrNativeStringAddStringToBuffer(&buffer, uCharBuffer + ZR_STRING_UTF8_SIZE - length, length);
            } break;
            case '%': {
                ZrNativeStringAddStringToBuffer(&buffer, "%", sizeof(TChar));
            } break;
            default: {
                // todo: handle exception
            } break;
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
            ZR_CAST_STRING(state, ZrValueGetRawObject(ZrStackGetValue(state->stackTop.valuePointer - 1))));
}

TNativeString ZrNativeStringFormat(struct SZrState *state, TNativeString format, ...) {
    va_list args;
    va_start(args, format);
    TNativeString result = ZrNativeStringVFormat(state, format, args);
    va_end(args);
    return result;
}

void ZrStringTableNew(SZrGlobalState *global) {
    SZrStringTable *stringTable =
            ZrMemoryRawMallocWithType(global, sizeof(SZrStringTable), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    global->stringTable = stringTable;
    // stringTable->bucketSize = 0;
    // stringTable->elementCount = 0;
    // stringTable->capacity = 0;
    // stringTable->buckets = ZR_NULL;
    ZrHashSetConstruct(&stringTable->stringHashSet);
}

void ZrStringTableFree(struct SZrGlobalState *global, SZrStringTable *stringTable) {
    SZrState *mainThread = global->mainThreadState;

    ZrHashSetDeconstruct(mainThread, &stringTable->stringHashSet);

    // todo: clear all strings
    ZrMemoryRawFreeWithType(global, stringTable, sizeof(SZrStringTable), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    // ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

void ZrStringTableInit(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = global->stringTable;
    // stringTable
    ZrHashSetInit(state, &stringTable->stringHashSet, ZR_STRING_TABLE_INIT_SIZE_LOG2);
    // this is the first string we created
    global->memoryErrorMessage = ZR_STRING_LITERAL(state, ZR_ERROR_MESSAGE_NOT_ENOUGH_MEMORY);
    ZrRawObjectMarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->memoryErrorMessage));
    // fill api cache with valid string
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STR_CACHE_N; i++) {
        for (TZrSize j = 0; j < ZR_GLOBAL_API_STR_CACHE_M; j++) {
            global->stringHashApiCache[i][j] = global->memoryErrorMessage;
        }
    }
    stringTable->isValid = ZR_TRUE;
}


static SZrString *ZrStringObjectCreate(SZrState *state, TNativeString string, TZrSize length, TUInt64 hash) {
    SZrGlobalState *global = state->global;
    SZrString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(SZrString);
    TNativeString stringBuffer = ZR_NULL;
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (SZrString *) ZrRawObjectNew(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        if (length > 0) {
            ZrMemoryRawCopy(constantString->stringDataExtend, string, length);
        }
        ((TNativeString) constantString->stringDataExtend)[length] = '\0';
        constantString->shortStringLength = (TUInt8) length;
        constantString->nextShortString = ZR_NULL;
        stringBuffer = (TNativeString) constantString->stringDataExtend;
    } else {
        totalSize += sizeof(TNativeString);
        constantString = (SZrString *) ZrRawObjectNew(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        TNativeString *pointer = (TNativeString *) &(constantString->stringDataExtend);
        *pointer = (TNativeString) ZrMemoryRawMallocWithType(global, length + 1, ZR_MEMORY_NATIVE_TYPE_STRING);

        ZrMemoryRawCopy(*pointer, string, length);

        (*pointer)[length] = '\0';
        constantString->shortStringLength = ZR_VM_LONG_STRING_FLAG;
        constantString->longStringLength = length;
        stringBuffer = *pointer;
    }

    ZrRawObjectInitHash(ZR_CAST_RAW_OBJECT_AS_SUPER(constantString),
                        hash == 0 ? ZrHashCreate(global, stringBuffer, length) : hash);
    return constantString;
}

static SZrString *ZrStringCreateShort(SZrState *state, TNativeString string, TZrSize length) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = global->stringTable;
    TUInt64 hash = ZrHashCreate(global, string, length);
    SZrHashKeyValuePair *object = ZrHashSetGetBucket(&stringTable->stringHashSet, hash);
    ZR_ASSERT(string != ZR_NULL);
    for (; object != ZR_NULL; object = object->next) {
        ZR_ASSERT(object->key.type == ZR_VALUE_TYPE_STRING);
        SZrRawObject *rawObject = ZrValueGetRawObject(&object->key);
        SZrString *stringObject = ZR_CAST_STRING(state, rawObject);
        // we customized string compare function for speed
        if (stringObject->shortStringLength == length &&
            ZrMemoryRawCompare(ZrStringGetNativeStringShort(stringObject), string, length * sizeof(TChar)) == 0) {
            if (ZrRawObjectIsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject))) {
                ZrRawObjectMarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
            }
            return stringObject;
        }
    }
    {
        // create a new string
        SZrString *newString = ZrStringObjectCreate(state, string, length, hash);
        ZrHashSetAddRawObject(state, &stringTable->stringHashSet, &newString->super);
        return newString;
    }
}

static ZR_FORCE_INLINE SZrString *ZrStringCreateLong(SZrState *state, TNativeString string, TZrSize length) {
    ZR_ASSERT(string != ZR_NULL);
    SZrString *newString = ZrStringObjectCreate(state, string, length, 0);
    return newString;
}


SZrString *ZrStringCreate(SZrState *state, TNativeString string, TZrSize length) {
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        return ZrStringCreateShort(state, string, length);
    }
    {
        // create a long string
        return ZrStringCreateLong(state, string, length);
    }
}

SZrString *ZrStringCreateTryHitCache(SZrState *state, TNativeString string) {
    SZrGlobalState *global = state->global;
    TUInt64 addressHash = ZR_CAST_UINT64(string) % ZR_GLOBAL_API_STR_CACHE_N;
    SZrString **apiCache = global->stringHashApiCache[addressHash];
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

TBool ZrStringEqual(SZrString *string1, SZrString *string2) {
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
    // todo:
    if (count == 0) {
        return;
    }
    // do {
    //     // TZrStackValuePointer top = state->stackTop.valuePointer;
    //     // int n = 2;
    //     // ZrStackGetValue(top - 2);
    //     // if () {
    //     // }
    // } while (count > 0);
}

void ZrStringConcatSafe(struct SZrState *state, TZrSize count) {
    // todo:
}

SZrString *ZrStringFromNumber(struct SZrState *state, struct SZrTypeValue *value) {
    SZrGlobalState *global = state->global;
    TZrSize length = 0;
    SZrString *string = ZR_NULL;
    ZR_ASSERT(ZR_VALUE_IS_TYPE_NUMBER(value->type) || ZR_VALUE_IS_TYPE_NATIVE(value->type));
    TNativeString nativeString =
            ZrMemoryRawMallocWithType(global, ZR_NUMBER_TO_STRING_LENGTH_MAX, ZR_MEMORY_NATIVE_TYPE_STRING);
    switch (value->type) {
        ZR_VALUE_CASES_SIGNED_INT {
            length = ZR_STRING_SIGNED_INTEGER_PRINT_FORMAT(nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                           value->value.nativeObject.nativeInt64);
        }
        break;
        ZR_VALUE_CASES_UNSIGNED_INT {
            length = ZR_STRING_UNSIGNED_INTEGER_PRINT_FORMAT(nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                             value->value.nativeObject.nativeUInt64);
        }
        break;
        ZR_VALUE_CASES_FLOAT {
            length = ZR_STRING_FLOAT_PRINT_FORMAT(nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                  value->value.nativeObject.nativeDouble);
        }
        break;
        ZR_VALUE_CASES_NATIVE {
            length = ZR_STRING_POINTER_PRINT_FORMAT(nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX,
                                                    value->value.nativeObject.nativePointer);
        }
        break;
        default: {
            ZR_ASSERT(ZR_FALSE);
        } break;
    }
    string = ZrStringCreateFromNative(state, nativeString);
    ZrMemoryRawFreeWithType(global, nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX, ZR_MEMORY_NATIVE_TYPE_STRING);
    return string;
}
