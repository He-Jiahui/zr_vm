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

/**
 * 给 UTF-8 首字节提供候选序列长度；不验证 continuation byte、过长编码、代理项或 Unicode 上界。
 * @param firstByte 已消费的首字节。
 * @return 1..4 表示候选长度；0 表示前缀形状不受支持。
 */
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

/**
 * 以 VM 运行时错误中止当前输入 callback；RunError 不返回。
 * @param context 提供当前 VM state 的借用调用上下文；允许空指针并转交给 RunError。
 */
static ZR_NO_RETURN void system_console_raise_invalid_utf8(ZrLibCallContext *context) {
    ZrCore_Debug_RunError(context != ZR_NULL ? context->state : ZR_NULL, "stdin contains invalid UTF-8");
}

/**
 * 将一个 VM 参数转为经 UTF-8 校验的 C 文本，再同步提交到选定日志 channel。
 * context/result 在 callback 期间借用；转换会执行脚本协议，可能重入或抛异常。ZR_TRUE 只表示日志 API 调用完成，
 * 不表示默认 FILE sink 或宿主 observer 成功消费；observer 的 message 仅在同步调用期间有效。
 */
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

    /* toString 可调用用户代码；若协议抛异常，后续 UTF-8 检查和日志提交均不会发生。 */
    value = ZrLib_CallContext_Argument(context, 0);
    text = value != ZR_NULL ? ZrCore_Value_ConvertToString(context->state, value) : ZR_NULL;
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    /* BUG: 先按显式 byteLength 验证完整 UTF-8，后续 sink 却用 strlen/%s；合法嵌入 NUL 会截断可见文本。 */
    nativeText = ZrCore_String_GetNativeString(text);
    byteLength = ZrCore_String_GetByteLength(text);
    if (!ZrCore_Utf8_IsValid(nativeText, byteLength)) {
        ZrCore_Debug_RunError(context->state, "invalid UTF-8 string");
    }

    /* 日志调用同步完成；void API 不回报 FILE/observer 写入错误，Printf 还会在 observer 返回后释放格式缓冲。 */
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

/** 绑定 `print`：写 stdout channel 且不添加换行，其他契约见公开声明。 */
TZrBool ZrSystem_Console_Print(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDOUT, ZR_FALSE);
}

/** 绑定 `printLine`：写 stdout channel 并添加换行，其他契约见公开声明。 */
TZrBool ZrSystem_Console_PrintLine(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDOUT, ZR_TRUE);
}

/** 绑定 `printError`：写 stderr channel 且不添加换行，其他契约见公开声明。 */
TZrBool ZrSystem_Console_PrintError(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDERR, ZR_FALSE);
}

/** 绑定 `printErrorLine`：写 stderr channel 并添加换行，其他契约见公开声明。 */
TZrBool ZrSystem_Console_PrintErrorLine(ZrLibCallContext *context, SZrTypeValue *result) {
    return system_console_print(context, result, ZR_OUTPUT_CHANNEL_STDERR, ZR_TRUE);
}

/**
 * 消费一个 UTF-8 候选序列，严格解码后把短字符串交给 VM result；结果槽由 dispatcher 预置为 null。
 * 首字节 EOF 正常返回 null，输入错误抛 VM 异常；短串驻留表扩容的 soft-OOM 路径另见下方 BUG。
 */
TZrBool ZrSystem_Console_Read(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrUInt8 buffer[ZR_STRING_UTF8_SIZE];
    TZrSize expectedLength;
    int firstByte;
    TZrUInt32 codePoint = 0;
    TZrSize consumedBytes = 0;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 只刷新默认 stdout/stderr sinks，不调用 custom log observer 的 flush；fgetc 可阻塞且此处不检查 ferror。 */
    ZrCore_Log_FlushDefaultSinks();
    firstByte = fgetc(stdin);
    if (firstByte == EOF) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    buffer[0] = (TZrUInt8)firstByte;
    /* 首字节 helper 只估算长度；后续 decoder 才检查完整 UTF-8 合法性。 */
    expectedLength = system_console_utf8_expected_length(buffer[0]);
    if (expectedLength == 0) {
        system_console_raise_invalid_utf8(context);
    }

    /* continuation bytes 逐个消费；EOF/流错误不回滚已读字节，再由 noreturn helper 报错。 */
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

    /* BUG: 合法码点的短串若在驻留表扩容时遇到 raw allocation OOM，String_Create 可回 null 不抛异常；result 保持
     * dispatcher 预置的 null，而 callback 仍返回 ZR_TRUE，故成功读到的码点会被误报为 EOF。 */
    ZrLib_Value_SetStringObject(context->state,
                                result,
                                ZrCore_String_Create(context->state, (TZrNativeString)buffer, expectedLength));
    return ZR_TRUE;
}

/**
 * 逐字节读取共享 stdin，累积到 LF/EOF 后校验并创建 VM 字符串；临时 realloc buffer 由此函数释放。
 * 行缓冲已分配后的字符串硬异常会绕过该释放，另有精确 BUG 说明；短串 soft-OOM 与 I/O EOF/error 混同亦在下方分列。
 */
TZrBool ZrSystem_Console_ReadLine(ZrLibCallContext *context, SZrTypeValue *result) {
    TZrChar *buffer = ZR_NULL;
    TZrSize length = 0;
    TZrSize capacity = 0;
    int nextByte;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 仅刷新默认 stdout/stderr；stdin 读取可无限阻塞，其他 observer 不参与 flush。 */
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
                /* realloc 失败发生在 String_Create 前；这里明确释放已累积缓冲后才抛异常。 */
                free(buffer);
                ZrCore_Debug_RunError(context->state, "not enough memory");
            }
            buffer = newBuffer;
            capacity = newCapacity;
        }

        buffer[length++] = ch;
    }

    /* TODO: 明确并测试 ferror 应与 EOF 共用 null/部分行策略，还是报告独立 I/O 错误；当前代码未查询 ferror。 */
    /* 首字节 EOF/error 都交付 null，部分输入遇到 EOF/error 都按结束行处理。 */
    if (nextByte == EOF && length == 0) {
        free(buffer);
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    /* TODO: 核对并测试 EOF 终止行是否应删除孤立尾 CR；当前 LF/EOF 两种结束都剥离一个尾 CR。 */
    /* 因此单独的 CR 后 EOF 也会丢弃该字节。 */
    if (length > 0 && buffer[length - 1] == '\r') {
        length--;
    }

    /* 校验按显式长度；非法字节路径先释放 C buffer，再通过 RunError 非局部抛出。 */
    if (length > 0 && !ZrCore_Utf8_IsValid(buffer, length)) {
        free(buffer);
        system_console_raise_invalid_utf8(context);
    }

    /* BUG: 仅当行缓冲已成功分配 buffer，且 String_Create 的 RawObject/GcMalloc 首次分配及 full-GC 重试均失败时，
     * MemoryError 非局部抛出会跳过紧随其后的 free(buffer)。初始空行/行首 EOF 没有 buffer；realloc OOM 和 UTF-8
     * 失败路径已在上方释放，均不属于此泄漏触发条件。 */
    /* BUG: 独立的短串驻留表 rehash raw OOM 可让 String_Create 返回 null 而不抛；result 保持预置 null，callback 仍成功，
     * 因而合法空行或短行可能被误报为 EOF。这个 soft-failure 与上一条硬 MemoryError/非局部跳转泄漏链不同。 */
    ZrLib_Value_SetStringObject(context->state,
                                result,
                                ZrCore_String_Create(context->state, buffer != ZR_NULL ? buffer : "", length));
    free(buffer);
    return ZR_TRUE;
}
