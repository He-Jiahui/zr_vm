/* Native formatting and numeric conversion. String storage/interning remains
 * in string.c; this translation unit uses the existing public String API. */
#include "zr_vm_core/string.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
#include <stdarg.h>
#include <stdio.h>
/* 格式化片段进入值栈后才可触发后续拼接和 GC；buffer 记录该栈根的存在。 */
static void native_string_push_string_to_stack(SZrNativeStringFormatBuffer *buffer, TZrNativeString string, TZrSize length) {
    SZrState *state = buffer->state;
    /* Reserve before creation: the new GC object is not rooted until published. */
    if (!ZrCore_Stack_Grow(state, 1, ZR_TRUE)) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        return;
    }
    SZrString *str = ZrCore_String_Create(state, string, length);
    if (str == ZR_NULL) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        return;
    }
    ZrCore_Stack_SetRawObjectValue(buffer->state, state->stackTop.valuePointer, ZR_CAST_RAW_OBJECT_AS_SUPER(str));
    ZrCore_Stack_GetValueNoProfile(state->stackTop.valuePointer)->type = ZR_VALUE_TYPE_STRING;
    state->stackTop.valuePointer += 1;
    if (!buffer->isOnStack) {
        buffer->isOnStack = ZR_TRUE;
    } else {
        ZrCore_String_Concat(state, 2);
        SZrTypeValue *result = ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1);
        if (result->type != ZR_VALUE_TYPE_STRING || ZrCore_Value_GetRawObject(result) == ZR_NULL) {
            ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        }
    }
}

/* 窗口满时把片段转为受值栈保护的 GC 字符串，再继续收集后续片段。 */
static void native_string_clear_format_buffer_and_push_to_stack(SZrNativeStringFormatBuffer *buffer) {
    native_string_push_string_to_stack(buffer, buffer->result, buffer->length);
    buffer->length = 0;
}

/* 为下一片段保留连续暂存窗口；长度不得超过固定缓冲容量。 */
static TZrChar *native_string_get_from_format_string_buffer(SZrNativeStringFormatBuffer *buffer, TZrSize length) {
    ZR_ASSERT(buffer->length <= ZR_STRING_FORMAT_BUFFER_SIZE);
    ZR_ASSERT(length <= ZR_STRING_FORMAT_BUFFER_SIZE);
    if (length > ZR_STRING_FORMAT_BUFFER_SIZE - buffer->length) {
        native_string_clear_format_buffer_and_push_to_stack(buffer);
    }
    return buffer->result + buffer->length;
}

/* 格式化器共用数字和原生指针文本策略；调用方保证输出窗口足够。 */
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

/* 按固定窗口分流短片段和长片段，长片段直接以受栈保护的字符串参与合并。 */
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

/* 数值转文本写入格式化窗口，沿用 FromNumber 的类型集合。 */
static void native_string_add_number_to_buffer(SZrNativeStringFormatBuffer *buffer, SZrTypeValue *value) {
    TZrChar *numberBuffer = native_string_get_from_format_string_buffer(buffer, ZR_NUMBER_TO_STRING_LENGTH_MAX);
    TZrSize length = native_string_number_to_string_buffer(value, numberBuffer);
    buffer->length += length;
}

/* 原生格式化入口；占位参数必须遵守本函数的 va_arg 类型，结果暂留栈顶。 */
typedef struct SZrNativeStringFormatContext {
    TZrNativeString format;
    va_list args;
    TZrNativeString result;
} SZrNativeStringFormatContext;

static void native_string_format_run(SZrState *state, TZrPtr arguments) {
    SZrNativeStringFormatContext *context = arguments;
    TZrNativeString format = context->format;
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
                TZrNativeString string = va_arg(context->args, TZrNativeString);
                if (string == ZR_NULL) {
                    TZrNativeString nullString = ZR_STRING_NULL_STRING;
                    native_string_add_string_to_buffer(&buffer, nullString, ZrCore_NativeString_Length(nullString));
                } else {
                    native_string_add_string_to_buffer(&buffer, string, ZrCore_NativeString_Length(string));
                }
            } break;
            case 'c': {
                TZrChar ch = ZR_CAST_CHAR(va_arg(context->args, TZrInt64));
                native_string_add_string_to_buffer(&buffer, &ch, sizeof(TZrChar));
            } break;
            case 'd': {
                // convert integer to string
                SZrTypeValue value;
                ZrCore_Value_InitAsInt(state, &value, va_arg(context->args, TZrInt64));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'u': {
                // convert unsigned to string
                SZrTypeValue value;
                ZrCore_Value_InitAsUInt(state, &value, va_arg(context->args, TZrUInt64));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'f': {
                // convert float to string
                SZrTypeValue value;
                ZrCore_Value_InitAsFloat(state, &value, va_arg(context->args, TZrFloat64));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'o': {
                /* TODO: 此占位符尚未定义对象文本及参数消费契约；确认是否应拒绝或实现。 */
            } break;
            case 'p': {
                // handle native pointer to string
                SZrTypeValue value;
                ZrCore_Value_InitAsNativePointer(state, &value, va_arg(context->args, TZrPtr));
                native_string_add_number_to_buffer(&buffer, &value);
            } break;
            case 'U': {
                // handle utf8 char
                TZrChar uCharBuffer[ZR_STRING_UTF8_SIZE];
                TZrSize length = 0;
                if (ZrCore_Utf8_EncodeCodePoint((TZrUInt32)ZR_CAST_UINT64(va_arg(context->args, TZrInt64)), uCharBuffer, &length)) {
                    native_string_add_string_to_buffer(&buffer, uCharBuffer, length);
                }
            } break;
            case '%': {
                native_string_add_string_to_buffer(&buffer, "%", sizeof(TZrChar));
            } break;
            default: {
                /* TODO: 非法占位符当前静默丢弃；明确诊断与 va_arg 类型约束。 */
            } break;
        }
        /* TODO: 尾随孤立 '%' 当前令 format=e+2 越过 NUL；核 public 格式串契约并补 malformed-format fixture，再定拒绝/诊断语义。 */
        format = e + 2;
    }
    TZrSize suffixLength = ZrCore_NativeString_Length(format);
    if (suffixLength > 0) {
        native_string_add_string_to_buffer(&buffer, format, ZrCore_NativeString_Length(format));
    }
    native_string_clear_format_buffer_and_push_to_stack(&buffer);
    ZR_ASSERT(buffer.isOnStack == ZR_TRUE);
    context->result = ZrCore_String_GetNativeString(
            ZR_CAST_STRING(state, ZrCore_Value_GetRawObject(ZrCore_Stack_GetValue(state->stackTop.valuePointer - 1))));
}

/* The context and copied va_list belong to the caller of the setjmp boundary. */
static EZrThreadStatus native_string_format_execute(SZrState *state, TZrNativeString format,
                                                   va_list args, TZrNativeString *result) {
    SZrNativeStringFormatContext context;
    TZrMemoryOffset savedTop = ZrCore_Stack_SavePointerAsOffset(state, state->stackTop.valuePointer);
    context.format = format;
    context.result = ZR_NULL;
    va_copy(context.args, args);
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, native_string_format_run, &context);
    va_end(context.args);
    if (status != ZR_THREAD_STATUS_FINE) {
        state->stackTop.valuePointer = ZrCore_Stack_LoadOffsetToPointer(state, savedTop);
    }
    *result = context.result;
    return status;
}

TZrNativeString ZrCore_NativeString_VFormat(struct SZrState *state, TZrNativeString format, va_list args) {
    TZrNativeString result;
    EZrThreadStatus status = native_string_format_execute(state, format, args, &result);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
    }
    return result;
}

/* 可变参数包装入口，与 VFormat 共享栈顶结果和占位符约束。 */
TZrNativeString ZrCore_NativeString_Format(struct SZrState *state, TZrNativeString format, ...) {
    va_list args;
    va_start(args, format);
    TZrNativeString result;
    EZrThreadStatus status = native_string_format_execute(state, format, args, &result);
    va_end(args);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
    }
    return result;
}

typedef struct SZrNumberStringCreation {
    TZrNativeString text;
    TZrSize length;
    SZrString *result;
} SZrNumberStringCreation;

static void native_string_create_number_run(SZrState *state, TZrPtr arguments) {
    SZrNumberStringCreation *context = arguments;
    context->result = ZrCore_String_Create(state, context->text, context->length);
}

/* Value_ConvertToString 和元方法的数字/指针转换入口；暂存字节由本函数释放。 */
SZrString *ZrCore_String_FromNumber(struct SZrState *state, struct SZrTypeValue *value) {
    SZrGlobalState *global = state->global;
    TZrSize length = 0;
    SZrString *string = ZR_NULL;
    ZR_ASSERT(ZR_VALUE_IS_TYPE_NUMBER(value->type) || ZR_VALUE_IS_TYPE_NATIVE(value->type));
    TZrNativeString nativeString =
            ZrCore_Memory_RawMallocWithType(global, ZR_NUMBER_TO_STRING_LENGTH_MAX, ZR_MEMORY_NATIVE_TYPE_STRING);
    if (nativeString == ZR_NULL) {
        return ZR_NULL;
    }
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
    SZrNumberStringCreation context = {nativeString, length, ZR_NULL};
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, native_string_create_number_run, &context);
    ZrCore_Memory_RawFreeWithType(global, nativeString, ZR_NUMBER_TO_STRING_LENGTH_MAX, ZR_MEMORY_NATIVE_TYPE_STRING);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
    }
    string = context.result;
    return string;
}
