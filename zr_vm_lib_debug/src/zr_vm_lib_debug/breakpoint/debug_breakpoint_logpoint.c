#include "breakpoint/debug_breakpoint_logpoint.h"

/* 多段插值共享同一个固定上限的输出缓冲区，保留尾端 NUL 给协议发送者。 */
static void zr_debug_breakpoint_logpoint_append(TZrChar *buffer,
                                                TZrSize bufferSize,
                                                const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || bufferSize == 0u || text == ZR_NULL) {
        return;
    }

    length = strlen(buffer);
    if (length >= bufferSize - 1u) {
        return;
    }

    snprintf(buffer + length, bufferSize - length, "%s", text);
    buffer[bufferSize - 1u] = '\0';
}

/* logpoint 由断点命中路径调用，表达式失败只影响该片段的展示，不取消事件。 */
TZrBool zr_debug_breakpoint_logpoint_format(ZrDebugAgent *agent,
                                            const TZrChar *logMessage,
                                            TZrChar *outText,
                                            TZrSize outTextSize) {
    const TZrChar *cursor;

    if (outText != ZR_NULL && outTextSize > 0u) {
        outText[0] = '\0';
    }
    if (agent == ZR_NULL || logMessage == ZR_NULL || outText == ZR_NULL || outTextSize == 0u) {
        return ZR_FALSE;
    }

    cursor = logMessage;
    while (*cursor != '\0') {
        const TZrChar *openBrace = strchr(cursor, '{');
        if (openBrace == ZR_NULL) {
            zr_debug_breakpoint_logpoint_append(outText, outTextSize, cursor);
            break;
        }

        if (openBrace > cursor) {
            TZrChar chunk[ZR_DEBUG_TEXT_CAPACITY];
            TZrSize length = (TZrSize)(openBrace - cursor);

            if (length >= sizeof(chunk)) {
                length = sizeof(chunk) - 1u;
            }
            memcpy(chunk, cursor, length);
            chunk[length] = '\0';
            zr_debug_breakpoint_logpoint_append(outText, outTextSize, chunk);
        }

        {
            const TZrChar *closeBrace = strchr(openBrace + 1, '}');
            if (closeBrace == ZR_NULL) {
                zr_debug_breakpoint_logpoint_append(outText, outTextSize, openBrace);
                break;
            }

            TZrChar expression[ZR_DEBUG_TEXT_CAPACITY];
            SZrTypeValue value;
            TZrChar valueText[ZR_DEBUG_TEXT_CAPACITY];
            TZrChar error[256];
            TZrSize expressionLength = (TZrSize)(closeBrace - (openBrace + 1));

            if (expressionLength >= sizeof(expression)) {
                expressionLength = sizeof(expression) - 1u;
            }
            memcpy(expression, openBrace + 1, expressionLength);
            expression[expressionLength] = '\0';

            /* 普通求值失败转成错误片段并继续模板；片段和尾换行仍受输出容量限制。 */
            memset(&value, 0, sizeof(value));
            if (zr_debug_evaluate_expression_with_capabilities(agent,
                                                                1u,
                                                                expression,
                                                                ZR_DEBUG_EVALUATION_EFFECT_NONE,
                                                                ZR_FALSE,
                                                                &value,
                                                                ZR_NULL,
                                                                ZR_NULL,
                                                                error,
                                                                sizeof(error),
                                                                ZR_NULL,
                                                                0u)) {
                zr_debug_format_value_text_safe(agent->state, &value, valueText, sizeof(valueText));
                zr_debug_breakpoint_logpoint_append(outText, outTextSize, valueText);
            } else {
                snprintf(valueText, sizeof(valueText), "<error:%.*s>", (int)(sizeof(valueText) - 9u), error);
                zr_debug_breakpoint_logpoint_append(outText, outTextSize, valueText);
            }

            cursor = closeBrace + 1;
        }
    }

    zr_debug_breakpoint_logpoint_append(outText, outTextSize, "\n");
    return ZR_TRUE;
}

/**
 * @fn void zr_debug_breakpoint_logpoint_append(TZrChar *buffer, TZrSize bufferSize, const TZrChar *text)
 * ## Summary
 * 把多个字面片段、值预览和错误片段拼入同一调用方缓冲，给模板格式化提供统一的容量边界。
 *
 * ## Parameters
 * @param[in,out] buffer 借用已 NUL 终止的累积文本；可为 NULL，此时不写入。非 NULL 时原内容必须在 bufferSize 内终止。
 * @param[in] bufferSize buffer 的可写字节容量；为零时立即返回。
 * @param[in] text 借用 NUL 结尾片段；可为 NULL，此时不追加；不得与写入范围产生 snprintf 不支持的重叠。
 *
 * ## Returns
 * 通过 buffer 交付追加结果，无返回值；无效参数或已有内容占满可见容量时不追加，也不报告截断。
 *
 * ## Constraints and lifecycle
 * 只拼接字节，不解释表达式、不分配资源、不做 Unicode 边界修复。有效缓冲保留末端 NUL；容量不足会截断当前片段。调用者先建立空串，后续各次追加依赖这个不变量；不保证完整错误标记或末尾换行。
 *
 * ## References
 * - zr_debug_breakpoint_logpoint_format — zr_vm_lib_debug/src/zr_vm_lib_debug/breakpoint/debug_breakpoint_logpoint.c:40 首段及其余各片段均经本辅助函数追加。
 */
/**
 * @fn TZrBool zr_debug_breakpoint_logpoint_format(ZrDebugAgent *agent, const TZrChar *logMessage, TZrChar *outText, TZrSize outTextSize)
 * ## Summary
 * 把 logpoint 模板拼成待发送文本；只负责格式化，console output 事件由命中路径另行发送。每个普通求值失败变成错误片段，后续模板仍继续处理。
 *
 * ## Parameters
 * @param[in,out] agent 借用非 NULL agent；每次插值可能通过其 state 分配临时解析或语义数据，不转移 agent/state 所有权。
 * @param[in] logMessage 借用 NUL 结尾模板，必须非 NULL；与 outText 不得重叠，整个同步调用期间保持有效。
 * @param[out] outText 借用输出缓冲，必须非 NULL 且容量为正；有效时先置为空串，之后交付可能截断的结果。
 * @param[in] outTextSize outText 的可写字节容量；零容量是无效请求。
 *
 * ## Returns
 * @return 假仅表示本层请求参数无效；普通插值失败或输出截断仍可返回真。真不保证输出完整、存在完整 <error:...> 标记或保留末尾换行，也不表示事件已发送。
 *
 * ## Constraints and lifecycle
 * 按第一个 { 与其后第一个 } 配对；未闭合的 { 及后文按字面量追加，不支持嵌套或转义花括号。字面片段和表达式各受 ZR_DEBUG_TEXT_CAPACITY 临时数组截断；被截断表达式实际只求值复制后的前缀。插值固定 frameId=1、零 capability、禁用兼容；值预览不调用用户转换。即使 outText 已满仍扫描并尝试后续插值，最后只尝试追加换行。普通失败以 <error:...> 片段继续，Throw/abort 不保证返回或转成文本；本层无 TryRun、无整体回滚，也不提供并发或重入隔离。
 *
 * ## References
 * - zr_debug_breakpoint_emit_logpoint — zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:1358 仅在格式化成功后发送 console 事件。
 * - zr_debug_format_value_text_safe — zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:374 借用值生成不执行用户代码的预览。
 * - test_debug_breakpoint_logpoint_requires_pure_formal_evaluation — tests/debug/test_debug_breakpoint_logpoint_cases.h:12 成功插值和能力拒绝转错误片段。
 */
