#include "repl/repl.h"
#include "repl/repl_input_scan.h"
#include "repl/repl_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_cli/conf.h"
#include "zr_vm_core/log.h"

static void zr_cli_repl_prepare_stdio(void) {
    static TZrBool prepared = ZR_FALSE;

    if (prepared) {
        return;
    }

    (void)setvbuf(stdout, ZR_NULL, _IONBF, 0);
    (void)setvbuf(stderr, ZR_NULL, _IONBF, 0);
    prepared = ZR_TRUE;
}

static void zr_cli_repl_write_help(void) {
    ZrCore_Log_Helpf(ZR_NULL,
                     "Available commands:\n"
                     "  :help   Show this help text.\n"
                     "  :reset  Clear the pending input buffer.\n"
                     "  :type   Show inferred type and local semantic facts for an expression.\n"
                     "  :quit   Exit the REPL.\n");
}

static TZrChar *zr_cli_repl_build_return_wrapper(const TZrChar *code) {
    static const TZrChar prefix[] = "return ";
    static const TZrChar suffix[] = ";";
    const TZrChar *begin;
    const TZrChar *end;
    TZrSize expressionLength;
    TZrSize prefixLength;
    TZrSize suffixLength;
    TZrSize wrapperLength;
    TZrChar *wrapper;

    begin = ZrCli_ReplInput_SkipSpace(code);
    if (begin == ZR_NULL || *begin == '\0') {
        return ZR_NULL;
    }

    end = begin + strlen(begin);
    while (end > begin && ZrCli_ReplInput_IsSpace(end[-1])) {
        --end;
    }
    if (end > begin && end[-1] == ';') {
        --end;
        while (end > begin && ZrCli_ReplInput_IsSpace(end[-1])) {
            --end;
        }
    }
    if (end == begin) {
        return ZR_NULL;
    }

    expressionLength = (TZrSize)(end - begin);
    prefixLength = strlen(prefix);
    suffixLength = strlen(suffix);
    wrapperLength = prefixLength + expressionLength + suffixLength;
    wrapper = (TZrChar *)malloc(wrapperLength + 1u);
    if (wrapper == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(wrapper, prefix, prefixLength);
    memcpy(wrapper + prefixLength, begin, expressionLength);
    memcpy(wrapper + prefixLength + expressionLength, suffix, suffixLength);
    wrapper[wrapperLength] = '\0';
    return wrapper;
}

static int zr_cli_repl_submit_session(ZrCliReplSession *session, const TZrChar *code) {
    TZrChar *wrappedExpressionCode = ZR_NULL;
    const TZrChar *compileCode = code;
    int result;

    if (session == ZR_NULL || code == ZR_NULL || code[0] == '\0') {
        return 0;
    }

    if (ZrCli_ReplInput_ShouldWrapExpression(code)) {
        wrappedExpressionCode = zr_cli_repl_build_return_wrapper(code);
        if (wrappedExpressionCode == ZR_NULL) {
            return 1;
        }
        compileCode = wrappedExpressionCode;
    }

    result = ZrCli_ReplSession_Submit(session, compileCode);
    free(wrappedExpressionCode);
    return result;
}

int ZrCli_Repl_Run(void) {
    TZrChar line[ZR_CLI_REPL_LINE_BUFFER_LENGTH];
    TZrChar *buffer = ZR_NULL;
    TZrSize bufferLength = 0u;
    TZrSize bufferCapacity = 0u;
    ZrCliReplSession session;

    memset(&session, 0, sizeof(session));
    zr_cli_repl_prepare_stdio();
    if (ZrCli_ReplSession_Init(&session) != 0) {
        return 1;
    }

    ZrCore_Log_Helpf(ZR_NULL,
                     "ZR VM REPL\n"
                     "Enter code, then submit with an empty line. Type :help for commands.\n");
    ZrCore_Log_FlushDefaultSinks();

    for (;;) {
        TZrSize lineLength;

        ZrCore_Log_FlushDefaultSinks();
        if (fgets(line, sizeof(line), stdin) == ZR_NULL) {
            break;
        }

        lineLength = strlen(line);
        while (lineLength > 0u && (line[lineLength - 1u] == '\n' || line[lineLength - 1u] == '\r')) {
            line[--lineLength] = '\0';
        }

        if (bufferLength == 0u && line[0] == ':') {
            if (strcmp(line, ":help") == 0) {
                zr_cli_repl_write_help();
            } else if (strcmp(line, ":quit") == 0) {
                ZrCore_Log_FlushDefaultSinks();
                free(buffer);
                ZrCli_ReplSession_Free(&session);
                return 0;
            } else if (strcmp(line, ":reset") == 0) {
                bufferLength = 0u;
                if (buffer != ZR_NULL) {
                    buffer[0] = '\0';
                }
                (void)ZrCli_ReplSession_Reset(&session);
            } else if (ZrCli_ReplInput_StartsWithKeyword(line, ":type")) {
                (void)ZrCli_ReplSession_TypeQuery(&session, line + 5);
            } else {
                ZrCore_Log_Error(ZR_NULL, "unknown REPL command: %s\n", line);
            }
            ZrCore_Log_FlushDefaultSinks();
            continue;
        }

        if (lineLength == 0u) {
            if (bufferLength > 0u) {
                (void)zr_cli_repl_submit_session(&session, buffer);
                bufferLength = 0u;
                if (buffer != ZR_NULL) {
                    buffer[0] = '\0';
                }
                ZrCore_Log_FlushDefaultSinks();
            }
            continue;
        }

        if (bufferLength + lineLength + 2u > bufferCapacity) {
            TZrSize newCapacity = bufferCapacity == 0u ? ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY
                                                        : bufferCapacity * ZR_CLI_COLLECTION_GROWTH_FACTOR;
            TZrChar *newBuffer;

            while (newCapacity < bufferLength + lineLength + 2u) {
                newCapacity *= ZR_CLI_COLLECTION_GROWTH_FACTOR;
            }
            newBuffer = (TZrChar *)realloc(buffer, newCapacity);
            if (newBuffer == ZR_NULL) {
                free(buffer);
                ZrCli_ReplSession_Free(&session);
                ZrCore_Log_Error(ZR_NULL, "out of memory\n");
                return 1;
            }
            buffer = newBuffer;
            bufferCapacity = newCapacity;
        }

        memcpy(buffer + bufferLength, line, lineLength);
        bufferLength += lineLength;
        buffer[bufferLength++] = '\n';
        buffer[bufferLength] = '\0';
    }

    ZrCore_Log_FlushDefaultSinks();
    free(buffer);
    ZrCli_ReplSession_Free(&session);
    return 0;
}
