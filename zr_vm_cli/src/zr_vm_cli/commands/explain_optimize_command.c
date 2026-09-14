#include "commands/explain_optimize_command.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void zr_cli_explain_error(TZrChar *buffer,
                                 TZrSize bufferSize,
                                 const TZrChar *format,
                                 ...) {
    va_list args;
    if (buffer == ZR_NULL || bufferSize == 0u || format == ZR_NULL) return;
    va_start(args, format);
    (void)vsnprintf(buffer, bufferSize, format, args);
    va_end(args);
    buffer[bufferSize - 1u] = '\0';
}

static TZrBool zr_cli_explain_parse_u64(const TZrChar *text,
                                         TZrUInt64 *value) {
    TZrChar *end = ZR_NULL;
    unsigned long long parsed;
    if (text == ZR_NULL || text[0] == '\0' || value == ZR_NULL ||
        text[0] == '-') return ZR_FALSE;
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno == ERANGE || end == text || *end != '\0') return ZR_FALSE;
    *value = (TZrUInt64)parsed;
    return ZR_TRUE;
}

static TZrBool zr_cli_explain_parse_u32(const TZrChar *text,
                                         TZrUInt32 *value) {
    TZrUInt64 parsed;
    if (value == ZR_NULL || !zr_cli_explain_parse_u64(text, &parsed) ||
        parsed > UINT32_MAX) return ZR_FALSE;
    *value = (TZrUInt32)parsed;
    return ZR_TRUE;
}

static TZrBool zr_cli_explain_name_equal(const TZrChar *left,
                                         const TZrChar *right) {
    TZrSize index;
    if (left == ZR_NULL || right == ZR_NULL) return ZR_FALSE;
    for (index = 0u; left[index] != '\0' && right[index] != '\0'; index++) {
        TZrChar lc = left[index] == '_' ? '-' : left[index];
        TZrChar rc = right[index] == '_' ? '-' : right[index];
        if (lc != rc) return ZR_FALSE;
    }
    return (TZrBool)(left[index] == '\0' && right[index] == '\0');
}

static TZrBool zr_cli_explain_reason_bit(const TZrChar *name,
                                         TZrUInt32 *bit) {
    EZrOptimizationRemarkReason reason;
    if (name == ZR_NULL || bit == ZR_NULL) return ZR_FALSE;
    for (reason = ZR_OPTIMIZATION_REMARK_REASON_NONE;
         reason < ZR_OPTIMIZATION_REMARK_REASON_COUNT;
         reason = (EZrOptimizationRemarkReason)((TZrUInt32)reason + 1u)) {
        if (zr_cli_explain_name_equal(
                    name, ZrCore_OptimizationRemark_ReasonName(reason))) {
            *bit = ZR_OPTIMIZATION_REMARK_REASON_MASK(reason);
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_cli_explain_status_bit(const TZrChar *name,
                                         TZrUInt32 *bit) {
    EZrOptimizationRemarkStatus status;
    if (name == ZR_NULL || bit == ZR_NULL) return ZR_FALSE;
    for (status = ZR_OPTIMIZATION_REMARK_SUCCESS;
         status < ZR_OPTIMIZATION_REMARK_STATUS_COUNT;
         status = (EZrOptimizationRemarkStatus)((TZrUInt32)status + 1u)) {
        if (zr_cli_explain_name_equal(
                    name, ZrCore_OptimizationRemark_StatusName(status))) {
            *bit = ZR_OPTIMIZATION_REMARK_STATUS_MASK(status);
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_cli_explain_evidence_bit(const TZrChar *name,
                                           TZrUInt32 *bit) {
    EZrOptimizationRemarkEvidence evidence;
    if (name == ZR_NULL || bit == ZR_NULL) return ZR_FALSE;
    for (evidence = ZR_OPTIMIZATION_REMARK_EVIDENCE_PROVEN;
         evidence < ZR_OPTIMIZATION_REMARK_EVIDENCE_COUNT;
         evidence = (EZrOptimizationRemarkEvidence)((TZrUInt32)evidence + 1u)) {
        if (zr_cli_explain_name_equal(
                    name, ZrCore_OptimizationRemark_EvidenceName(evidence))) {
            *bit = ZR_OPTIMIZATION_REMARK_EVIDENCE_MASK(evidence);
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool zr_cli_explain_backend_bit(const TZrChar *name,
                                          TZrUInt32 *bit) {
    struct SZrCliBackendName {
        const TZrChar *name;
        TZrUInt32 bit;
    };
    static const struct SZrCliBackendName names[] = {
        {"exec-bc", ZR_OPTIMIZATION_REMARK_BACKEND_EXEC_BC},
        {"aot", ZR_OPTIMIZATION_REMARK_BACKEND_AOT},
        {"jit", ZR_OPTIMIZATION_REMARK_BACKEND_JIT},
        {"llvm", ZR_OPTIMIZATION_REMARK_BACKEND_LLVM},
        {"interpreter", ZR_OPTIMIZATION_REMARK_BACKEND_INTERPRETER}
    };
    TZrSize index;
    if (name == ZR_NULL || bit == ZR_NULL) return ZR_FALSE;
    for (index = 0u; index < sizeof(names) / sizeof(names[0]); index++) {
        if (zr_cli_explain_name_equal(name, names[index].name)) {
            *bit = names[index].bit;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

typedef enum EZrCliExplainMaskKind {
    ZR_CLI_EXPLAIN_MASK_REASON = 0,
    ZR_CLI_EXPLAIN_MASK_STATUS,
    ZR_CLI_EXPLAIN_MASK_BACKEND,
    ZR_CLI_EXPLAIN_MASK_EVIDENCE
} EZrCliExplainMaskKind;

static TZrBool zr_cli_explain_parse_mask(const TZrChar *text,
                                         EZrCliExplainMaskKind kind,
                                         TZrUInt32 *mask) {
    const TZrChar *cursor = text;
    TZrUInt32 result = 0u;
    if (text == ZR_NULL || text[0] == '\0' || mask == ZR_NULL) return ZR_FALSE;
    while (*cursor != '\0') {
        const TZrChar *end = strchr(cursor, ',');
        TZrSize length = end != ZR_NULL ? (TZrSize)(end - cursor) : strlen(cursor);
        TZrChar token[64];
        TZrUInt32 bit = 0u;
        if (length == 0u || length >= sizeof(token)) return ZR_FALSE;
        (void)memcpy(token, cursor, length);
        token[length] = '\0';
        if (kind == ZR_CLI_EXPLAIN_MASK_REASON) {
            if (!zr_cli_explain_reason_bit(token, &bit)) return ZR_FALSE;
        } else if (kind == ZR_CLI_EXPLAIN_MASK_STATUS) {
            if (!zr_cli_explain_status_bit(token, &bit)) return ZR_FALSE;
        } else if (kind == ZR_CLI_EXPLAIN_MASK_BACKEND) {
            if (!zr_cli_explain_backend_bit(token, &bit)) return ZR_FALSE;
        } else if (!zr_cli_explain_evidence_bit(token, &bit)) {
            return ZR_FALSE;
        }
        result |= bit;
        if (end != ZR_NULL) {
            /* A separator must be followed by another token.  Without this
             * check `--reason code_budget,` would silently broaden a query
             * while appearing to parse successfully. */
            if (end[1] == '\0') return ZR_FALSE;
            cursor = end + 1;
        } else {
            cursor = cursor + length;
        }
    }
    *mask = result;
    return ZR_TRUE;
}

static TZrBool zr_cli_explain_copy_bounded(TZrChar *destination,
                                           TZrSize capacity,
                                           const TZrChar *source) {
    TZrSize length;
    if (destination == ZR_NULL || capacity == 0u || source == ZR_NULL) return ZR_FALSE;
    length = strlen(source);
    /* An empty pass/module selector is not a useful query and would be
     * indistinguishable from an omitted selector at the core boundary. */
    if (length == 0u || length >= capacity) return ZR_FALSE;
    (void)memcpy(destination, source, length + 1u);
    return ZR_TRUE;
}

static TZrBool zr_cli_explain_parse_range(const TZrChar *text,
                                          SZrOptimizationRemarkSourceRange *range) {
    const TZrChar *separator;
    TZrChar startText[32];
    TZrChar endText[32];
    TZrSize startLength;
    TZrUInt32 start;
    TZrUInt32 end;
    if (text == ZR_NULL || range == ZR_NULL) return ZR_FALSE;
    separator = strchr(text, ':');
    if (separator == ZR_NULL) separator = strstr(text, "..");
    if (separator == ZR_NULL) return ZR_FALSE;
    startLength = (TZrSize)(separator - text);
    if (startLength == 0u || startLength >= sizeof(startText)) return ZR_FALSE;
    (void)memcpy(startText, text, startLength);
    startText[startLength] = '\0';
    if (separator[0] == ':' ) {
        if (separator[1] == '\0' || strlen(separator + 1u) >= sizeof(endText)) return ZR_FALSE;
        (void)memcpy(endText, separator + 1u, strlen(separator + 1u) + 1u);
    } else {
        if (separator[2] == '\0' || strlen(separator + 2u) >= sizeof(endText)) return ZR_FALSE;
        (void)memcpy(endText, separator + 2u, strlen(separator + 2u) + 1u);
    }
    if (!zr_cli_explain_parse_u32(startText, &start) ||
        !zr_cli_explain_parse_u32(endText, &end) || end < start) return ZR_FALSE;
    range->startOffset = start;
    range->endOffset = end;
    return ZR_TRUE;
}

void ZrCli_ExplainOptimizeOptions_Init(
        SZrCliExplainOptimizeOptions *options) {
    if (options == ZR_NULL) return;
    memset(options, 0, sizeof(*options));
    options->pageLimit = 64u;
}

TZrBool ZrCli_ExplainOptimizeOptions_Parse(
        int argc,
        const TZrChar *const *argv,
        SZrCliExplainOptimizeOptions *options,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    int index;

    if (errorBuffer != ZR_NULL && errorBufferSize != 0u) errorBuffer[0] = '\0';
    if (options == ZR_NULL || argc < 0 || (argc > 0 && argv == ZR_NULL)) {
        zr_cli_explain_error(errorBuffer, errorBufferSize,
                             "optimization remark arguments are invalid");
        return ZR_FALSE;
    }
    ZrCli_ExplainOptimizeOptions_Init(options);
    for (index = 0; index < argc; index++) {
        const TZrChar *argument = argv[index];
        const TZrChar *value = ZR_NULL;
        const TZrChar *equals;
        TZrChar optionName[32];
        TZrSize optionLength;
        if (argument == ZR_NULL || argument[0] == '\0') {
            zr_cli_explain_error(errorBuffer, errorBufferSize,
                                 "empty optimization remark argument");
            return ZR_FALSE;
        }
        if (strcmp(argument, "--json") == 0) {
            if (options->json) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "duplicate option: --json");
                return ZR_FALSE;
            }
            options->json = ZR_TRUE;
            continue;
        }
        if (argument[0] != '-' || argument[1] != '-') {
            if (options->hasModule ||
                !zr_cli_explain_copy_bounded(options->module, sizeof(options->module), argument)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "expected one module name after explain optimize");
                return ZR_FALSE;
            }
            options->hasModule = ZR_TRUE;
            continue;
        }
        equals = strchr(argument, '=');
        if (equals != ZR_NULL) {
            optionLength = (TZrSize)(equals - argument);
            value = equals + 1u;
        } else {
            optionLength = strlen(argument);
        }
        if (optionLength <= 2u || optionLength >= sizeof(optionName)) {
            zr_cli_explain_error(errorBuffer, errorBufferSize,
                                 "invalid optimization remark option: %s", argument);
            return ZR_FALSE;
        }
        (void)memcpy(optionName, argument, optionLength);
        optionName[optionLength] = '\0';

#define ZR_CLI_EXPLAIN_NEEDS_VALUE() \
        do { \
            if (value == ZR_NULL) { \
                if (index + 1 >= argc || argv[index + 1] == ZR_NULL || \
                    argv[index + 1][0] == '\0' || argv[index + 1][0] == '-') { \
                    zr_cli_explain_error(errorBuffer, errorBufferSize, \
                                         "missing value after %s", optionName); \
                    return ZR_FALSE; \
                } \
                value = argv[++index]; \
            } \
        } while (0)

        if (strcmp(optionName, "--module") == 0) {
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasModule ||
                !zr_cli_explain_copy_bounded(options->module, sizeof(options->module), value)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --module");
                return ZR_FALSE;
            }
            options->hasModule = ZR_TRUE;
        } else if (strcmp(optionName, "--module-hash") == 0) {
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasModuleHash || !zr_cli_explain_parse_u64(value, &options->moduleHash)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --module-hash");
                return ZR_FALSE;
            }
            options->hasModuleHash = ZR_TRUE;
        } else if (strcmp(optionName, "--ir-hash") == 0) {
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasIrHash || !zr_cli_explain_parse_u64(value, &options->irHash)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --ir-hash");
                return ZR_FALSE;
            }
            options->hasIrHash = ZR_TRUE;
        } else if (strcmp(optionName, "--source-version") == 0) {
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasSourceVersion || !zr_cli_explain_parse_u64(value, &options->sourceVersion)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --source-version");
                return ZR_FALSE;
            }
            options->hasSourceVersion = ZR_TRUE;
        } else if (strcmp(optionName, "--source-id") == 0) {
            TZrUInt32 parsed;
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasSourceId || !zr_cli_explain_parse_u32(value, &parsed)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --source-id");
                return ZR_FALSE;
            }
            options->sourceId = parsed;
            options->hasSourceId = ZR_TRUE;
        } else if (strcmp(optionName, "--reason") == 0 ||
                   strcmp(optionName, "--status") == 0 ||
                   strcmp(optionName, "--backend") == 0 ||
                   strcmp(optionName, "--evidence") == 0) {
            TZrUInt32 parsed;
            EZrCliExplainMaskKind kind;
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (strcmp(optionName, "--reason") == 0) kind = ZR_CLI_EXPLAIN_MASK_REASON;
            else if (strcmp(optionName, "--status") == 0) kind = ZR_CLI_EXPLAIN_MASK_STATUS;
            else if (strcmp(optionName, "--backend") == 0) kind = ZR_CLI_EXPLAIN_MASK_BACKEND;
            else kind = ZR_CLI_EXPLAIN_MASK_EVIDENCE;
            if (!zr_cli_explain_parse_mask(value, kind, &parsed)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid %s filter: %s", optionName + 2u, value);
                return ZR_FALSE;
            }
            if (kind == ZR_CLI_EXPLAIN_MASK_REASON) options->reasonMask |= parsed;
            else if (kind == ZR_CLI_EXPLAIN_MASK_STATUS) options->statusMask |= parsed;
            else if (kind == ZR_CLI_EXPLAIN_MASK_BACKEND) options->backendMask |= parsed;
            else options->evidenceMask |= parsed;
        } else if (strcmp(optionName, "--pass") == 0) {
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasPass ||
                !zr_cli_explain_copy_bounded(options->pass, sizeof(options->pass), value)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --pass");
                return ZR_FALSE;
            }
            options->hasPass = ZR_TRUE;
        } else if (strcmp(optionName, "--range") == 0) {
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (options->hasSourceRange ||
                !zr_cli_explain_parse_range(value, &options->sourceRange)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid or duplicate --range (expected start:end)");
                return ZR_FALSE;
            }
            options->hasSourceRange = ZR_TRUE;
        } else if (strcmp(optionName, "--offset") == 0 ||
                   strcmp(optionName, "--page-offset") == 0) {
            TZrUInt32 parsed;
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (!zr_cli_explain_parse_u32(value, &parsed)) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid page offset: %s", value);
                return ZR_FALSE;
            }
            options->pageOffset = parsed;
        } else if (strcmp(optionName, "--limit") == 0 ||
                   strcmp(optionName, "--page-limit") == 0) {
            TZrUInt32 parsed;
            ZR_CLI_EXPLAIN_NEEDS_VALUE();
            if (!zr_cli_explain_parse_u32(value, &parsed) || parsed == 0u) {
                zr_cli_explain_error(errorBuffer, errorBufferSize,
                                     "invalid page limit: %s", value);
                return ZR_FALSE;
            }
            options->pageLimit = parsed;
        } else {
            zr_cli_explain_error(errorBuffer, errorBufferSize,
                                 "unknown optimization remark option: %s", optionName);
            return ZR_FALSE;
        }
#undef ZR_CLI_EXPLAIN_NEEDS_VALUE
    }
    return ZR_TRUE;
}

static void zr_cli_explain_copy_query(
        const SZrCliExplainOptimizeOptions *options,
        SZrOptimizationRemarkQuery *query) {
    ZrCore_OptimizationRemarkQuery_Init(query);
    query->moduleHash = options->moduleHash;
    query->irHash = options->irHash;
    query->sourceVersion = options->sourceVersion;
    query->sourceId = options->sourceId;
    query->reasonMask = options->reasonMask;
    query->statusMask = options->statusMask;
    query->backendMask = options->backendMask;
    query->evidenceMask = options->evidenceMask;
    query->pageOffset = options->pageOffset;
    query->pageLimit = options->pageLimit;
    query->hasModuleHash = options->hasModuleHash;
    query->hasIrHash = options->hasIrHash;
    query->hasSourceVersion = options->hasSourceVersion;
    query->hasSourceId = options->hasSourceId;
    query->hasSourceRange = options->hasSourceRange;
    query->hasPass = options->hasPass;
    query->hasModule = options->hasModule;
    query->sourceRange = options->sourceRange;
    if (options->hasPass) {
        memcpy(query->pass, options->pass, sizeof(query->pass) - 1u);
        query->pass[sizeof(query->pass) - 1u] = '\0';
    }
    if (options->hasModule) {
        memcpy(query->module, options->module, sizeof(query->module));
        query->module[sizeof(query->module) - 1u] = '\0';
    }
}

static int zr_cli_explain_write_json(const SZrOptimizationRemarkPage *page,
                                     FILE *output,
                                     FILE *errorOutput) {
    TZrChar *buffer = ZR_NULL;
    TZrSize required = 0u;
    SZrOptimizationRemarkDiagnostic diagnostic;
    TZrBool ok;

    ok = ZrCore_OptimizationRemarks_PageWriteJson(
            page, ZR_NULL, 0u, &required, &diagnostic);
    if (!ok && diagnostic.code != ZR_OPTIMIZATION_REMARK_DIAGNOSTIC_BUFFER_TOO_SMALL) {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "optimization remarks JSON failed: %s\n",
                    ZrCore_OptimizationRemark_DiagnosticName(diagnostic.code));
        }
        return 3;
    }
    if (required == (TZrSize)-1 ||
        (buffer = (TZrChar *)malloc(required + 1u)) == ZR_NULL ||
        !ZrCore_OptimizationRemarks_PageWriteJson(
                page, buffer, required + 1u, &required, &diagnostic)) {
        free(buffer);
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "optimization remarks JSON allocation failed\n");
        }
        return 3;
    }
    if (output != ZR_NULL) {
        fputs(buffer, output);
        fputc('\n', output);
    }
    free(buffer);
    return 0;
}

static int zr_cli_explain_write_text(const SZrOptimizationRemarkPage *page,
                                     FILE *output,
                                     FILE *errorOutput) {
    TZrUInt32 index;
    TZrChar line[512];
    TZrSize written;
    SZrOptimizationRemarkDiagnostic diagnostic;

    if (output == ZR_NULL) return 2;
    fprintf(output, "optimization remarks: %u/%u\n",
            (unsigned)page->count, (unsigned)page->totalMatches);
    for (index = 0u; index < page->count; index++) {
        if (!ZrCore_OptimizationRemark_WriteText(
                    &page->items[index], line, sizeof(line), &written, &diagnostic)) {
            if (errorOutput != ZR_NULL) {
                fprintf(errorOutput, "optimization remark %u is invalid: %s\n",
                        (unsigned)index,
                        ZrCore_OptimizationRemark_DiagnosticName(diagnostic.code));
            }
            return 3;
        }
        fputs(line, output);
        fputc('\n', output);
    }
    if (page->truncated) {
        fputs("optimization remarks: truncated (some records were dropped)\n", output);
    }
    return 0;
}

int ZrCli_ExplainOptimize_RunStore(
        const SZrOptimizationRemarkStore *store,
        const SZrCliExplainOptimizeOptions *options,
        FILE *output,
        FILE *errorOutput) {
    SZrOptimizationRemarkQuery query;
    SZrOptimizationRemarkPage page;
    SZrOptimizationRemarkDiagnostic diagnostic;
    int result;

    if (store == ZR_NULL || options == ZR_NULL || output == ZR_NULL) {
        if (errorOutput != ZR_NULL) fputs("invalid optimization remark query\n", errorOutput);
        return 2;
    }
    zr_cli_explain_copy_query(options, &query);
    if (!ZrCore_OptimizationRemarks_Query(store, &query, &page, &diagnostic)) {
        if (errorOutput != ZR_NULL) {
            fprintf(errorOutput, "optimization remark query failed: %s\n",
                    ZrCore_OptimizationRemark_DiagnosticName(diagnostic.code));
        }
        return 2;
    }
    result = options->json
             ? zr_cli_explain_write_json(&page, output, errorOutput)
             : zr_cli_explain_write_text(&page, output, errorOutput);
    ZrCore_OptimizationRemarks_PageFree(&page);
    return result;
}
