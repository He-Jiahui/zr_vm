//
// Created by HeJiahui on 2025/6/15.
//
#include "zr_vm_core/string.h"
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"

static void native_string_push_string_to_stack(SZrNativeStringFormatBuffer *buffer, TZrNativeString string, TZrSize length) {
    SZrState *state = buffer->state;
    SZrString *str = ZrCore_String_Create(state, string, length);
    ZrCore_Stack_SetRawObjectValue(buffer->state, state->stackTop.valuePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(str));
    state->stackTop.valuePointer += 1;
    if (!buffer->isOnStack) {
        buffer->isOnStack = ZR_TRUE;
    } else {
        ZrCore_String_Concat(state, 2);
    }
}

static void native_string_clear_format_buffer_and_push_to_stack(SZrNativeStringFormatBuffer *buffer) {
    native_string_push_string_to_stack(buffer, buffer->result, buffer->length);
    buffer->length = 0;
}

static TZrChar *native_string_get_from_format_string_buffer(SZrNativeStringFormatBuffer *buffer, TZrSize length) {
    ZR_ASSERT(buffer->length <= ZR_STRING_FORMAT_BUFFER_SIZE);
    ZR_ASSERT(length <= ZR_STRING_FORMAT_BUFFER_SIZE);
    if (length > ZR_STRING_FORMAT_BUFFER_SIZE - buffer->length) {
        native_string_clear_format_buffer_and_push_to_stack(buffer);
    }
    return buffer->result + buffer->length;
}

static TZrSize native_string_number_to_string_buffer(SZrTypeValue *value, TZrChar *buffer) {
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
            if (buffer[ZrCore_NativeString_Span(buffer, ZR_STRING_DECIMAL_NUMBER_SET)] == '\0') {
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

static void native_string_add_string_to_buffer(SZrNativeStringFormatBuffer *buffer, TZrNativeString string, TZrSize length) {
    if (length <= ZR_STRING_FORMAT_BUFFER_SIZE) {
        TZrChar *nextBuffer = native_string_get_from_format_string_buffer(buffer, length);
        ZrCore_Memory_RawCopy(nextBuffer, string, length * sizeof(TZrChar));
        buffer->length += length;
    } else {
        native_string_clear_format_buffer_and_push_to_stack(buffer);
        native_string_push_string_to_stack(buffer, string, length);
    }
}

static void native_string_add_number_to_buffer(SZrNativeStringFormatBuffer *buffer, SZrTypeValue *value) {
    TZrChar *numberBuffer = native_string_get_from_format_string_buffer(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX);
    TZrSize length = native_string_number_to_string_buffer(value, numberBuffer);
    buffer->length += length;
}

static TZrSize zr_string_length_local(const SZrString *string) {
    if (string == ZR_NULL) {
        return 0;
    }

    return string->shortStringLength < ZR_VM_LONG_STRING_FLAG
                   ? (TZrSize)string->shortStringLength
                   : string->longStringLength;
}

static void zr_string_collapse_stack_window(SZrState *state,
                                            TZrStackValuePointer firstSlot,
                                            SZrString *result) {
    if (state == ZR_NULL || firstSlot == ZR_NULL) {
        return;
    }

    if (result != ZR_NULL) {
        ZrCore_Stack_SetRawObjectValue(state, firstSlot, ZR_CAST_RAW_OBJECT_AS_SUPER(result));
    } else {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(firstSlot));
    }

    state->stackTop.valuePointer = firstSlot + 1;
}

static void zr_string_concat_stack_values(SZrState *state, TZrSize count, TZrBool convertNonStrings) {
    SZrGlobalState *global;
    TZrSize availableCount;
    TZrStackValuePointer firstSlot;
    SZrFunctionStackAnchor firstSlotAnchor;
    SZrString **stringValues = ZR_NULL;
    TZrNativeString buffer = ZR_NULL;
    TZrSize totalLength = 0;

    if (state == ZR_NULL || count == 0 || state->global == ZR_NULL) {
        return;
    }

    availableCount = (TZrSize)(state->stackTop.valuePointer - state->stackBase.valuePointer);
    if (availableCount < count) {
        return;
    }

    global = state->global;
    firstSlot = state->stackTop.valuePointer - count;
    ZrCore_Function_StackAnchorInit(state, firstSlot, &firstSlotAnchor);
    if (count == 1) {
        SZrTypeValue *value = ZrCore_Stack_GetValue(firstSlot);
        SZrString *singleString = ZR_NULL;

        if (value == ZR_NULL) {
            zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
            return;
        }

        if (value->type == ZR_VALUE_TYPE_STRING) {
            state->stackTop.valuePointer = firstSlot + 1;
            return;
        }

        if (!convertNonStrings) {
            zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
            return;
        }

        singleString = ZrCore_Value_ConvertToString(state, value);
        firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
        zr_string_collapse_stack_window(state, firstSlot, singleString);
        return;
    }

    stringValues = (SZrString **)ZrCore_Memory_RawMallocWithType(global,
                                                           count * sizeof(SZrString *),
                                                           ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (stringValues == ZR_NULL) {
        zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
        return;
    }

    for (TZrSize i = 0; i < count; i++) {
        SZrTypeValue *value = ZrCore_Stack_GetValue(firstSlot + i);
        SZrString *stringValue = ZR_NULL;

        if (value == ZR_NULL) {
            goto concat_fail;
        }

        if (value->type == ZR_VALUE_TYPE_STRING) {
            stringValue = ZR_CAST_STRING(state, value->value.object);
        } else if (convertNonStrings) {
            stringValue = ZrCore_Value_ConvertToString(state, value);
            firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
        }

        if (stringValue == ZR_NULL) {
            goto concat_fail;
        }

        stringValues[i] = stringValue;
        totalLength += zr_string_length_local(stringValue);
    }

    buffer = (TZrNativeString)ZrCore_Memory_RawMallocWithType(global,
                                                      totalLength + 1,
                                                      ZR_MEMORY_NATIVE_TYPE_STRING);
    if (buffer == ZR_NULL) {
        goto concat_fail;
    }

    {
        TZrChar *cursor = buffer;
        for (TZrSize i = 0; i < count; i++) {
            SZrString *stringValue = stringValues[i];
            TZrSize length = zr_string_length_local(stringValue);
            TZrNativeString nativeString = ZrCore_String_GetNativeString(stringValue);

            if (length > 0 && nativeString != ZR_NULL) {
                memcpy(cursor, nativeString, length);
                cursor += length;
            }
        }
        *cursor = '\0';
    }

    {
        SZrString *result = ZrCore_String_Create(state, buffer, totalLength);
        firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
        ZrCore_Memory_RawFreeWithType(global,
                                buffer,
                                totalLength + 1,
                                ZR_MEMORY_NATIVE_TYPE_STRING);
        ZrCore_Memory_RawFreeWithType(global,
                                stringValues,
                                count * sizeof(SZrString *),
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        zr_string_collapse_stack_window(state, firstSlot, result);
        return;
    }

concat_fail:
    firstSlot = ZrCore_Function_StackAnchorRestore(state, &firstSlotAnchor);
    if (buffer != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                buffer,
                                totalLength + 1,
                                ZR_MEMORY_NATIVE_TYPE_STRING);
    }
    if (stringValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global,
                                stringValues,
                                count * sizeof(SZrString *),
                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    zr_string_collapse_stack_window(state, firstSlot, ZR_NULL);
}

TZrNativeString ZrCore_NativeString_VFormat(struct SZrState *state, TZrNativeString format, va_list args) {
    SZrNativeStringFormatBuffer buffer;
    buffer.isOnStack = ZR_FALSE;
    buffer.length = 0;
    buffer.state = state;
    TZrChar *e;
    while ((e = ZrCore_NativeString_CharFind(format, '%')) != ZR_NULL) {
        TZrSize prefixLength = e - format;
        if (prefixLength > 0) {
            native_string_add_string_to_buffer(&buffer, format, e - format);
        }
        // get next char from '%'
        switch (*(e + 1)) {
            case 's': {
                TZrNativeString string = va_arg(args, TZrNativeString);
                if (string == ZR_NULL) {
                    TZrNativeString nullString = ZR_STRING_NULL_STRING;
                    native_string_add_string_to_buffer(&buffer, nullString, ZrCore_NativeString_Length(nullString));
                } else {
                    native_string_add_string_to_buffer(&buffer, string, ZrCore_NativeString_Length(string));
                }
            } break;
            case 'c': {
                TZrChar ch = ZR_CAST_CHAR(va_arg(args, TZrInt64));
                native_string_add_string_to_buffer(&buffer, &ch, sizeof(TZrChar));
            } break;
            case 'd': {
                // convert integer to string
                SZrTypeValue value;
                ZrCore_Value_InitAsInt(state, &value, va_arg(args, TZrInt64));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'u': {
                // convert unsigned to string
                SZrTypeValue value;
                ZrCore_Value_InitAsUInt(state, &value, va_arg(args, TZrUInt64));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'f': {
                // convert float to string
                SZrTypeValue value;
                ZrCore_Value_InitAsFloat(state, &value, va_arg(args, TZrFloat64));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'o': {
                // todo: handle zr object
            } break;
            case 'p': {
                // handle native pointer to string
                SZrTypeValue value;
                ZrCore_Value_InitAsNativePointer(state, &value, va_arg(args, TZrPtr));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'U': {
                // handle utf8 char
                TZrChar uCharBuffer[ZR_STRING_UTF8_SIZE];
                TZrSize length = ZrCore_NativeString_Utf8CharLength(uCharBuffer, ZR_CAST_UINT64(va_arg(args, TZrInt64)));
                native_string_add_string_to_buffer(&buffer, uCharBuffer + ZR_STRING_UTF8_SIZE - length, length);
            } break;
            case '%': {
                native_string_add_string_to_buffer(&buffer, "%", sizeof(TZrChar));
            } break;
            default: {
                // todo: handle exception
            } break;
        }
        format = e + 2;
    }
    TZrSize suffixLength = ZrCore_NativeString_Length(format);
    if (suffixLength > 0) {
        native_string_add_string_to_buffer(&buffer, format, ZrCore_NativeString_Length(format));
    }
    native_string_clear_format_buffer_and_push_to_stack(&buffer);
    ZR_ASSERT(buffer.isOnStack == ZR_TRUE);
    return ZR_CAST_STRING_TO_NATIVE(
            ZR_CAST_STRING(state, ZrCore_Value_GetRawObject(ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1))));
}

TZrNativeString ZrCore_NativeString_Format(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;
    va_start(args, format);
    TZrNativeString result = ZrCore_NativeString_VFormat(state, format, args);
    va_end(args);
    return result;
}

void ZrCore_StringTable_New(SZrGlobalState *global) {
    SZrStringTable *stringTable =
            ZrCore_Memory_RawMallocWithType(global, sizeof(SZrStringTable), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    global->stringTable = stringTable;
    // stringTable->bucketSize = 0;
    // stringTable->elementCount = 0;
    // stringTable->capacity = 0;
    // stringTable->buckets = ZR_NULL;
    ZrCore_HashSet_Construct(&stringTable->stringHashSet);
}

void ZrCore_StringTable_Free(struct SZrGlobalState *global, SZrStringTable *stringTable) {
    SZrState *mainThread = global->mainThreadState;

    ZrCore_HashSet_Deconstruct(mainThread, &stringTable->stringHashSet);

    // todo: clear all strings
    ZrCore_Memory_RawFreeWithType(global, stringTable, sizeof(SZrStringTable), ZR_MEMORY_NATIVE_TYPE_MANAGER);
    // ZR_MEMORY_NATIVE_TYPE_MANAGER);
}

void ZrCore_StringTable_Init(SZrState *state) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = global->stringTable;
    // stringTable
    ZrCore_HashSet_Init(state, &stringTable->stringHashSet, ZR_STRING_TABLE_INIT_SIZE_LOG2);
    // this is the first string we created
    global->memoryErrorMessage = ZR_STRING_LITERAL(state, ZR_ERROR_MESSAGE_NOT_ENOUGH_MEMORY);
    ZrCore_RawObject_MarkAsPermanent(state, ZR_CAST_RAW_OBJECT_AS_SUPER(global->memoryErrorMessage));
    // fill api cache with valid string
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STR_CACHE_N; i++) {
        for (TZrSize j = 0; j < ZR_GLOBAL_API_STR_CACHE_M; j++) {
            global->stringHashApiCache[i][j] = global->memoryErrorMessage;
        }
    }
    stringTable->isValid = ZR_TRUE;
}


static SZrString *string_object_create(SZrState *state, TZrNativeString string, TZrSize length, TZrUInt64 hash) {
    SZrGlobalState *global = state->global;
    SZrString *constantString = ZR_NULL;
    TZrSize totalSize = sizeof(SZrString);
    TZrNativeString stringBuffer = ZR_NULL;
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        totalSize += ZR_VM_SHORT_STRING_MAX;
        constantString = (SZrString *) ZrCore_RawObject_New(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        if (length > 0) {
            ZrCore_Memory_RawCopy(constantString->stringDataExtend, string, length);
        }
        ((TZrNativeString) constantString->stringDataExtend)[length] = '\0';
        constantString->shortStringLength = (TZrUInt8) length;
        constantString->nextShortString = ZR_NULL;
        stringBuffer = (TZrNativeString) constantString->stringDataExtend;
    } else {
        totalSize += sizeof(TZrNativeString);
        constantString = (SZrString *) ZrCore_RawObject_New(state, ZR_VALUE_TYPE_STRING, totalSize, ZR_TRUE);
        TZrNativeString *pointer = (TZrNativeString *) &(constantString->stringDataExtend);
        *pointer = (TZrNativeString) ZrCore_Memory_RawMallocWithType(global, length + 1, ZR_MEMORY_NATIVE_TYPE_STRING);

        ZrCore_Memory_RawCopy(*pointer, string, length);

        (*pointer)[length] = '\0';
        constantString->shortStringLength = ZR_VM_LONG_STRING_FLAG;
        constantString->longStringLength = length;
        stringBuffer = *pointer;
    }

    ZrCore_RawObject_InitHash(ZR_CAST_RAW_OBJECT_AS_SUPER(constantString),
                        hash == 0 ? ZrCore_Hash_Create(global, stringBuffer, length) : hash);
    return constantString;
}

static SZrString *string_create_short(SZrState *state, TZrNativeString string, TZrSize length) {
    SZrGlobalState *global = state->global;
    SZrStringTable *stringTable = global->stringTable;
    TZrUInt64 hash = ZrCore_Hash_Create(global, string, length);
    SZrHashKeyValuePair *object = ZrCore_HashSet_GetBucket(&stringTable->stringHashSet, hash);
    ZR_ASSERT(string != ZR_NULL);
    for (; object != ZR_NULL; object = object->next) {
        ZR_ASSERT(object->key.type == ZR_VALUE_TYPE_STRING);
        SZrRawObject *rawObject = ZrCore_Value_GetRawObject(&object->key);
        SZrString *stringObject = ZR_CAST_STRING(state, rawObject);
        // we customized string compare function for speed
        if (stringObject->shortStringLength == length &&
            ZrCore_Memory_RawCompare(ZrCore_String_GetNativeStringShort(stringObject), string, length * sizeof(TZrChar)) == 0) {
            if (ZrCore_RawObject_IsReleased(ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject))) {
                ZrCore_RawObject_MarkAsReferenced(ZR_CAST_RAW_OBJECT_AS_SUPER(stringObject));
            }
            return stringObject;
        }
    }
    {
        // create a new string
        SZrString *newString = string_object_create(state, string, length, hash);
        ZrCore_HashSet_AddRawObject(state, &stringTable->stringHashSet, &newString->super);
        return newString;
    }
}

static ZR_FORCE_INLINE SZrString *string_create_long(SZrState *state, TZrNativeString string, TZrSize length) {
    ZR_ASSERT(string != ZR_NULL);
    SZrString *newString = string_object_create(state, string, length, 0);
    return newString;
}


SZrString *ZrCore_String_Create(SZrState *state, TZrNativeString string, TZrSize length) {
    if (length <= ZR_VM_SHORT_STRING_MAX) {
        return string_create_short(state, string, length);
    }
    {
        // create a long string
        return string_create_long(state, string, length);
    }
}

SZrString *ZrCore_String_CreateTryHitCache(SZrState *state, TZrNativeString string) {
    SZrGlobalState *global = state->global;
    TZrUInt64 addressHash = ZR_CAST_UINT64(string) % ZR_GLOBAL_API_STR_CACHE_N;
    SZrString **apiCache = global->stringHashApiCache[addressHash];
    for (TZrSize i = 0; i < ZR_GLOBAL_API_STR_CACHE_M; i++) {
        if (ZrCore_NativeString_Compare(ZR_CAST_STRING_TO_NATIVE(apiCache[i]), string) == 0) {
            return apiCache[i];
        }
    }
    // replace cache
    for (TZrSize i = ZR_GLOBAL_API_STR_CACHE_M - 1; i > 0; i--) {
        apiCache[i] = apiCache[i - 1];
    }
    apiCache[0] = ZrCore_String_Create(state, string, ZrCore_NativeString_Length(string));
    return apiCache[0];
}

TZrBool ZrCore_String_Equal(SZrString *string1, SZrString *string2) {
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
        return ZrCore_Memory_RawCompare(*ZrCore_String_GetNativeStringLong(string1), *ZrCore_String_GetNativeStringLong(string2),
                                  string1->longStringLength * sizeof(TZrChar)) == 0;
    }

    // short string
    if (string1->super.hash != string2->super.hash) {
        return ZR_FALSE;
    }
    return ZrCore_Memory_RawCompare(ZrCore_String_GetNativeStringShort(string1), ZrCore_String_GetNativeStringShort(string2),
                              string1->shortStringLength * sizeof(TZrChar)) == 0;
}

void ZrCore_String_Concat(struct SZrState *state, TZrSize count) {
    zr_string_concat_stack_values(state, count, ZR_FALSE);
}

void ZrCore_String_ConcatSafe(struct SZrState *state, TZrSize count) {
    zr_string_concat_stack_values(state, count, ZR_TRUE);
}

SZrString *ZrCore_String_FromNumber(struct SZrState *state, struct SZrTypeValue *value) {
    SZrGlobalState *global = state->global;
    TZrSize length = 0;
    SZrString *string = ZR_NULL;
    ZR_ASSERT(ZR_VALUE_IS_TYPE_NUMBER(value->type) || ZR_VALUE_IS_TYPE_NATIVE(value->type));
    TZrNativeString nativeString =
            ZrCore_Memory_RawMallocWithType(global, ZR_NUMBER_TO_STRING_LENGTH_MAX, ZR_MEMORY_NATIVE_TYPE_STRING);
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
    string = ZrCore_String_CreateFromNative(state, nativeString);
    ZrCore_Memory_RawFreeWithType(global, nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX, ZR_MEMORY_NATIVE_TYPE_STRING);
    return string;
}
