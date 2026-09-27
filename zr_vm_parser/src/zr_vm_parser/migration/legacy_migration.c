#include "zr_vm_parser/legacy_migration.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

#include "legacy_migration_module_specifier.h"

/* 迁移扫描只在代码区生成候选；注释和字符串状态避免把示例文本当成旧语法。 */
typedef enum EZrLegacyMigrationLexState {
    ZR_LEGACY_MIGRATION_LEX_CODE = 0,
    ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT,
    ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT,
    ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING,
    ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING
} EZrLegacyMigrationLexState;

/* 规则表把旧指令的归属计划与自动化等级放在一起，供报告和编辑共用。 */
typedef struct SZrLegacyMigrationDirectiveRule {
    const TZrChar *directive;
    const TZrChar *oldConstructKind;
    const TZrChar *targetConstructKind;
    const TZrChar *targetPlanId;
    EZrLegacyMigrationApplicability applicability;
    const TZrChar *reason;
} SZrLegacyMigrationDirectiveRule;

static const SZrLegacyMigrationDirectiveRule k_legacy_migration_directive_rules[] = {
        {"module", "percentModule", "moduleDeclaration", "06A",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The module declaration uses the promoted module keyword."},
        {"import", "percentImport", "importBinding", "06A",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Legacy imports require module-scope binding and alias proof."},
        {"async", "percentAsync", "asyncDeclaration", "12",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The async declaration uses the promoted async keyword."},
        {"await", "percentAwait", "awaitExpression", "12",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The await expression uses the promoted await keyword."},
        {"extern", "percentExtern", "nativeDeclaration", "10",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "Native declarations use the promoted native extern spelling."},
        {"test", "percentTest", "testDeclaration", "14",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "A structurally safe legacy test becomes an ordinary function with zr.testing.test metadata."},
        {"compileTime", "percentCompileTime", "compileTimeDeclaration", "11",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "Compile-time declarations use the promoted comptime keyword."},
        {"func", "percentFunc", "functionType", "06A",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "Callable types use the promoted fn keyword."},
        {"owned", "percentOwned", "resourceModifier", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The ownership shell maps to the promoted resource modifier."},
        {"release", "percentRelease", "dropIntrinsic", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The removed release operation maps directly to drop(owner)."},
        {"upgrade", "percentUpgrade", "wakeIntrinsic", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The removed weak upgrade operation maps directly to wake(weak)."},
        {"weak", "percentWeak", "degradeIntrinsic", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The removed weak projection maps directly to degrade(shared)."},
        {"shared", "percentShared", "shareIntrinsic", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The removed shared projection maps directly to share(owner)."},
        {"detach", "percentDetach", "intoGcIntrinsic", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The removed detach bridge maps directly to intoGc(owner)."},
        {"unique", "percentUnique", "uniqueConstruction", "04",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Unique construction requires resource-type proof before it can be rewritten."},
        {"in", "percentIn", "inParameter", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Passing mode migration requires a resolved parameter declaration."},
        {"ref", "percentRef", "refParameter", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Passing mode migration requires a resolved parameter declaration."},
        {"out", "percentOut", "outParameter", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Passing mode migration requires a resolved parameter declaration."},
        {"borrow", "percentBorrow", "borrowExpression", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Borrow migration requires canonical lifetime proof."},
        {"loan", "percentLoan", "loanExpression", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Loan migration requires canonical lifetime proof."},
        {"borrowed", "percentBorrowed", "borrowedType", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Borrowed type migration requires canonical lifetime proof."},
        {"loaned", "percentLoaned", "loanedType", "02",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Loaned type migration requires canonical lifetime proof."},
        {"type", "percentType", "typeReflection", "08",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "Runtime type queries use the canonical typeof expression."},
        {"using", "percentUsing", "usingResource", "06A",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Using migration requires resource-role proof."},
};

static TZrBool legacy_migration_is_identifier_start(TZrChar value) {
    return isalpha((unsigned char)value) || value == '_';
}

static TZrBool legacy_migration_is_identifier_continue(TZrChar value) {
    return isalnum((unsigned char)value) || value == '_';
}

static TZrSize legacy_migration_read_identifier(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize start) {
    TZrSize end = start;

    while (end < sourceLength && legacy_migration_is_identifier_continue(source[end])) {
        end++;
    }
    return end;
}

static TZrSize legacy_migration_skip_space(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize start) {
    while (start < sourceLength &&
           (source[start] == ' ' || source[start] == '\t' || source[start] == '\r')) {
        start++;
    }
    return start;
}

static TZrSize legacy_migration_skip_whitespace(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize start) {
    while (start < sourceLength && isspace((unsigned char)source[start])) {
        start++;
    }
    return start;
}

static TZrSize legacy_migration_trim_end(
        const TZrChar *source,
        TZrSize start,
        TZrSize end) {
    while (end > start &&
           (source[end - 1U] == ' ' || source[end - 1U] == '\t' || source[end - 1U] == '\r')) {
        end--;
    }
    return end;
}

static TZrSize legacy_migration_line_end(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize start) {
    while (start < sourceLength && source[start] != '\n') {
        start++;
    }
    return start;
}

static TZrBool legacy_migration_span_equals(
        const TZrChar *source,
        TZrSize start,
        TZrSize end,
        const TZrChar *expected) {
    TZrSize expectedLength = strlen(expected);

    return end >= start && end - start == expectedLength &&
           memcmp(source + start, expected, expectedLength) == 0;
}

/* 仅凭 '%' 字符不足以识别旧指令：表达式尾部的 '%' 仍可能是取模运算。 */
static TZrBool legacy_migration_percent_starts_directive(
        const TZrChar *source,
        TZrSize percentOffset) {
    TZrSize cursor = percentOffset;
    TZrSize wordStart;
    TZrChar previous;

    if (source == ZR_NULL || percentOffset == 0U) {
        return ZR_TRUE;
    }
    while (cursor > 0U && isspace((unsigned char)source[cursor - 1U])) {
        if (source[cursor - 1U] == '\n') {
            return ZR_TRUE;
        }
        cursor--;
    }
    if (cursor == 0U) {
        return ZR_TRUE;
    }

    previous = source[cursor - 1U];
    if (legacy_migration_is_identifier_continue(previous)) {
        wordStart = cursor - 1U;
        while (wordStart > 0U &&
               legacy_migration_is_identifier_continue(source[wordStart - 1U])) {
            wordStart--;
        }
        return legacy_migration_span_equals(source, wordStart, cursor, "return") ||
               legacy_migration_span_equals(source, wordStart, cursor, "throw") ||
               legacy_migration_span_equals(source, wordStart, cursor, "yield") ||
               legacy_migration_span_equals(source, wordStart, cursor, "await") ||
               legacy_migration_span_equals(source, wordStart, cursor, "case") ||
               legacy_migration_span_equals(source, wordStart, cursor, "out");
    }
    if (isdigit((unsigned char)previous) || previous == ')' || previous == ']' ||
        previous == '}' || previous == '\'' || previous == '"' || previous == '`') {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static const SZrLegacyMigrationDirectiveRule *legacy_migration_find_directive_rule(
        const TZrChar *source,
        TZrSize start,
        TZrSize end) {
    TZrSize index;

    for (index = 0U;
         index < sizeof(k_legacy_migration_directive_rules) /
                         sizeof(k_legacy_migration_directive_rules[0]);
         index++) {
        if (legacy_migration_span_equals(
                    source,
                    start,
                    end,
                    k_legacy_migration_directive_rules[index].directive)) {
            return &k_legacy_migration_directive_rules[index];
        }
    }
    return ZR_NULL;
}

/* 诊断和修复都以原始字节偏移为准，再映射到编辑器可展示的位置。 */
static SZrFilePosition legacy_migration_position_from_offset(
        const TZrChar *source,
        TZrSize offset) {
    TZrInt32 line = 1;
    TZrInt32 column = 0;
    TZrSize index;

    for (index = 0U; index < offset; index++) {
        if (source[index] == '\n') {
            line++;
            column = 0;
        } else {
            column++;
        }
    }
    return ZrParser_FilePosition_Create(offset, line, column);
}

static SZrFileRange legacy_migration_range(
        const TZrChar *source,
        SZrString *sourceName,
        TZrSize start,
        TZrSize end) {
    return ZrParser_FileRange_Create(
            legacy_migration_position_from_offset(source, start),
            legacy_migration_position_from_offset(source, end),
            sourceName);
}

/* 计划绑定创建时的源码；应用前重新计算，阻止对不同版本套用旧偏移。 */
static TZrUInt64 legacy_migration_hash(const TZrChar *source, TZrSize sourceLength) {
    TZrUInt64 result = 1469598103934665603ULL;
    TZrSize index;

    for (index = 0U; index < sourceLength; index++) {
        result ^= (TZrUInt8)source[index];
        result *= 1099511628211ULL;
    }
    return result != 0U ? result : 1U;
}

static TZrChar *legacy_migration_format_unary_call(
        SZrState *state,
        const TZrChar *source,
        TZrSize argumentStart,
        TZrSize argumentEnd,
        const TZrChar *prefix,
        const TZrChar *suffix) {
    TZrSize argumentLength = argumentEnd - argumentStart;
    TZrSize prefixLength = strlen(prefix);
    TZrSize suffixLength = strlen(suffix);
    TZrChar *result = (TZrChar *)ZrCore_Memory_RawMalloc(
            state->global,
            prefixLength + argumentLength + suffixLength + 1U);

    if (result == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(result, prefix, prefixLength);
    memcpy(result + prefixLength, source + argumentStart, argumentLength);
    memcpy(result + prefixLength + argumentLength, suffix, suffixLength);
    result[prefixLength + argumentLength + suffixLength] = '\0';
    return result;
}

/* 动态构造只能提出待审核建议，调用方不得将返回文本标成机器安全修复。 */
static TZrChar *legacy_migration_format_dynamic_construct(
        SZrState *state,
        const TZrChar *source,
        TZrSize targetStart,
        TZrSize targetEnd,
        TZrSize argumentStart,
        TZrSize argumentEnd) {
    static const TZrChar prefix[] = "reflection.requireConstructible(";
    static const TZrChar middle[] = ").createInstance(...[";
    static const TZrChar suffix[] = "])";
    TZrSize prefixLength = sizeof(prefix) - 1U;
    TZrSize middleLength = sizeof(middle) - 1U;
    TZrSize suffixLength = sizeof(suffix) - 1U;
    TZrSize targetLength = targetEnd - targetStart;
    TZrSize argumentLength = argumentEnd - argumentStart;
    TZrSize resultLength = prefixLength + targetLength + middleLength +
                           argumentLength + suffixLength;
    TZrChar *result;
    TZrSize cursor = 0U;

    if (state == ZR_NULL || source == ZR_NULL || targetStart > targetEnd ||
        argumentStart > argumentEnd) {
        return ZR_NULL;
    }
    result = (TZrChar *)ZrCore_Memory_RawMalloc(
            state->global, resultLength + 1U);
    if (result == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(result + cursor, prefix, prefixLength);
    cursor += prefixLength;
    memcpy(result + cursor, source + targetStart, targetLength);
    cursor += targetLength;
    memcpy(result + cursor, middle, middleLength);
    cursor += middleLength;
    memcpy(result + cursor, source + argumentStart, argumentLength);
    cursor += argumentLength;
    memcpy(result + cursor, suffix, suffixLength);
    cursor += suffixLength;
    result[cursor] = '\0';
    return result;
}

/* %test 与旧式调用共用结构边界；只有确定边界后才可形成替换区间。
 * BUG: 这里未把单引号字符字面量纳入词法状态。`%test("x") { let c = '}'; return 0; }`
 * 会在字符中的 `}` 提前结束块扫描，遗漏后面的 return，并把本应人工审核的测试标为机器可用。
 * tests/parser/test_char_and_type_cast.c 的字符字面量测试与下方 append_percent_test 的 return 检查可核对该路径。 */
static TZrBool legacy_migration_find_balanced_end(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize openOffset,
        TZrChar openCharacter,
        TZrChar closeCharacter,
        TZrSize *outEnd) {
    EZrLegacyMigrationLexState lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
    TZrSize index;
    TZrSize depth = 0U;

    if (openOffset >= sourceLength || source[openOffset] != openCharacter ||
        outEnd == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = openOffset; index < sourceLength; index++) {
        TZrChar current = source[index];

        if (lexState == ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT) {
            if (current == '\n') {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
            }
            continue;
        }
        if (lexState == ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT) {
            if (current == '*' && index + 1U < sourceLength &&
                source[index + 1U] == '/') {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
                index++;
            }
            continue;
        }
        if (lexState == ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING ||
            lexState == ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING) {
            TZrChar terminator =
                    lexState == ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING ? '"' : '`';
            if (current == '\\' && index + 1U < sourceLength) {
                index++;
            } else if (current == terminator) {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
            }
            continue;
        }
        if (current == '/' && index + 1U < sourceLength && source[index + 1U] == '/') {
            lexState = ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT;
            index++;
        } else if (current == '/' && index + 1U < sourceLength && source[index + 1U] == '*') {
            lexState = ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT;
            index++;
        } else if (current == '"') {
            lexState = ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING;
        } else if (current == '`') {
            lexState = ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING;
        } else if (current == openCharacter) {
            depth++;
        } else if (current == closeCharacter) {
            if (depth == 0U) {
                return ZR_FALSE;
            }
            depth--;
            if (depth == 0U) {
                *outEnd = index + 1U;
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool legacy_migration_find_call_end(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize openOffset,
        TZrSize *outEnd) {
    return legacy_migration_find_balanced_end(
            source, sourceLength, openOffset, '(', ')', outEnd);
}

/* 所有候选经此入口取得统一的来源、适用性和可选 fix，供 CLI 报告与应用器共享。 */
static TZrBool legacy_migration_append_item(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        SZrString *sourceName,
        const TZrChar *oldConstructKind,
        const TZrChar *targetConstructKind,
        const TZrChar *targetPlanId,
        EZrLegacyMigrationApplicability applicability,
        const TZrChar *reason,
        TZrSize itemStart,
        TZrSize itemEnd,
        TZrSize editStart,
        TZrSize editEnd,
        const TZrChar *editText) {
    SZrLegacyMigrationItem item;

    if (state == ZR_NULL || plan == ZR_NULL || source == ZR_NULL ||
        oldConstructKind == ZR_NULL || targetConstructKind == ZR_NULL ||
        targetPlanId == ZR_NULL || reason == ZR_NULL || itemStart > itemEnd ||
        editStart > editEnd) {
        return ZR_FALSE;
    }
    memset(&item, 0, sizeof(item));
    item.diagnosticCode = ZR_STRING_LITERAL(state, "legacy_syntax_migration");
    item.oldConstructKind = ZrCore_String_Create(state, (TZrNativeString)oldConstructKind, strlen(oldConstructKind));
    item.targetConstructKind = ZrCore_String_Create(
            state,
            (TZrNativeString)targetConstructKind,
            strlen(targetConstructKind));
    item.oldTargetBindingKind = ZR_STRING_LITERAL(state, "legacySyntax");
    item.targetPlanId = ZrCore_String_Create(state, (TZrNativeString)targetPlanId, strlen(targetPlanId));
    item.reason = ZrCore_String_Create(state, (TZrNativeString)reason, strlen(reason));
    item.range = legacy_migration_range(source, sourceName, itemStart, itemEnd);
    item.applicability = applicability;
    item.fix.applicability = ZR_DIAGNOSTIC_FIX_APPLICABILITY_UNKNOWN;
    if (editText != ZR_NULL) {
        item.fix.title = ZR_STRING_LITERAL(state, "Migrate legacy syntax");
        item.fix.editRange = legacy_migration_range(source, sourceName, editStart, editEnd);
        item.fix.editText = ZrCore_String_Create(state, (TZrNativeString)editText, strlen(editText));
        item.fix.applicability =
                applicability == ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE
                ? ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE
                : ZR_DIAGNOSTIC_FIX_MAYBE_INCORRECT;
        item.hasFix = item.fix.editText != ZR_NULL;
    }
    if (item.diagnosticCode == ZR_NULL || item.oldConstructKind == ZR_NULL ||
        item.targetConstructKind == ZR_NULL || item.oldTargetBindingKind == ZR_NULL ||
        item.targetPlanId == ZR_NULL || item.reason == ZR_NULL ||
        (editText != ZR_NULL && !item.hasFix)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(state, &plan->items, &item);
    return ZR_TRUE;
}

/* 解析器诊断回调只在本次同步解析期间借用这些上下文；计划持有复制后的候选。 */
typedef struct SZrLegacyMigrationPropertyCapture {
    SZrState *state;
    SZrLegacyMigrationPlan *plan;
    const TZrChar *source;
    SZrString *sourceName;
} SZrLegacyMigrationPropertyCapture;

/* 配对 property 的替换文本由解析器掌握，本层只投影其结构化诊断和适用性。 */
static void legacy_migration_capture_property_diagnostic(
        TZrPtr userData,
        const SZrStructuredDiagnostic *diagnostic,
        EZrToken token) {
    SZrLegacyMigrationPropertyCapture *capture =
            (SZrLegacyMigrationPropertyCapture *)userData;
    const TZrChar *code;
    const SZrStructuredDiagnosticFix *fix = ZR_NULL;
    const TZrChar *editText = ZR_NULL;

    ZR_UNUSED_PARAMETER(token);
    if (capture == ZR_NULL || diagnostic == ZR_NULL || diagnostic->code == ZR_NULL) {
        return;
    }
    code = ZrCore_String_GetNativeString(diagnostic->code);
    if (code == ZR_NULL || strcmp(code, "legacy_property_syntax") != 0) {
        return;
    }
    if (diagnostic->fixes.length == 1U) {
        fix = (const SZrStructuredDiagnosticFix *)ZrCore_Array_Get(
                (SZrArray *)&diagnostic->fixes,
                0U);
        if (fix != ZR_NULL && fix->applicability == ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE &&
            fix->editText != ZR_NULL) {
            editText = ZrCore_String_GetNativeString(fix->editText);
        }
    }
    /* TODO: append_item 失败目前被回调吞掉；核查分配失败时是否应让 PlanSource
     * 返回失败，而不是输出缺少 parser-owned property 候选的成功计划。 */
    (void)legacy_migration_append_item(
            capture->state,
            capture->plan,
            capture->source,
            capture->sourceName,
            "legacyPropertyAccessor",
            "propertyDeclaration",
            "05",
            editText != ZR_NULL
                    ? ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE
                    : ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
            editText != ZR_NULL
                    ? "The paired accessor producer supplied an exact property replacement."
                    : "The paired accessor producer did not publish a machine-applicable replacement.",
            diagnostic->location.start.offset,
            diagnostic->location.end.offset,
            fix != ZR_NULL ? fix->editRange.start.offset : diagnostic->location.start.offset,
            fix != ZR_NULL ? fix->editRange.end.offset : diagnostic->location.end.offset,
            editText);
}

/* 先运行允许旧 property 的解析路径，以便后续词法扫描不自行猜测配对修复。 */
static void legacy_migration_capture_property_facts(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName) {
    SZrLegacyMigrationPropertyCapture capture;
    SZrParserState parserState;
    SZrAstNode *ast;

    memset(&capture, 0, sizeof(capture));
    memset(&parserState, 0, sizeof(parserState));
    capture.state = state;
    capture.plan = plan;
    capture.source = source;
    capture.sourceName = sourceName;
    ZrParser_State_Init(&parserState, state, source, sourceLength, sourceName);
    parserState.structuredErrorCallback = legacy_migration_capture_property_diagnostic;
    parserState.errorUserData = &capture;
    parserState.suppressErrorOutput = ZR_TRUE;
    parserState.enableLegacyMigrationParsing = ZR_TRUE;
    ast = ZrParser_ParseWithState(&parserState);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrParser_State_Free(&parserState);
}

/* 词法 fallback 只在解析器没有产生 property 候选时生效。
 * TODO: 当前按整份计划判断，尚需用含多组 property 且仅部分组可解析的样例确认
 * 是否会压掉其余组的 review-only 提示；核查 parser_property_migration_collection_publish_and_free。 */
static TZrBool legacy_migration_has_item_kind(
        const SZrLegacyMigrationPlan *plan,
        const TZrChar *kind) {
    TZrSize index;

    if (plan == ZR_NULL || kind == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);
        const TZrChar *itemKind = item != ZR_NULL && item->oldConstructKind != ZR_NULL
                                        ? ZrCore_String_GetNativeString(item->oldConstructKind)
                                        : ZR_NULL;
        if (itemKind != ZR_NULL && strcmp(itemKind, kind) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 旧 %test 的返回约定必须人工判断；此检查只在非字符串区寻找独立标识符。
 * BUG: 单引号字符中的 `"` 被当作双引号字符串起点。`%test("x") { let a = '"';
 * return 0; let b = '"'; }` 的 return 因而被跳过，候选误标机器可用。 */
static TZrBool legacy_migration_range_has_identifier(
        const TZrChar *source,
        TZrSize start,
        TZrSize end,
        const TZrChar *identifier) {
    EZrLegacyMigrationLexState lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
    TZrSize index = start;

    while (index < end) {
        TZrChar current = source[index];

        if (lexState == ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT) {
            if (current == '\n') {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
            }
            index++;
            continue;
        }
        if (lexState == ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT) {
            if (current == '*' && index + 1U < end && source[index + 1U] == '/') {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
                index += 2U;
            } else {
                index++;
            }
            continue;
        }
        if (lexState == ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING ||
            lexState == ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING) {
            TZrChar terminator =
                    lexState == ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING ? '"' : '`';
            if (current == '\\' && index + 1U < end) {
                index += 2U;
            } else if (current == terminator) {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
                index++;
            } else {
                index++;
            }
            continue;
        }
        if (current == '/' && index + 1U < end && source[index + 1U] == '/') {
            lexState = ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT;
            index += 2U;
            continue;
        }
        if (current == '/' && index + 1U < end && source[index + 1U] == '*') {
            lexState = ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT;
            index += 2U;
            continue;
        }
        if (current == '"') {
            lexState = ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING;
            index++;
            continue;
        }
        if (current == '`') {
            lexState = ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING;
            index++;
            continue;
        }
        if (legacy_migration_is_identifier_start(current)) {
            TZrSize wordEnd = legacy_migration_read_identifier(source, end, index);
            if (legacy_migration_span_equals(source, index, wordEnd, identifier)) {
                return ZR_TRUE;
            }
            index = wordEnd;
            continue;
        }
        index++;
    }
    return ZR_FALSE;
}

/* BUG: 这里未跳过注释。`%test("x") { // %release(owner)\n}` 中注释的 `%release`
 * 会被 percent_starts_directive 认作嵌套指令，使原本可自动改写的测试误报为需人工审核。
 * 应与 PlanSource 的注释词法状态使用同一判定边界。 */
static TZrBool legacy_migration_range_has_percent_directive(
        const TZrChar *source,
        TZrSize start,
        TZrSize end) {
    for (TZrSize index = start; index + 1U < end; index++) {
        if (source[index] == '%' &&
            legacy_migration_is_identifier_start(source[index + 1U]) &&
            legacy_migration_percent_starts_directive(source, index)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 从旧测试展示名生成普通函数名；碰撞时上层只给待审核建议。 */
static TZrChar *legacy_migration_test_identifier(
        SZrState *state,
        const TZrChar *source,
        TZrSize displayStart,
        TZrSize displayEnd) {
    TZrSize maximumLength = (displayEnd - displayStart) + strlen("testCase") + 1U;
    TZrChar *identifier;
    TZrSize writeOffset = 0U;
    TZrBool capitalizeNext = ZR_TRUE;

    identifier = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, maximumLength);
    if (identifier == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(identifier, "test", strlen("test"));
    writeOffset = strlen("test");
    for (TZrSize index = displayStart; index < displayEnd; index++) {
        TZrChar current = source[index];

        if (current == '\\' && index + 1U < displayEnd) {
            current = source[++index];
        }
        if (!isalnum((unsigned char)current)) {
            capitalizeNext = ZR_TRUE;
            continue;
        }
        if (capitalizeNext && isalpha((unsigned char)current)) {
            current = (TZrChar)toupper((unsigned char)current);
        }
        identifier[writeOffset++] = current;
        capitalizeNext = ZR_FALSE;
    }
    if (writeOffset == strlen("test")) {
        memcpy(identifier + writeOffset, "Case", strlen("Case"));
        writeOffset += strlen("Case");
    }
    identifier[writeOffset] = '\0';
    return identifier;
}

/* BUG: 当前按原文字节查重，`// testFoo` 或 `"testFoo"` 也会让
 * `%test("foo") {}` 的提案降级为人工审核，虽然它们不是声明；需要符号级查重。 */
static TZrBool legacy_migration_identifier_occurs(
        const TZrChar *source,
        TZrSize sourceLength,
        const TZrChar *identifier) {
    TZrSize identifierLength = strlen(identifier);

    for (TZrSize index = 0U; index + identifierLength <= sourceLength; index++) {
        if (memcmp(source + index, identifier, identifierLength) == 0 &&
            (index == 0U || !legacy_migration_is_identifier_continue(source[index - 1U])) &&
            (index + identifierLength == sourceLength ||
             !legacy_migration_is_identifier_continue(source[index + identifierLength]))) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 将旧测试提议为普通测试函数；返回约定和命名碰撞决定是否降级为人工审核。 */
static TZrBool legacy_migration_append_percent_test(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        const SZrLegacyMigrationDirectiveRule *rule,
        TZrSize percentOffset,
        TZrSize wordEnd,
        TZrSize *outConsumedEnd) {
    TZrSize openOffset = legacy_migration_skip_whitespace(source, sourceLength, wordEnd);
    TZrSize callEnd;
    TZrSize argumentStart;
    TZrSize argumentEnd;
    TZrSize blockStart;
    TZrSize blockEnd;
    TZrChar *identifier = ZR_NULL;
    TZrChar *editText = ZR_NULL;
    TZrBool collision;
    EZrLegacyMigrationApplicability applicability;
    const TZrChar *reason;
    TZrBool result;

    if (outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = wordEnd;
    }
    if (!legacy_migration_find_call_end(
                source, sourceLength, openOffset, &callEnd)) {
        return legacy_migration_append_item(
                state, plan, source, sourceName,
                rule->oldConstructKind, rule->targetConstructKind, rule->targetPlanId,
                ZR_LEGACY_MIGRATION_BLOCKED,
                "A legacy test migration requires a balanced display-name argument list.",
                percentOffset, wordEnd, percentOffset, wordEnd, ZR_NULL);
    }
    argumentStart = legacy_migration_skip_whitespace(
            source, callEnd - 1U, openOffset + 1U);
    argumentEnd = legacy_migration_trim_end(source, argumentStart, callEnd - 1U);
    /* BUG: 首尾引号不能证明括号内恰好只有一个静态字符串。
     * `%test("a", "b") {}` 通过此检查并获得 MACHINE_APPLICABLE，随后被改写成
     * `fn testAB(): void {}`；需让解析器或完整 token 检查确认唯一实参。 */
    if (argumentEnd <= argumentStart + 1U || source[argumentStart] != '"' ||
        source[argumentEnd - 1U] != '"') {
        return legacy_migration_append_item(
                state, plan, source, sourceName,
                rule->oldConstructKind, rule->targetConstructKind, rule->targetPlanId,
                ZR_LEGACY_MIGRATION_BLOCKED,
                "A legacy test migration requires exactly one static string display name.",
                percentOffset, callEnd, percentOffset, callEnd, ZR_NULL);
    }
    blockStart = legacy_migration_skip_whitespace(source, sourceLength, callEnd);
    if (blockStart >= sourceLength || source[blockStart] != '{' ||
        !legacy_migration_find_balanced_end(
                source, sourceLength, blockStart, '{', '}', &blockEnd)) {
        return legacy_migration_append_item(
                state, plan, source, sourceName,
                rule->oldConstructKind, rule->targetConstructKind, rule->targetPlanId,
                ZR_LEGACY_MIGRATION_BLOCKED,
                "A legacy test migration requires a balanced test body.",
                percentOffset, callEnd, percentOffset, callEnd, ZR_NULL);
    }
    if (outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = blockEnd;
    }
    if (legacy_migration_range_has_identifier(
                source, blockStart + 1U, blockEnd - 1U, "return") ||
        legacy_migration_range_has_percent_directive(
                source, blockStart + 1U, blockEnd - 1U)) {
        return legacy_migration_append_item(
                state, plan, source, sourceName,
                rule->oldConstructKind, rule->targetConstructKind, rule->targetPlanId,
                ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                "The legacy test body uses a return convention or nested percent syntax that requires semantic review.",
                percentOffset, blockEnd, percentOffset, blockEnd, ZR_NULL);
    }

    identifier = legacy_migration_test_identifier(
            state, source, argumentStart + 1U, argumentEnd - 1U);
    if (identifier == ZR_NULL) {
        return ZR_FALSE;
    }
    /* 名称碰撞可能改变测试发现结果，因此带稳定后缀的提案仍需人工确认。 */
    collision = legacy_migration_identifier_occurs(source, sourceLength, identifier);
    if (collision) {
        TZrSize baseLength = strlen(identifier);
        TZrChar *suffixed = (TZrChar *)ZrCore_Memory_RawMalloc(
                state->global, baseLength + 1U + 8U + 1U);
        if (suffixed == ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, identifier, baseLength + 1U);
            return ZR_FALSE;
        }
        snprintf(suffixed,
                 baseLength + 1U + 8U + 1U,
                 "%s_%08x",
                 identifier,
                 (unsigned int)(legacy_migration_hash(
                         source + percentOffset, blockEnd - percentOffset) & 0xffffffffU));
        ZrCore_Memory_RawFree(state->global, identifier, baseLength + 1U);
        identifier = suffixed;
    }

    {
        static const TZrChar prefix[] = "#zr.testing.test#\nfn ";
        static const TZrChar signature[] = "(): void ";
        TZrSize identifierLength = strlen(identifier);
        TZrSize bodyLength = blockEnd - blockStart;
        TZrSize editLength = strlen(prefix) + identifierLength +
                             strlen(signature) + bodyLength;
        TZrSize offset = 0U;

        editText = (TZrChar *)ZrCore_Memory_RawMalloc(
                state->global, editLength + 1U);
        if (editText == ZR_NULL) {
            ZrCore_Memory_RawFree(
                    state->global, identifier, identifierLength + 1U);
            return ZR_FALSE;
        }
        memcpy(editText + offset, prefix, strlen(prefix));
        offset += strlen(prefix);
        memcpy(editText + offset, identifier, identifierLength);
        offset += identifierLength;
        memcpy(editText + offset, signature, strlen(signature));
        offset += strlen(signature);
        memcpy(editText + offset, source + blockStart, bodyLength);
        offset += bodyLength;
        editText[offset] = '\0';
    }
    applicability = collision
                    ? ZR_LEGACY_MIGRATION_REQUIRES_REVIEW
                    : ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE;
    reason = collision
             ? "The generated test identifier collided; a stable hash suffix was proposed for review."
             : rule->reason;
    result = legacy_migration_append_item(
            state, plan, source, sourceName,
            rule->oldConstructKind, rule->targetConstructKind, rule->targetPlanId,
            applicability, reason,
            percentOffset, blockEnd, percentOffset, blockEnd, editText);
    ZrCore_Memory_RawFree(state->global, editText, strlen(editText) + 1U);
    ZrCore_Memory_RawFree(state->global, identifier, strlen(identifier) + 1U);
    return result;
}

/* 规则表先决定语义归属；只有已提升且结构足够明确的旧指令生成机器修复。 */
static TZrBool legacy_migration_append_directive(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        TZrSize percentOffset,
        TZrSize wordStart,
        TZrSize wordEnd,
        TZrSize *outConsumedEnd) {
    const SZrLegacyMigrationDirectiveRule *rule =
            legacy_migration_find_directive_rule(source, wordStart, wordEnd);
    TZrSize tokenEnd = wordEnd;
    TZrSize editEnd = tokenEnd;
    TZrChar *temporaryEdit = ZR_NULL;
    const TZrChar *editText = ZR_NULL;
    TZrBool result;

    if (outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = wordEnd;
    }

    if (rule == ZR_NULL) {
        return legacy_migration_append_item(
                state,
                plan,
                source,
                sourceName,
                "unrecognizedPercentDirective",
                "unrecognizedDirective",
                "06A",
                ZR_LEGACY_MIGRATION_BLOCKED,
                "The percent directive is not recognized by the migration rule table.",
                percentOffset,
                wordEnd,
                percentOffset,
                wordEnd,
                ZR_NULL);
    }
    if (strcmp(rule->directive, "test") == 0) {
        return legacy_migration_append_percent_test(
                state,
                plan,
                source,
                sourceLength,
                sourceName,
                rule,
                percentOffset,
                wordEnd,
                outConsumedEnd);
    }
    if (rule->applicability == ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE) {
        if (strcmp(rule->directive, "async") == 0) {
            editText = "async";
        } else if (strcmp(rule->directive, "await") == 0) {
            editText = "await";
        } else if (strcmp(rule->directive, "extern") == 0) {
            editText = "native extern";
        } else if (strcmp(rule->directive, "compileTime") == 0) {
            editText = "comptime";
        } else if (strcmp(rule->directive, "func") == 0) {
            editText = "fn";
        } else if (strcmp(rule->directive, "module") == 0) {
            TZrSize pathStart = legacy_migration_skip_space(source, sourceLength, wordEnd);
            TZrSize lineEnd = legacy_migration_trim_end(
                    source,
                    pathStart,
                    legacy_migration_line_end(source, sourceLength, pathStart));
            TZrSize pathLength = lineEnd - pathStart;

            if (pathLength == 0U) {
                return legacy_migration_append_item(
                        state,
                        plan,
                        source,
                        sourceName,
                        rule->oldConstructKind,
                        rule->targetConstructKind,
                        rule->targetPlanId,
                        ZR_LEGACY_MIGRATION_BLOCKED,
                        "A module migration requires a static module path.",
                        percentOffset,
                        wordEnd,
                        percentOffset,
                        wordEnd,
                        ZR_NULL);
            }
            temporaryEdit = (TZrChar *)ZrCore_Memory_RawMalloc(
                    state->global,
                    strlen("module ") + pathLength + strlen(";") + 1U);
            if (temporaryEdit == ZR_NULL) {
                return ZR_FALSE;
            }
            memcpy(temporaryEdit, "module ", strlen("module "));
            memcpy(temporaryEdit + strlen("module "), source + pathStart, pathLength);
            temporaryEdit[strlen("module ") + pathLength] = ';';
            temporaryEdit[strlen("module ") + pathLength + 1U] = '\0';
            editText = temporaryEdit;
            tokenEnd = lineEnd;
            editEnd = lineEnd;
        } else if (strcmp(rule->directive, "owned") == 0) {
            TZrSize declarationStart = legacy_migration_skip_space(source, sourceLength, wordEnd);
            TZrSize declarationEnd = legacy_migration_read_identifier(
                    source,
                    sourceLength,
                    declarationStart);

            if (!legacy_migration_span_equals(
                        source,
                        declarationStart,
                        declarationEnd,
                        "class")) {
                return legacy_migration_append_item(
                        state,
                        plan,
                        source,
                        sourceName,
                        rule->oldConstructKind,
                        rule->targetConstructKind,
                        rule->targetPlanId,
                        ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                        "The ownership shell is machine-applicable only before a class declaration.",
                        percentOffset,
                        wordEnd,
                        percentOffset,
                        wordEnd,
                        ZR_NULL);
            }
            editText = "resource";
        } else if (strcmp(rule->directive, "type") == 0) {
            TZrSize openOffset = legacy_migration_skip_space(source, sourceLength, wordEnd);
            TZrSize callEnd;
            TZrSize argumentStart;
            TZrSize argumentEnd;

            if (!legacy_migration_find_call_end(source, sourceLength, openOffset, &callEnd)) {
                return legacy_migration_append_item(
                        state,
                        plan,
                        source,
                        sourceName,
                        rule->oldConstructKind,
                        rule->targetConstructKind,
                        rule->targetPlanId,
                        ZR_LEGACY_MIGRATION_BLOCKED,
                        "A type reflection migration requires a balanced argument list.",
                        percentOffset,
                        wordEnd,
                        percentOffset,
                        wordEnd,
                        ZR_NULL);
            }
            argumentStart = legacy_migration_skip_space(source, sourceLength, openOffset + 1U);
            argumentEnd = legacy_migration_trim_end(source, argumentStart, callEnd - 1U);
            if (argumentStart == argumentEnd) {
                return legacy_migration_append_item(
                        state,
                        plan,
                        source,
                        sourceName,
                        rule->oldConstructKind,
                        rule->targetConstructKind,
                        rule->targetPlanId,
                        ZR_LEGACY_MIGRATION_BLOCKED,
                        "A type reflection migration requires one concrete expression.",
                        percentOffset,
                        callEnd,
                        percentOffset,
                        callEnd,
                        ZR_NULL);
            }
            editText = "typeof";
        } else {
            TZrSize openOffset = legacy_migration_skip_space(source, sourceLength, wordEnd);
            TZrSize callEnd;
            TZrSize argumentStart;
            TZrSize argumentEnd;

            if (!legacy_migration_find_call_end(source, sourceLength, openOffset, &callEnd)) {
                return legacy_migration_append_item(
                        state,
                        plan,
                        source,
                        sourceName,
                        rule->oldConstructKind,
                        rule->targetConstructKind,
                        rule->targetPlanId,
                        ZR_LEGACY_MIGRATION_BLOCKED,
                        "The legacy ownership builtin requires a balanced argument list.",
                        percentOffset,
                        wordEnd,
                        percentOffset,
                        wordEnd,
                        ZR_NULL);
            }
            argumentStart = legacy_migration_skip_space(source, sourceLength, openOffset + 1U);
            argumentEnd = legacy_migration_trim_end(source, argumentStart, callEnd - 1U);
            if (argumentStart == argumentEnd) {
                return legacy_migration_append_item(
                        state,
                        plan,
                        source,
                        sourceName,
                        rule->oldConstructKind,
                        rule->targetConstructKind,
                        rule->targetPlanId,
                        ZR_LEGACY_MIGRATION_BLOCKED,
                        "The legacy ownership builtin requires one concrete argument.",
                        percentOffset,
                        callEnd,
                        percentOffset,
                        callEnd,
                        ZR_NULL);
            }
            if (strcmp(rule->directive, "release") == 0) {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "drop(", ")");
            } else if (strcmp(rule->directive, "upgrade") == 0) {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "wake(", ")");
            } else if (strcmp(rule->directive, "weak") == 0) {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "degrade(", ")");
            } else if (strcmp(rule->directive, "shared") == 0) {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "share(", ")");
            } else {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "intoGc(", ")");
            }
            if (temporaryEdit == ZR_NULL) {
                return ZR_FALSE;
            }
            editText = temporaryEdit;
            tokenEnd = callEnd;
            editEnd = callEnd;
        }
    }
    result = legacy_migration_append_item(
            state,
            plan,
            source,
            sourceName,
            rule->oldConstructKind,
            rule->targetConstructKind,
            rule->targetPlanId,
            rule->applicability,
            rule->reason,
            percentOffset,
            tokenEnd,
            percentOffset,
            editEnd,
            editText);
    if (temporaryEdit != ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, temporaryEdit, strlen(temporaryEdit) + 1U);
    }
    if (result && outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = tokenEnd;
    }
    return result;
}

static TZrBool legacy_migration_word_precedes_call(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize wordEnd) {
    return legacy_migration_skip_space(source, sourceLength, wordEnd) < sourceLength &&
           source[legacy_migration_skip_space(source, sourceLength, wordEnd)] == '(';
}

static TZrBool legacy_migration_is_line_word_start(
        const TZrChar *source,
        TZrSize offset) {
    TZrSize index = offset;

    while (index > 0U && source[index - 1U] != '\n') {
        index--;
    }
    while (index < offset && (source[index] == ' ' || source[index] == '\t')) {
        index++;
    }
    return index == offset;
}

/* 当前 fn 可调用形态的箭头不是迁移项，避免把新语法报告成旧定义箭头。 */
static TZrBool legacy_migration_callable_open_preceded_by_fn(
        const TZrChar *source,
        TZrSize arrowOffset) {
    TZrSize closeOffset = arrowOffset;
    TZrSize openOffset;
    TZrSize wordEnd;
    TZrSize wordStart;
    TZrSize depth = 0U;

    while (closeOffset > 0U && source[closeOffset - 1U] != ')') {
        if (source[closeOffset - 1U] == '\n' || source[closeOffset - 1U] == ';' ||
            source[closeOffset - 1U] == '{' || source[closeOffset - 1U] == '}') {
            return ZR_FALSE;
        }
        closeOffset--;
    }
    if (closeOffset == 0U) {
        return ZR_FALSE;
    }

    openOffset = closeOffset;
    while (openOffset > 0U) {
        TZrChar current = source[--openOffset];
        if (current == ')') {
            depth++;
        } else if (current == '(') {
            if (depth == 0U) {
                return ZR_FALSE;
            }
            depth--;
            if (depth == 0U) {
                break;
            }
        }
    }
    if (source[openOffset] != '(') {
        return ZR_FALSE;
    }

    wordEnd = openOffset;
    while (wordEnd > 0U && isspace((unsigned char)source[wordEnd - 1U])) {
        wordEnd--;
    }
    wordStart = wordEnd;
    while (wordStart > 0U && legacy_migration_is_identifier_continue(source[wordStart - 1U])) {
        wordStart--;
    }
    return legacy_migration_span_equals(source, wordStart, wordEnd, "fn");
}

static TZrBool legacy_migration_append_word_item(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        SZrString *sourceName,
        const TZrChar *oldConstructKind,
        const TZrChar *targetConstructKind,
        const TZrChar *targetPlanId,
        EZrLegacyMigrationApplicability applicability,
        const TZrChar *reason,
        TZrSize start,
        TZrSize end,
        const TZrChar *editText) {
    return legacy_migration_append_item(
            state,
            plan,
            source,
            sourceName,
            oldConstructKind,
            targetConstructKind,
            targetPlanId,
            applicability,
            reason,
            start,
            end,
            start,
            end,
            editText);
}

/* 动态目标需要类型身份与实参装箱证明；即便能给出文本，也只供人工审核。 */
static TZrBool legacy_migration_append_dynamic_dollar_construct(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        TZrSize dollarOffset,
        TZrSize *outConsumedEnd) {
    TZrSize targetCallEnd;
    TZrSize targetStart;
    TZrSize targetEnd;
    TZrSize argumentOpen;
    TZrSize argumentCallEnd;
    TZrSize argumentStart;
    TZrSize argumentEnd;
    TZrChar *editText;
    TZrBool appended;

    if (outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = dollarOffset + 1U;
    }
    if (state == ZR_NULL || plan == ZR_NULL || source == ZR_NULL ||
        dollarOffset + 1U >= sourceLength ||
        source[dollarOffset] != '$' || source[dollarOffset + 1U] != '(') {
        return ZR_FALSE;
    }
    if (!legacy_migration_find_call_end(
                source, sourceLength, dollarOffset + 1U, &targetCallEnd)) {
        return legacy_migration_append_word_item(
                state,
                plan,
                source,
                sourceName,
                "legacyDynamicDollarConstruct",
                "constructorCall",
                "08",
                ZR_LEGACY_MIGRATION_BLOCKED,
                "Dynamic constructor target is not structurally complete.",
                dollarOffset,
                dollarOffset + 1U,
                ZR_NULL);
    }

    targetStart = legacy_migration_skip_whitespace(
            source, sourceLength, dollarOffset + 2U);
    targetEnd = targetCallEnd - 1U;
    while (targetEnd > targetStart &&
           isspace((unsigned char)source[targetEnd - 1U])) {
        targetEnd--;
    }
    argumentOpen = legacy_migration_skip_whitespace(
            source, sourceLength, targetCallEnd);
    if (targetStart == targetEnd || argumentOpen >= sourceLength ||
        source[argumentOpen] != '(' ||
        !legacy_migration_find_call_end(
                source, sourceLength, argumentOpen, &argumentCallEnd)) {
        if (outConsumedEnd != ZR_NULL) {
            *outConsumedEnd = targetCallEnd;
        }
        return legacy_migration_append_item(
                state,
                plan,
                source,
                sourceName,
                "legacyDynamicDollarConstruct",
                "constructorCall",
                "08",
                ZR_LEGACY_MIGRATION_BLOCKED,
                "Dynamic constructor arguments are not structurally complete.",
                dollarOffset,
                targetCallEnd,
                dollarOffset,
                targetCallEnd,
                ZR_NULL);
    }

    argumentStart = argumentOpen + 1U;
    argumentEnd = argumentCallEnd - 1U;
    editText = legacy_migration_format_dynamic_construct(
            state,
            source,
            targetStart,
            targetEnd,
            argumentStart,
            argumentEnd);
    if (editText == ZR_NULL) {
        return ZR_FALSE;
    }
    appended = legacy_migration_append_item(
            state,
            plan,
            source,
            sourceName,
            "legacyDynamicDollarConstruct",
            "reflectionConstruction",
            "08",
            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
            "Confirm the target is zr.reflection.Type and review argument boxing before applying.",
            dollarOffset,
            argumentCallEnd,
            dollarOffset,
            argumentCallEnd,
            editText);
    ZrCore_Memory_RawFree(
            state->global, editText, strlen(editText) + 1U);
    if (appended && outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = argumentCallEnd;
    }
    return appended;
}

/* 仅迁移已知的 draft 测试角色，保持属性参数并转到 zr.testing 提供者。 */
static TZrBool legacy_migration_append_old_test_attribute(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        TZrSize start,
        TZrSize *outEnd) {
    static const TZrChar prefix[] = "#zr.test.";
    static const TZrChar replacementPrefix[] = "#zr.testing.";
    TZrSize roleStart;
    TZrSize roleEnd;
    TZrSize end;
    TZrSize replacementLength;
    TZrChar *replacement;
    TZrBool knownRole;
    TZrBool result;

    if (outEnd != ZR_NULL) {
        *outEnd = start;
    }
    if (start + strlen(prefix) > sourceLength ||
        memcmp(source + start, prefix, strlen(prefix)) != 0) {
        return ZR_FALSE;
    }
    roleStart = start + strlen(prefix);
    roleEnd = legacy_migration_read_identifier(source, sourceLength, roleStart);
    end = roleEnd;
    knownRole = legacy_migration_span_equals(source, roleStart, roleEnd, "test") ||
                legacy_migration_span_equals(source, roleStart, roleEnd, "case") ||
                legacy_migration_span_equals(source, roleStart, roleEnd, "skip");
    if (!knownRole || end >= sourceLength) {
        return ZR_FALSE;
    }
    if (source[end] == '(') {
        if (!legacy_migration_find_call_end(source, sourceLength, end, &end)) {
            return ZR_FALSE;
        }
    }
    if (end >= sourceLength || source[end] != '#') {
        return ZR_FALSE;
    }
    end++;

    replacementLength = strlen(replacementPrefix) + (end - 1U - roleStart) + 1U;
    replacement = (TZrChar *)ZrCore_Memory_RawMalloc(
            state->global, replacementLength + 1U);
    if (replacement == ZR_NULL) {
        return ZR_FALSE;
    }
    memcpy(replacement, replacementPrefix, strlen(replacementPrefix));
    memcpy(replacement + strlen(replacementPrefix),
           source + roleStart,
           end - 1U - roleStart);
    replacement[replacementLength - 1U] = '#';
    replacement[replacementLength] = '\0';
    result = legacy_migration_append_item(
            state,
            plan,
            source,
            sourceName,
            "legacyTestAttribute",
            "testingAttribute",
            "14",
            ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
            "The draft zr.test role is now owned by the canonical zr.testing provider.",
            start,
            end,
            start,
            end,
            replacement);
    ZrCore_Memory_RawFree(
            state->global, replacement, replacementLength + 1U);
    if (result && outEnd != ZR_NULL) {
        *outEnd = end;
    }
    return result;
}

static TZrBool legacy_migration_word_is_test_function_prefix(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize wordEnd) {
    TZrSize next = legacy_migration_skip_whitespace(source, sourceLength, wordEnd);
    TZrSize nextEnd;

    if (next >= sourceLength || !legacy_migration_is_identifier_start(source[next])) {
        return ZR_FALSE;
    }
    nextEnd = legacy_migration_read_identifier(source, sourceLength, next);
    if (legacy_migration_span_equals(source, next, nextEnd, "fn")) {
        return ZR_TRUE;
    }
    if (!legacy_migration_span_equals(source, next, nextEnd, "async")) {
        return ZR_FALSE;
    }
    next = legacy_migration_skip_whitespace(source, sourceLength, nextEnd);
    nextEnd = legacy_migration_read_identifier(source, sourceLength, next);
    return legacy_migration_span_equals(source, next, nextEnd, "fn");
}

/* 只替换裸 import 的模块字面量，不建立旧 debug 模块的运行时别名。 */
static TZrBool legacy_migration_append_bare_debug_import(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        TZrSize wordStart,
        TZrSize wordEnd,
        TZrSize *outConsumedEnd) {
    SZrLegacyMigrationModuleSpecifierMatch match;

    if (outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = wordEnd;
    }
    if (!ZrParser_LegacyMigrationModuleSpecifier_TryMatchBareDebugImport(
                source,
                sourceLength,
                wordStart,
                wordEnd,
                &match)) {
        return ZR_TRUE;
    }

    if (!legacy_migration_append_item(
                state,
                plan,
                source,
                sourceName,
                "legacyDebugModuleSpecifier",
                "canonicalModuleSpecifier",
                "10",
                ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
                "The bare debug native module was removed; use canonical zr.debug.",
                wordStart,
                match.itemEnd,
                match.editStart,
                match.editEnd,
                "zr.debug")) {
        return ZR_FALSE;
    }
    if (outConsumedEnd != ZR_NULL) {
        *outConsumedEnd = match.itemEnd;
    }
    return ZR_TRUE;
}

/* 应用器要求按源偏移顺序排列的非重叠机器编辑。
 * BUG: property 修复先由解析器加入，其他修复再按源码顺序加入；若裸
 * `import("debug")` 位于配对 property 之前，两个不相交的编辑会被误判为重叠，
 * `ApplyMachineEdits` 因 hasOverlap 拒绝整份计划；现有测试分别覆盖这两类修复。 */
static TZrBool legacy_migration_has_machine_overlap(const SZrLegacyMigrationPlan *plan) {
    TZrSize index;
    TZrSize previousEnd = 0U;
    TZrBool hasPrevious = ZR_FALSE;

    for (index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);

        if (item == ZR_NULL ||
            item->applicability != ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE ||
            !item->hasFix) {
            continue;
        }
        if (hasPrevious && item->fix.editRange.start.offset < previousEnd) {
            return ZR_TRUE;
        }
        previousEnd = item->fix.editRange.end.offset;
        hasPrevious = ZR_TRUE;
    }
    return ZR_FALSE;
}

/* CLI、LSP 格式化入口和迁移测试共用的候选生产入口：先投影解析器掌握的 property 事实，
 * 再扫描其他旧表层；失败时清理计划，成功时由调用方负责 PlanFree。 */
ZR_PARSER_API TZrBool ZrParser_LegacyMigration_PlanSource(
        SZrState *state,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        SZrLegacyMigrationPlan *outPlan) {
    EZrLegacyMigrationLexState lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
    TZrSize index = 0U;

    if (outPlan != ZR_NULL) {
        memset(outPlan, 0, sizeof(*outPlan));
    }
    if (state == ZR_NULL || source == ZR_NULL || sourceName == ZR_NULL || outPlan == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Init(state, &outPlan->items, sizeof(SZrLegacyMigrationItem), 16U);
    outPlan->sourceHash = legacy_migration_hash(source, sourceLength);
    legacy_migration_capture_property_facts(state, outPlan, source, sourceLength, sourceName);
    while (index < sourceLength) {
        TZrChar current = source[index];

        if (lexState == ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT) {
            if (current == '\n') {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
            }
            index++;
            continue;
        }
        if (lexState == ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT) {
            if (current == '*' && index + 1U < sourceLength && source[index + 1U] == '/') {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
                index += 2U;
            } else {
                index++;
            }
            continue;
        }
        if (lexState == ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING ||
            lexState == ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING) {
            TZrChar terminator = lexState == ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING ? '"' : '`';

            if (current == '\\' && index + 1U < sourceLength) {
                index += 2U;
            } else if (current == terminator) {
                lexState = ZR_LEGACY_MIGRATION_LEX_CODE;
                index++;
            } else {
                index++;
            }
            continue;
        }
        if (current == '/' && index + 1U < sourceLength && source[index + 1U] == '/') {
            lexState = ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT;
            index += 2U;
            continue;
        }
        if (current == '/' && index + 1U < sourceLength && source[index + 1U] == '*') {
            lexState = ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT;
            index += 2U;
            continue;
        }
        if (current == '"') {
            lexState = ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING;
            index++;
            continue;
        }
        if (current == '`') {
            lexState = ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING;
            index++;
            continue;
        }
        if (current == '#') {
            TZrSize attributeEnd = index;

            /* TODO: helper 用同一个 false 表示非匹配与分配失败；核查内存失败时
             * 是否应终止规划，避免静默略过本应生成的旧测试属性候选。 */
            if (legacy_migration_append_old_test_attribute(
                        state,
                        outPlan,
                        source,
                        sourceLength,
                        sourceName,
                        index,
                        &attributeEnd)) {
                index = attributeEnd;
                continue;
            }
        }
        if (current == '%' && index + 1U < sourceLength &&
            legacy_migration_is_identifier_start(source[index + 1U]) &&
            legacy_migration_percent_starts_directive(source, index)) {
            TZrSize wordStart = index + 1U;
            TZrSize wordEnd = legacy_migration_read_identifier(source, sourceLength, wordStart);
            TZrSize consumedEnd = wordEnd;

            if (!legacy_migration_append_directive(
                        state,
                        outPlan,
                        source,
                        sourceLength,
                        sourceName,
                        index,
                        wordStart,
                        wordEnd,
                        &consumedEnd)) {
                ZrParser_LegacyMigration_PlanFree(state, outPlan);
                return ZR_FALSE;
            }
            index = consumedEnd;
            continue;
        }
        if (current == '$') {
            if (index + 1U < sourceLength && source[index + 1U] == '(') {
                TZrSize consumedEnd = index + 1U;

                if (!legacy_migration_append_dynamic_dollar_construct(
                            state,
                            outPlan,
                            source,
                            sourceLength,
                            sourceName,
                            index,
                            &consumedEnd)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
                index = consumedEnd;
                continue;
            } else if (index + 1U < sourceLength &&
                       legacy_migration_is_identifier_start(source[index + 1U])) {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "legacyDollarConstruct",
                            "constructorCall",
                            "03",
                            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Static constructor migration requires a resolved TypeRef binding.",
                            index,
                            index + 1U,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            }
            index++;
            continue;
        }
        if (current == '=' && index + 1U < sourceLength && source[index + 1U] == '>') {
            if (legacy_migration_callable_open_preceded_by_fn(source, index)) {
                index += 2U;
                continue;
            }
            if (!legacy_migration_append_word_item(
                        state,
                        outPlan,
                        source,
                        sourceName,
                        "legacyFunctionTypeArrow",
                        "functionTypeArrow",
                        "06A",
                        ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                        "Callable type migration requires parser-owned declaration context.",
                        index,
                        index + 2U,
                        ZR_NULL)) {
                ZrParser_LegacyMigration_PlanFree(state, outPlan);
                return ZR_FALSE;
            }
            index += 2U;
            continue;
        }
        if (current == '-' && index + 1U < sourceLength && source[index + 1U] == '>') {
            if (legacy_migration_callable_open_preceded_by_fn(source, index)) {
                index += 2U;
                continue;
            }
            if (!legacy_migration_append_word_item(
                        state,
                        outPlan,
                        source,
                        sourceName,
                        "legacyDefinitionArrow",
                        "returnTypeDelimiter",
                        "06A",
                        ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                        "Function declaration migration requires parser-owned declaration context.",
                        index,
                        index + 2U,
                        ZR_NULL)) {
                ZrParser_LegacyMigration_PlanFree(state, outPlan);
                return ZR_FALSE;
            }
            index += 2U;
            continue;
        }
        if (legacy_migration_is_identifier_start(current)) {
            TZrSize wordEnd = legacy_migration_read_identifier(source, sourceLength, index);
            TZrSize consumedEnd = wordEnd;

            if (legacy_migration_span_equals(source, index, wordEnd, "import")) {
                if (!legacy_migration_append_bare_debug_import(
                            state,
                            outPlan,
                            source,
                            sourceLength,
                            sourceName,
                            index,
                            wordEnd,
                            &consumedEnd)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (legacy_migration_span_equals(source, index, wordEnd, "test") &&
                legacy_migration_word_is_test_function_prefix(
                        source, sourceLength, wordEnd)) {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "legacyTestFunctionKeyword",
                            "testingAttribute",
                            "14",
                            ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
                            "The draft test function becomes an ordinary function with the canonical test role.",
                            index,
                            wordEnd,
                            "#zr.testing.test#")) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (legacy_migration_span_equals(source, index, wordEnd, "func")) {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "legacyFuncKeyword",
                            "functionDeclaration",
                            "06A",
                            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Function declaration migration requires parser-owned declaration context.",
                            index,
                            wordEnd,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (legacy_migration_span_equals(source, index, wordEnd, "keywordless") &&
                       legacy_migration_is_line_word_start(source, index) &&
                       legacy_migration_word_precedes_call(source, sourceLength, wordEnd)) {
                if (!legacy_migration_append_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "keywordlessFunction",
                            "functionDeclaration",
                            "06A",
                            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Keywordless function migration requires a parser-owned declaration binding.",
                            index,
                            wordEnd,
                            index,
                            index,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (legacy_migration_span_equals(source, index, wordEnd, "new")) {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "legacyNewStruct",
                            "constructorCall",
                            "06A",
                            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Struct construction requires a resolved type identity.",
                            index,
                            wordEnd,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (legacy_migration_span_equals(source, index, wordEnd, "nativeFactory")) {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "nativePrototypeFactory",
                            "nativeFactory",
                            "10",
                            ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
                            "Native prototype factory migration remains owned by plan 10.",
                            index,
                            wordEnd,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (legacy_migration_span_equals(source, index, wordEnd, "pub") &&
                       !legacy_migration_has_item_kind(outPlan, "legacyPropertyAccessor")) {
                TZrSize next = legacy_migration_skip_space(source, sourceLength, wordEnd);
                TZrSize nextEnd = legacy_migration_read_identifier(source, sourceLength, next);

                if (next < sourceLength && legacy_migration_span_equals(source, next, nextEnd, "get") &&
                    !legacy_migration_append_word_item(
                            state, outPlan, source, sourceName, "legacyPropertyAccessor",
                            "propertyDeclaration", "05", ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Property migration remains owned by the paired accessor producer.",
                            index, nextEnd, ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            } else if (isupper((unsigned char)source[index]) &&
                       legacy_migration_word_precedes_call(source, sourceLength, wordEnd) &&
                       (index == 0U || source[index - 1U] != '$')) {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "legacyBareTypeCall",
                            "constructorCall",
                            "06A",
                            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Bare constructor calls require resolved type identity.",
                            index,
                            wordEnd,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
            }
            index = consumedEnd;
            continue;
        }
        index++;
    }
    outPlan->hasOverlap = legacy_migration_has_machine_overlap(outPlan);
    if (outPlan->hasOverlap) {
        (void)legacy_migration_append_item(
                state,
                outPlan,
                source,
                sourceName,
                "migrationOverlappingEdits",
                "migrationPlan",
                "06A",
                ZR_LEGACY_MIGRATION_BLOCKED,
                "Machine migration edits overlap and cannot be applied safely.",
                0U,
                0U,
                0U,
                0U,
                ZR_NULL);
    }
    return ZR_TRUE;
}

/* CLI、LSP 和测试均在使用后清理计划；它也可用于部分构造后的失败路径。 */
ZR_PARSER_API void ZrParser_LegacyMigration_PlanFree(
        SZrState *state,
        SZrLegacyMigrationPlan *plan) {
    if (plan == ZR_NULL) {
        return;
    }
    if (state != ZR_NULL) {
        ZrCore_Array_Free(state, &plan->items);
    }
    memset(plan, 0, sizeof(*plan));
}

/* 仅处理机器可用的 fix；源哈希与重叠标记保护旧偏移，结果由调用方 RawFree。
 * 逆序复制以保持每项相对原始源码的区间有效，review-only 候选原样保留。 */
ZR_PARSER_API TZrBool ZrParser_LegacyMigration_ApplyMachineEdits(
        SZrState *state,
        const SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        TZrChar **outText,
        TZrSize *outLength) {
    TZrSize index;
    TZrSize newLength = sourceLength;
    TZrSize tail = sourceLength;
    TZrSize writeOffset;
    TZrChar *result;

    if (outText != ZR_NULL) {
        *outText = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0U;
    }
    if (state == ZR_NULL || plan == ZR_NULL || source == ZR_NULL || outText == ZR_NULL ||
        outLength == ZR_NULL || plan->hasOverlap || plan->sourceHash != legacy_migration_hash(source, sourceLength)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);
        TZrSize editLength;
        TZrSize rangeLength;

        if (item == ZR_NULL ||
            item->applicability != ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE ||
            item->fix.applicability != ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE ||
            !item->hasFix) {
            continue;
        }
        if (item->fix.editText == ZR_NULL || item->fix.editRange.start.offset > item->fix.editRange.end.offset ||
            item->fix.editRange.end.offset > sourceLength) {
            return ZR_FALSE;
        }
        editLength = ZrCore_String_GetByteLength(item->fix.editText);
        rangeLength = item->fix.editRange.end.offset - item->fix.editRange.start.offset;
        if (editLength >= rangeLength) {
            newLength += editLength - rangeLength;
        } else {
            newLength -= rangeLength - editLength;
        }
    }
    result = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, newLength + 1U);
    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    writeOffset = newLength;
    for (index = plan->items.length; index > 0U; index--) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index - 1U);
        TZrSize editStart;
        TZrSize editEnd;
        TZrSize suffixLength;
        TZrSize editLength;

        if (item == ZR_NULL ||
            item->applicability != ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE ||
            item->fix.applicability != ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE ||
            !item->hasFix) {
            continue;
        }
        editStart = item->fix.editRange.start.offset;
        editEnd = item->fix.editRange.end.offset;
        if (editEnd > tail || editStart > editEnd) {
            ZrCore_Memory_RawFree(state->global, result, newLength + 1U);
            return ZR_FALSE;
        }
        suffixLength = tail - editEnd;
        writeOffset -= suffixLength;
        if (suffixLength > 0U) {
            memcpy(result + writeOffset, source + editEnd, suffixLength);
        }
        editLength = ZrCore_String_GetByteLength(item->fix.editText);
        writeOffset -= editLength;
        if (editLength > 0U) {
            memcpy(result + writeOffset, ZrCore_String_GetNativeString(item->fix.editText), editLength);
        }
        tail = editStart;
    }
    if (tail > 0U) {
        writeOffset -= tail;
        memcpy(result + writeOffset, source, tail);
    }
    if (writeOffset != 0U) {
        ZrCore_Memory_RawFree(state->global, result, newLength + 1U);
        return ZR_FALSE;
    }
    result[newLength] = '\0';
    *outText = result;
    *outLength = newLength;
    return ZR_TRUE;
}
