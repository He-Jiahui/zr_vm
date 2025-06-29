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

TNativeString ZrNativeStringVFormat(struct SZrState *state, TNativeString format, va_list args) {
    SZrNativeStringFormatBuffer buffer;
    buffer.isOnStack = ZR_FALSE;
    buffer.length = 0;
    buffer.state = state;
    TChar *e;
    while ((e = ZrNativeStringCharFind(format, '%')) != ZR_NULL) {
        ZrNativeStringAddStringToBuffer(&buffer, format, e - format);
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
                // todo: convert integer to string
            }
            break;
            case 'f': {
                // todo: convert float to string
            }
            break;
            case 'o': {
                // todo: handle zr object
            }
            break;
            case 'p': {
                // todo: handle native pointer to string
            }
            break;
            case 'x': {
                // todo: handle utf8 string
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
    ZrNativeStringAddStringToBuffer(&buffer, format, ZrNativeStringLength(format));
    ZrNativeStringClearFormatBufferAndPushToStack(&buffer);
    ZR_ASSERT(buffer.isOnStack == ZR_TRUE);
    return ZR_CAST_STRING_TO_NATIVE(ZR_CAST_STRING(ZrStackGetValue(state->stackTop.valuePointer - 1)));
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

void ZrStringConcat(struct SZrState *state, TZrSize count) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(count);
}

