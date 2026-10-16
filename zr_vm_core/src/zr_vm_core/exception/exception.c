//
// Created by HeJiahui on 2025/6/18.
//

#include "zr_vm_core/exception.h"
#include "exception/exception_internal.h"
#include "exception_error_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc_domain.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/log.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

/* Stack traces expose 0 as "no mapped source line" instead of the debug-hook-only 0xFFFFFFFF sentinel. */
/* 零同时写入结构化帧与格式化默认行 */
#define ZR_EXCEPTION_SOURCE_LINE_NONE ((TZrUInt32)0u)
static void exception_throw_on_state(SZrState *state, EZrThreadStatus errorCode);

/* state 必须非空；本地恢复先还原入口 GC scopes，转发分支采用整域 unwind；没有恢复点最终 abort。 */
static void exception_throw_impl(
        SZrState *state, EZrThreadStatus errorCode, TZrBool restoreLocalRootSnapshot) {
    if (state->exceptionRecoverPoint != ZR_NULL) {
        SZrExceptionLongJump *recoverPoint = state->exceptionRecoverPoint;
        /*
         * Some native paths (for example legacy IO helpers) longjmp with a thread status without
         * first materializing currentException. Test harness and VM diagnostics assume
         * hasCurrentException matches recoverable error codes before unwinding.
         * Debug_RunError already normalizes; this covers raw Throw sites and keeps threadStatus
         * consistent with exceptionLongJump.status.
         */
        /* Error construction may itself throw MEMORY_ERROR. Reentry must reach
         * this recovery point without allocating another Error. Keep status
         * untouched until the actual throw so foreign C++ exceptions retain
         * the catch-all's INVALID fallback. */
        if (!state->hasCurrentException &&
            (errorCode == ZR_THREAD_STATUS_RUNTIME_ERROR || errorCode == ZR_THREAD_STATUS_EXCEPTION_ERROR ||
             errorCode == ZR_THREAD_STATUS_MEMORY_ERROR) &&
            ZrCore_Exception_BeginStatusNormalization(recoverPoint)) {
            (void)ZrCore_Exception_NormalizeStatus(state, errorCode);
            ZrCore_Exception_EndStatusNormalization(recoverPoint);
        }
/* 入口域注册与外层scope必须存活；转发分支不读取主state私有快照 */
        state->threadStatus = errorCode;
        state->exceptionRecoverPoint->status = errorCode;
        if (restoreLocalRootSnapshot) {
            ZrCore_Exception_RestoreLocalTryRunScopes(state);
        } else {
            ZrCore_GcDomain_MutatorUnwindScopes(state);
        }
        ZR_EXCEPTION_NATIVE_THROW(state, state->exceptionRecoverPoint);
    }

    if (state == ZR_NULL || state->global == ZR_NULL) {
        ZR_ABORT();
    }

    /* TODO: ResetThread 清异常三字段后本分支才复制到 main；尚未找到合法的同原生线程 secondary/no-local-recovery 调用链。下一步核查宿主 state API、worker 入口及 root/close 回调的恢复点与线程归属，再判断此顺序是否造成可达的原 Error 丢失。 */
    if (state != state->global->mainThreadState) {
        errorCode = ZrCore_State_ResetThread(state, errorCode);
        state->threadStatus = errorCode;
        if (state->global->mainThreadState != ZR_NULL && state->global->mainThreadState->exceptionRecoverPoint != ZR_NULL) {
            state->global->mainThreadState->currentException = state->currentException;
            state->global->mainThreadState->currentExceptionStatus = state->currentExceptionStatus;
            state->global->mainThreadState->hasCurrentException = state->hasCurrentException;
            exception_throw_on_state(state->global->mainThreadState, errorCode);
        }
    }

/* panic回调借用state；返回不代表错误已恢复 */
    if (state->global->panicHandlingFunction != ZR_NULL) {
        ZR_THREAD_UNLOCK(state);
        state->global->panicHandlingFunction(state);
    }
    ZR_ABORT();
}

/* 这是旧转发路径，不读取主线程私有 TryRun 快照；真正跨原生线程 longjmp 不在本地恢复契约内。 */
static void exception_throw_on_state(SZrState *state, EZrThreadStatus errorCode) {
    /* This legacy forwarding path jumps to another thread's recovery point.
     * Its private TryRun root snapshot belongs to that thread, so do not access
     * or mutate it from the forwarding state. Cross-thread longjmp semantics
     * remain outside this root-lifecycle guarantee. */
    exception_throw_impl(state, errorCode, ZR_FALSE);
}

/* 抛出首先回当前线程的恢复点；没有恢复点的 worker 尝试向主线程转发。 */
/* 非空 state；不保证普通 C 局部资源清理，命中恢复点时即使 FINE 也非局部离开。 */
void ZrCore_Exception_Throw(SZrState *state, EZrThreadStatus errorCode) {
    exception_throw_impl(state, errorCode, ZR_TRUE);
}

/* TODO: 目前只回传 status，不按 level 停止嵌套层；需沿 State_ResetThread 的 level=1 调用核对停止层级预期。 */
/* state 与 level 当前均不参与处理；TODO: 沿 State_ResetThread 的 level=1 调用确认是否仍需要分层停止语义。 */
EZrThreadStatus ZrCore_Exception_TryStop(SZrState *state, TZrMemoryOffset level, EZrThreadStatus status) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(level);
    return status;
}

/* 把线程当前异常或最后的栈顶载荷移至调用结果槽，随后恢复 VM 栈顶。 */
/* previousTop 须属于有效可写栈且其前一槽可作 top；写完 stackTop=previousTop-1，未保证 top 指向结果之后。 */
void ZrCore_Exception_MarkError(SZrState *state, EZrThreadStatus errorCode, TZrStackValuePointer previousTop) {
    if (state == ZR_NULL || previousTop == ZR_NULL) {
        return;
    }

    switch (errorCode) {
        case ZR_THREAD_STATUS_FINE:
            ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(previousTop));
            break;
        case ZR_THREAD_STATUS_MEMORY_ERROR:
            if (state->hasCurrentException) {
                ZrCore_Stack_CopyValue(state, previousTop, &state->currentException);
            } else {
                ZrCore_Stack_SetRawObjectValue(state, previousTop,
                                         ZR_CAST_RAW_OBJECT_AS_SUPER(state->global->memoryErrorMessage));
            }
            break;
        default:
            if (state->hasCurrentException) {
                ZrCore_Stack_CopyValue(state, previousTop, &state->currentException);
            } else if (state->stackTop.valuePointer > state->stackBase.valuePointer) {
                ZrCore_Stack_CopyValue(state, previousTop, &(state->stackTop.valuePointer - 1)->value);
            } else {
                ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(previousTop));
            }
            break;
    }

    state->stackTop.valuePointer = previousTop - 1;
}

/* 同时清 value/status/has 标志；不改 threadStatus，不执行对象销毁或 handler 清理。 */
void ZrCore_Exception_ClearCurrent(struct SZrState *state) {
    if (state == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(&state->currentException);
    state->currentExceptionStatus = ZR_THREAD_STATUS_FINE;
    state->hasCurrentException = ZR_FALSE;
}

/* 调试位置表按指令顺序组织，异常与 AOT 诊断共享此最近前驱查找。 */
/* 依赖 executionLocationInfoList 按 instructionOffset 递增；无映射返回零，不使用 debug hook 的无行哨兵。 */
TZrUInt32 ZrCore_Exception_FindSourceLine(struct SZrFunction *function, TZrMemoryOffset instructionOffset) {
    TZrUInt32 bestLine = ZR_EXCEPTION_SOURCE_LINE_NONE;

    if (function == ZR_NULL || function->executionLocationInfoList == ZR_NULL || function->executionLocationInfoLength == 0) {
        return ZR_EXCEPTION_SOURCE_LINE_NONE;
    }

    for (TZrUInt32 index = 0; index < function->executionLocationInfoLength; index++) {
        SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
        if (info->currentInstructionOffset > instructionOffset) {
            break;
        }
        bestLine = info->lineInSource;
    }

    return bestLine;
}

/* 未捕获异常的可增长宿主文本缓冲区，最终由 PrintUnhandled/LogUnhandled 释放。 */
/* 不由 VM allocator 管理，格式化正常返回转交 Print/Log free */
typedef struct SZrExceptionTextBuilder {
/* 空初始化；成功后交输出函数free，reserve失败仍保留原块 */
    TZrChar *buffer;
/* 不含末尾零；append仅在reserve成功后增加 */
    TZrSize length;
/* 必须容纳length及终止零；reserve只增长，不表示有效文本长度 */
    TZrSize capacity;
} SZrExceptionTextBuilder;

/* 按需要扩充诊断文本；失败时保留旧缓冲区供调用方释放。 */
/* 容量含终止零；原块由最终格式化消费者释放；TODO: 沿 append/appendf 的长度加法及倍增核查超大诊断的 SIZE_MAX 边界。 */
static TZrBool exception_text_builder_reserve(SZrExceptionTextBuilder *builder, TZrSize requiredLength) {
    TZrChar *newBuffer;
    TZrSize newCapacity;

    if (builder == ZR_NULL) {
        return ZR_FALSE;
    }

    if (requiredLength <= builder->capacity) {
        return ZR_TRUE;
    }

    newCapacity = builder->capacity > 0 ? builder->capacity : 256;
    while (newCapacity < requiredLength) {
        newCapacity *= 2;
    }

    newBuffer = (TZrChar *)realloc(builder->buffer, newCapacity);
    if (newBuffer == ZR_NULL) {
        return ZR_FALSE;
    }

    builder->buffer = newBuffer;
    builder->capacity = newCapacity;
    return ZR_TRUE;
}

/* text 必须有效且不含待输出的内嵌零；reserve 失败不增加 length。 */
static TZrBool exception_text_builder_append(SZrExceptionTextBuilder *builder, const TZrChar *text) {
    TZrSize textLength;

    if (builder == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    textLength = strlen(text);
    if (!exception_text_builder_reserve(builder, builder->length + textLength + 1)) {
        return ZR_FALSE;
    }

    memcpy(builder->buffer + builder->length, text, textLength);
    builder->length += textLength;
    builder->buffer[builder->length] = '\0';
    return ZR_TRUE;
}

/* va_copy 用于测长度；失败分支结束两份参数列表，成功长度不包含末尾零。 */
static TZrBool exception_text_builder_appendf(SZrExceptionTextBuilder *builder, const TZrChar *format, ...) {
    va_list args;
    va_list copyArgs;
    int requiredLength;

    if (builder == ZR_NULL || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(args, format);
    va_copy(copyArgs, args);
    requiredLength = vsnprintf(ZR_NULL, 0, format, copyArgs);
    va_end(copyArgs);
    if (requiredLength < 0 ||
        !exception_text_builder_reserve(builder, builder->length + (TZrSize)requiredLength + 1)) {
        va_end(args);
        return ZR_FALSE;
    }

    vsnprintf(builder->buffer + builder->length,
              builder->capacity - builder->length,
              format,
              args);
    va_end(args);
    builder->length += (TZrSize)requiredLength;
    return ZR_TRUE;
}

/* 把 Error 对象或普通抛出值转换成宿主可输出文本；返回 malloc 缓冲区。 */
/* 优先已保存 stack 文本，再退回 stacks；成功返回 malloc/realloc 缓冲区，普通失败释放已建文本，VM Throw 可绕过该清理。 */
static TZrChar *exception_format_unhandled_text(struct SZrState *state, const SZrTypeValue *errorValue) {
    SZrObject *errorObject;
    const SZrTypeValue *messageValue;
    const SZrTypeValue *payloadValue;
    const SZrTypeValue *stackValue;
    const SZrTypeValue *stacksValue;
    const TZrChar *typeName = "Error";
    SZrString *payloadSummary = ZR_NULL;
    SZrExceptionTextBuilder builder;

    memset(&builder, 0, sizeof(builder));
    if (state == ZR_NULL || errorValue == ZR_NULL) {
        return ZR_NULL;
    }

    if (!exception_value_is_error_object(state, errorValue)) {
        payloadSummary = ZrCore_Value_ConvertToString(state, (SZrTypeValue *)errorValue);
        if (!exception_text_builder_appendf(&builder,
                                            "Unhandled exception: %s\n",
                                            payloadSummary != ZR_NULL ? ZrCore_String_GetNativeString(payloadSummary) : "<unknown>")) {
            free(builder.buffer);
            return ZR_NULL;
        }
        return builder.buffer;
    }

    errorObject = ZR_CAST_OBJECT(state, errorValue->value.object);
    if (errorObject != ZR_NULL && errorObject->prototype != ZR_NULL && errorObject->prototype->name != ZR_NULL) {
        typeName = ZrCore_String_GetNativeString(errorObject->prototype->name);
    }

    messageValue = exception_get_object_field_cstring(state, errorObject, "message");
    payloadValue = exception_get_object_field_cstring(state, errorObject, "exception");
    stackValue = exception_get_object_field_cstring(state, errorObject, "stack");
    stacksValue = exception_get_object_field_cstring(state, errorObject, "stacks");
    if (payloadValue != ZR_NULL) {
        payloadSummary = ZrCore_Value_ConvertToString(state, (SZrTypeValue *)payloadValue);
    }

    if (!exception_text_builder_append(&builder, typeName != ZR_NULL ? typeName : "Error")) {
        free(builder.buffer);
        return ZR_NULL;
    }
    if (messageValue != ZR_NULL && messageValue->type == ZR_VALUE_TYPE_STRING && messageValue->value.object != ZR_NULL) {
        if (!exception_text_builder_appendf(&builder,
                                            ": %s",
                                            ZrCore_String_GetNativeString(ZR_CAST_STRING(state, messageValue->value.object)))) {
            free(builder.buffer);
            return ZR_NULL;
        }
    }
    if (!exception_text_builder_append(&builder, "\n")) {
        free(builder.buffer);
        return ZR_NULL;
    }
    if (payloadSummary != ZR_NULL) {
        if (!exception_text_builder_appendf(&builder,
                                            "payload: %s\n",
                                            ZrCore_String_GetNativeString(payloadSummary))) {
            free(builder.buffer);
            return ZR_NULL;
        }
    }

/* 仅接受非空字符串；必要时补换行然后直接返回 */
    if (stackValue != ZR_NULL && stackValue->type == ZR_VALUE_TYPE_STRING && stackValue->value.object != ZR_NULL) {
        const TZrChar *stackText = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, stackValue->value.object));
        TZrSize stackLength = stackText != ZR_NULL ? strlen(stackText) : 0u;
        if (stackLength > 0u) {
            if (!exception_text_builder_append(&builder, stackText)) {
                free(builder.buffer);
                return ZR_NULL;
            }
            if (stackText[stackLength - 1u] != '\n' && !exception_text_builder_append(&builder, "\n")) {
                free(builder.buffer);
                return ZR_NULL;
            }
            return builder.buffer;
        }
    }

/* 物化失败释放宿主builder；可能的Throw不走这条普通失败清理 */
    if (stacksValue != ZR_NULL && stacksValue->type == ZR_VALUE_TYPE_ARRAY && stacksValue->value.object != ZR_NULL) {
        SZrObject *frames = ZR_CAST_OBJECT(state, stacksValue->value.object);
        if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, frames)) {
            free(builder.buffer);
            return ZR_NULL;
        }
        TZrSize frameCount = frames->nodeMap.elementCount;
        for (TZrSize index = 0; index < frameCount; index++) {
            SZrTypeValue key;
            const SZrTypeValue *frameValue;
            const SZrTypeValue *functionNameValue;
            const SZrTypeValue *sourceFileValue;
            const SZrTypeValue *sourceLineValue;
            const SZrTypeValue *instructionOffsetValue;
            const TZrChar *functionName = "<anonymous>";
            const TZrChar *sourceFile = "<unknown>";
            TZrInt64 sourceLine = (TZrInt64)ZR_EXCEPTION_SOURCE_LINE_NONE;
            TZrInt64 instructionOffset = 0;

            ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
            frameValue = ZrCore_Object_GetValue(state, frames, &key);
            if (frameValue == ZR_NULL || frameValue->type != ZR_VALUE_TYPE_OBJECT || frameValue->value.object == ZR_NULL) {
                continue;
            }

            functionNameValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "functionName");
            sourceFileValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "sourceFile");
            sourceLineValue = exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "sourceLine");
            instructionOffsetValue =
                    exception_get_object_field_cstring(state, ZR_CAST_OBJECT(state, frameValue->value.object), "instructionOffset");

            if (functionNameValue != ZR_NULL && functionNameValue->type == ZR_VALUE_TYPE_STRING && functionNameValue->value.object != ZR_NULL) {
                functionName = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, functionNameValue->value.object));
            }
            if (sourceFileValue != ZR_NULL && sourceFileValue->type == ZR_VALUE_TYPE_STRING && sourceFileValue->value.object != ZR_NULL) {
                sourceFile = ZrCore_String_GetNativeString(ZR_CAST_STRING(state, sourceFileValue->value.object));
            }
            if (sourceLineValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(sourceLineValue->type)) {
                sourceLine = sourceLineValue->value.nativeObject.nativeInt64;
            }
            if (instructionOffsetValue != ZR_NULL && ZR_VALUE_IS_TYPE_INT(instructionOffsetValue->type)) {
                instructionOffset = instructionOffsetValue->value.nativeObject.nativeInt64;
            }

            if (!exception_text_builder_appendf(&builder,
                                                "  at %s (%s:%lld, ip=%lld)\n",
                                                functionName != ZR_NULL ? functionName : "<anonymous>",
                                                sourceFile != ZR_NULL ? sourceFile : "<unknown>",
                                                (long long)sourceLine,
                                                (long long)instructionOffset)) {
                free(builder.buffer);
                return ZR_NULL;
            }
        }
    }

    return builder.buffer;
}

/* 空 FILE 选择 stderr；格式化普通失败不输出；正常输出后 free 文本，不关闭借用 stream。 */
void ZrCore_Exception_PrintUnhandled(struct SZrState *state, const SZrTypeValue *errorValue, FILE *stream) {
    FILE *output = stream != ZR_NULL ? stream : stderr;
    TZrChar *text = exception_format_unhandled_text(state, errorValue);

    if (output == ZR_NULL || text == ZR_NULL) {
        return;
    }

    fputs(text, output);
    fflush(output);
    free(text);
}

/* 按 EXCEPTION/STDERR/DIAGNOSTIC 分类发送；调用后释放宿主文本，路由须同步消费或复制文本。 */
void ZrCore_Exception_LogUnhandled(struct SZrState *state, const SZrTypeValue *errorValue) {
    TZrChar *text = exception_format_unhandled_text(state, errorValue);

    if (text == ZR_NULL) {
        return;
    }

    ZrCore_Log_Write(state,
                     ZR_LOG_LEVEL_EXCEPTION,
                     ZR_OUTPUT_CHANNEL_STDERR,
                     ZR_OUTPUT_KIND_DIAGNOSTIC,
                     text);
    free(text);
}
