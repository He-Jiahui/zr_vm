#include "zr_vm_parser/legacy_migration.h"

#include <ctype.h>
#include <string.h>

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"

typedef enum EZrLegacyMigrationLexState {
    ZR_LEGACY_MIGRATION_LEX_CODE = 0,
    ZR_LEGACY_MIGRATION_LEX_LINE_COMMENT,
    ZR_LEGACY_MIGRATION_LEX_BLOCK_COMMENT,
    ZR_LEGACY_MIGRATION_LEX_QUOTED_STRING,
    ZR_LEGACY_MIGRATION_LEX_BACKTICK_STRING
} EZrLegacyMigrationLexState;

typedef struct SZrLegacyMigrationDirectiveRule {
    const TZrChar *directive;
    const TZrChar *oldConstructKind;
    const TZrChar *targetConstructKind;
    const TZrChar *targetPlanId;
    EZrLegacyMigrationApplicability applicability;
    const TZrChar *reason;
} SZrLegacyMigrationDirectiveRule;

static const SZrLegacyMigrationDirectiveRule k_legacy_migration_directive_rules[] = {
        {"module", "percentModule", "moduleDeclaration", "06B",
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "The module declaration target is not accepted by the current parser."},
        {"import", "percentImport", "importBinding", "06A",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Legacy imports require module-scope binding and alias proof."},
        {"async", "percentAsync", "asyncDeclaration", "12",
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "The async target syntax is not promoted in the current parser."},
        {"await", "percentAwait", "awaitExpression", "12",
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "The async target syntax is not promoted in the current parser."},
        {"extern", "percentExtern", "nativeDeclaration", "10",
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "Native declaration migration remains owned by plan 10."},
        {"test", "percentTest", "testDeclaration", "14",
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "Test declaration migration remains owned by plan 14."},
        {"compileTime", "percentCompileTime", "compileTimeDeclaration", "11",
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "Compile-time declaration migration remains owned by plan 11."},
        {"func", "percentFunc", "functionType", "06A",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Callable type migration requires parser-owned declaration context."},
        {"owned", "percentOwned", "resourceModifier", "04",
         ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE,
         "The ownership shell maps to the promoted resource modifier."},
        {"release", "percentRelease", "dropCall", "04",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Release migration requires the canonical resource ownership proof."},
        {"upgrade", "percentUpgrade", "weakUpgrade", "04",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Weak upgrade migration requires the canonical receiver type."},
        {"weak", "percentWeak", "weakProjection", "04",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Weak conversion migration requires the canonical ownership source."},
        {"shared", "percentShared", "sharedProjection", "04",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Shared conversion migration requires the canonical ownership source."},
        {"detach", "percentDetach", "ownershipDetach", "04",
         ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
         "Detach requires ownership-source proof before it can be rewritten."},
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
         ZR_LEGACY_MIGRATION_TARGET_NOT_PROMOTED,
         "Type reflection migration remains owned by plan 08."},
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

static TZrBool legacy_migration_find_call_end(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize openOffset,
        TZrSize *outEnd) {
    TZrSize index;
    TZrSize depth = 0U;

    if (openOffset >= sourceLength || source[openOffset] != '(' || outEnd == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = openOffset; index < sourceLength; index++) {
        if (source[index] == '(') {
            depth++;
        } else if (source[index] == ')') {
            depth--;
            if (depth == 0U) {
                *outEnd = index + 1U;
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

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
        item.fix.applicability = ZR_DIAGNOSTIC_FIX_MACHINE_APPLICABLE;
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

typedef struct SZrLegacyMigrationPropertyCapture {
    SZrState *state;
    SZrLegacyMigrationPlan *plan;
    const TZrChar *source;
    SZrString *sourceName;
} SZrLegacyMigrationPropertyCapture;

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
    ast = ZrParser_ParseWithState(&parserState);
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrParser_State_Free(&parserState);
}

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

static TZrBool legacy_migration_append_directive(
        SZrState *state,
        SZrLegacyMigrationPlan *plan,
        const TZrChar *source,
        TZrSize sourceLength,
        SZrString *sourceName,
        TZrSize percentOffset,
        TZrSize wordStart,
        TZrSize wordEnd) {
    const SZrLegacyMigrationDirectiveRule *rule =
            legacy_migration_find_directive_rule(source, wordStart, wordEnd);
    TZrSize tokenEnd = wordEnd;
    TZrSize editEnd = tokenEnd;
    TZrChar *temporaryEdit = ZR_NULL;
    const TZrChar *editText = ZR_NULL;
    TZrBool result;

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
    if (rule->applicability == ZR_LEGACY_MIGRATION_MACHINE_APPLICABLE) {
        if (strcmp(rule->directive, "module") == 0) {
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
                        state, source, argumentStart, argumentEnd, "", ".upgrade()");
            } else if (strcmp(rule->directive, "weak") == 0) {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "", ".weak()");
            } else {
                temporaryEdit = legacy_migration_format_unary_call(
                        state, source, argumentStart, argumentEnd, "", ".share()");
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

static TZrBool legacy_migration_has_machine_overlap(const SZrLegacyMigrationPlan *plan) {
    TZrSize index;
    TZrSize previousEnd = 0U;
    TZrBool hasPrevious = ZR_FALSE;

    for (index = 0U; index < plan->items.length; index++) {
        const SZrLegacyMigrationItem *item =
                (const SZrLegacyMigrationItem *)ZrCore_Array_Get((SZrArray *)&plan->items, index);

        if (item == ZR_NULL || !item->hasFix) {
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
        if (current == '%' && index + 1U < sourceLength &&
            legacy_migration_is_identifier_start(source[index + 1U])) {
            TZrSize wordStart = index + 1U;
            TZrSize wordEnd = legacy_migration_read_identifier(source, sourceLength, wordStart);

            if (!legacy_migration_append_directive(
                        state, outPlan, source, sourceLength, sourceName, index, wordStart, wordEnd)) {
                ZrParser_LegacyMigration_PlanFree(state, outPlan);
                return ZR_FALSE;
            }
            index = wordEnd;
            continue;
        }
        if (current == '$') {
            if (index + 1U < sourceLength && source[index + 1U] == '(') {
                if (!legacy_migration_append_word_item(
                            state,
                            outPlan,
                            source,
                            sourceName,
                            "legacyDynamicDollarConstruct",
                            "constructorCall",
                            "06A",
                            ZR_LEGACY_MIGRATION_REQUIRES_REVIEW,
                            "Dynamic constructor targets require resolved type identity.",
                            index,
                            index + 1U,
                            ZR_NULL)) {
                    ZrParser_LegacyMigration_PlanFree(state, outPlan);
                    return ZR_FALSE;
                }
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

            if (legacy_migration_span_equals(source, index, wordEnd, "func")) {
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
            index = wordEnd;
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

        if (item == ZR_NULL || !item->hasFix) {
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

        if (item == ZR_NULL || !item->hasFix) {
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
