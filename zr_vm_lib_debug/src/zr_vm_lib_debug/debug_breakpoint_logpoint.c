#include "debug_breakpoint_logpoint.h"

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

            memset(&value, 0, sizeof(value));
            if (zr_debug_evaluate_expression_with_capabilities(agent,
                                                                1u,
                                                                expression,
                                                                ZR_DEBUG_EVALUATION_EFFECT_NONE,
                                                                ZR_FALSE,
                                                                &value,
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
