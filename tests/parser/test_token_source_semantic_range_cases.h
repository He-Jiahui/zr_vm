#ifndef ZR_TEST_TOKEN_SOURCE_SEMANTIC_RANGE_CASES_H
#define ZR_TEST_TOKEN_SOURCE_SEMANTIC_RANGE_CASES_H

#include "compiler/tool/compile_tool_binding.h"
#include "parser/parser_ownership.h"
#include "parser/parser_range_evidence.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_display.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/type_inference.h"

enum {
    ZR_TEST_SEMANTIC_RANGE_STREAMS = 6,
    ZR_TEST_SEMANTIC_RANGE_PADDING = 64
};

typedef struct SZrTokenSemanticRangeFixture {
    SZrSyntaxContextTable *table;
    SZrTokenStream *streams[ZR_TEST_SEMANTIC_RANGE_STREAMS];
    TZrSize streamCount;
    SZrParserState parser;
    TZrBool parserInitialized;
    SZrAstNode *ast;
    SZrCompilerState compiler;
    TZrBool compilerInitialized;
    SZrInferredType inferred;
    SZrInferredType returnType;
    SZrInferredType parameterType;
    TZrBool inferredInitialized;
    TZrBool callableTypesInitialized;
    SZrArray parameterTypes;
    SZrGcRootLease entryNameRoot;
    char longName[ZR_VM_SHORT_STRING_MAX + 33U];
    char paddedSource[96];
} SZrTokenSemanticRangeFixture;

static SZrTokenSemanticRangeFixture g_tokenSemanticRange;

static void token_source_semantic_range_cleanup(void) {
    TZrSize index;

    if (g_tokenSemanticRange.inferredInitialized) {
        ZrParser_InferredType_Free(g_state, &g_tokenSemanticRange.inferred);
    }
    if (g_tokenSemanticRange.compilerInitialized) {
        ZrParser_CompilerState_Free(&g_tokenSemanticRange.compiler);
    }
    if (g_tokenSemanticRange.parameterTypes.isValid) {
        ZrCore_Array_Free(g_state, &g_tokenSemanticRange.parameterTypes);
    }
    if (g_tokenSemanticRange.callableTypesInitialized) {
        ZrParser_InferredType_Free(g_state, &g_tokenSemanticRange.parameterType);
        ZrParser_InferredType_Free(g_state, &g_tokenSemanticRange.returnType);
    }
    ZrParser_Ast_Free(g_state, g_tokenSemanticRange.ast);
    if (g_tokenSemanticRange.parserInitialized) {
        ZrParser_State_Free(&g_tokenSemanticRange.parser);
    }
    if (g_state != ZR_NULL) {
        ZrCore_Gc_RootLeaseRelease(g_state->global, &g_tokenSemanticRange.entryNameRoot);
    }
    for (index = g_tokenSemanticRange.streamCount; index > 0U; --index) {
        ZrParser_TokenStream_Release(g_tokenSemanticRange.streams[index - 1U]);
    }
    ZrParser_SyntaxContextTable_Release(g_tokenSemanticRange.table);
    memset(&g_tokenSemanticRange, 0, sizeof(g_tokenSemanticRange));
}

static void semantic_range_table(void) {
    g_tokenSemanticRange.table = ZrParser_SyntaxContextTable_Create(g_state->global, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tokenSemanticRange.table,
            "Fixture prerequisite: semantic range context table allocation failed");
}

static SZrTokenStream **semantic_range_stream_slot(void) {
    TZrSize index = g_tokenSemanticRange.streamCount;

    TEST_ASSERT_TRUE_MESSAGE(index < ZR_TEST_SEMANTIC_RANGE_STREAMS,
            "Fixture prerequisite: semantic range stream inventory overflowed");
    g_tokenSemanticRange.streamCount++;
    return &g_tokenSemanticRange.streams[index];
}

static SZrTokenStream *semantic_range_from_text(const char *name, const char *source) {
    SZrTokenStream **slot = semantic_range_stream_slot();

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_FromText(g_state, g_tokenSemanticRange.table,
            name, strlen(name), source, strlen(source), ZR_NULL, slot) && *slot != ZR_NULL &&
            ZrParser_TokenStream_IsSealed(*slot),
            "Fixture prerequisite: semantic range source tokenization failed");
    return *slot;
}

static SZrTokenStream *semantic_range_concat(SZrTokenStream *left, SZrTokenStream *right) {
    SZrTokenStream **slot = semantic_range_stream_slot();

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_Concat(left, right, ZR_NULL, slot) &&
            *slot != ZR_NULL && ZrParser_TokenStream_IsSealed(*slot),
            "Fixture prerequisite: semantic range source concatenation failed");
    return *slot;
}

static const char *semantic_range_long_name(void) {
    TZrSize length = sizeof(g_tokenSemanticRange.longName) - 1U;

    memset(g_tokenSemanticRange.longName, 'n', length);
    memcpy(g_tokenSemanticRange.longName, "semantic_", sizeof("semantic_") - 1U);
    memcpy(g_tokenSemanticRange.longName + length - 3U, ".zr", 3U);
    g_tokenSemanticRange.longName[length] = '\0';
    TEST_ASSERT_TRUE_MESSAGE(length > ZR_VM_SHORT_STRING_MAX,
            "Fixture prerequisite: semantic range source name must use the long string representation");
    return g_tokenSemanticRange.longName;
}

static const char *semantic_range_padded(const char *suffix) {
    TZrSize length = strlen(suffix);

    TEST_ASSERT_TRUE_MESSAGE(length + ZR_TEST_SEMANTIC_RANGE_PADDING <
            sizeof(g_tokenSemanticRange.paddedSource),
            "Fixture prerequisite: semantic range padded source exceeded its fixture buffer");
    memset(g_tokenSemanticRange.paddedSource, ' ', ZR_TEST_SEMANTIC_RANGE_PADDING);
    memcpy(g_tokenSemanticRange.paddedSource + ZR_TEST_SEMANTIC_RANGE_PADDING, suffix, length + 1U);
    return g_tokenSemanticRange.paddedSource;
}

static void semantic_range_entry_name(const char *name) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, (TZrNativeString)name);

    TEST_ASSERT_NOT_NULL_MESSAGE(sourceName,
            "Fixture prerequisite: semantic range parser source name allocation failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Gc_RootLeaseAcquire(g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(sourceName), &g_tokenSemanticRange.entryNameRoot),
            "Fixture prerequisite: semantic range parser source name rooting failed");
    g_tokenSemanticRange.parserInitialized = ZR_TRUE;
}

static SZrString *semantic_range_entry_string(void) {
    return (SZrString *)g_tokenSemanticRange.entryNameRoot.object;
}

static void semantic_range_finish_parse(void) {
    g_tokenSemanticRange.parser.suppressErrorOutput = ZR_TRUE;
    g_tokenSemanticRange.ast = ZrParser_ParseWithState(&g_tokenSemanticRange.parser);
    TEST_ASSERT_NOT_NULL_MESSAGE(g_tokenSemanticRange.ast,
            "Fixture prerequisite: semantic range parse did not produce an AST");
    TEST_ASSERT_TRUE_MESSAGE(g_tokenSemanticRange.parser.lexer != ZR_NULL &&
            !g_tokenSemanticRange.parser.hasError && !g_tokenSemanticRange.parser.hasFatalError &&
            !g_tokenSemanticRange.parser.lexer->hasSourceEvidenceError &&
            !g_tokenSemanticRange.parser.lexer->t.hasLexError &&
            g_tokenSemanticRange.parser.lexer->t.token == ZR_TK_EOS,
            "Fixture prerequisite: semantic range parse must consume the input without errors");
    TEST_ASSERT_TRUE_MESSAGE(g_tokenSemanticRange.ast->type == ZR_AST_SCRIPT &&
            g_tokenSemanticRange.ast->data.script.statements != ZR_NULL &&
            g_tokenSemanticRange.ast->data.script.statements->count == 1U &&
            g_tokenSemanticRange.ast->ownership == g_tokenSemanticRange.parser.lexer->ownership,
            "Fixture prerequisite: semantic range script ownership or statement count changed");
}

static void semantic_range_parse_stream(SZrTokenStream *stream) {
    semantic_range_entry_name("semantic_expansion.zr");
    ZrParser_State_InitWithTokenStream(&g_tokenSemanticRange.parser, g_state,
            stream, semantic_range_entry_string());
    semantic_range_finish_parse();
}

static void semantic_range_parse_text(const char *source) {
    semantic_range_entry_name("semantic_plain.zr");
    ZrParser_State_Init(&g_tokenSemanticRange.parser, g_state, source,
            strlen(source), semantic_range_entry_string());
    semantic_range_finish_parse();
}

static void semantic_range_require_leaf(const SZrTokenStream *stream, const SZrTokenTree *tree,
        EZrToken kind, const char *name, TZrSize start, TZrSize end, const char *rawText) {
    SZrTokenSpan span;
    SZrTokenText rawToken;
    SZrTokenNativeSemInfo semInfo;
    const char *sourceName;
    const char *source;
    const char *rawBytes;
    TZrSize nameLength = 0U;
    TZrSize sourceLength = 0U;

    TEST_ASSERT_NOT_NULL_MESSAGE(tree, "Fixture prerequisite: semantic range native leaf is missing");
    span = ZrParser_TokenTree_Span(tree);
    rawToken = ZrParser_TokenTree_Text(tree);
    sourceName = ZrParser_TokenStream_SourceName(stream, span.sourceId, &nameLength);
    source = ZrParser_TokenStream_SourceBytes(stream, span.sourceId, &sourceLength);
    rawBytes = ZrParser_TokenStream_TextBytes(stream, rawToken);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span) && sourceName != ZR_NULL &&
            source != ZR_NULL && rawBytes != ZR_NULL && nameLength == strlen(name) &&
            memcmp(sourceName, name, nameLength) == 0 && start < end && end <= sourceLength &&
            span.startOffset == start && span.endOffset == end &&
            span.startLine == 1 && span.endLine == 1 &&
            span.startColumn == (TZrInt32)start + 1 && span.endColumn == (TZrInt32)end + 1 &&
            rawToken.sourceId == span.sourceId && rawToken.offset == start && rawToken.length == end - start &&
            strlen(rawText) == end - start && memcmp(source + start, rawText, end - start) == 0 &&
            memcmp(rawBytes, rawText, end - start) == 0 &&
            ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(tree),
                    ZrParser_SyntaxContext_TableRoot(g_tokenSemanticRange.table)) &&
            ZrParser_TokenTree_NativeSemInfo(tree, &semInfo) && semInfo.token == kind,
            "Fixture prerequisite: semantic range native leaf raw text, Span, context or source identity changed");
    if (kind == ZR_TK_INTEGER) {
        TEST_ASSERT_EQUAL_INT64_MESSAGE(1, semInfo.intValue,
                "Fixture prerequisite: semantic range native integer value changed");
    }
}

static TZrBool semantic_range_position_equal(SZrFilePosition left, SZrFilePosition right) {
    return left.offset == right.offset && left.line == right.line && left.column == right.column;
}

static TZrBool semantic_range_physical_is(SZrFileRange range, const char *name,
        TZrSize start, TZrSize end) {
    return range.source != ZR_NULL && ZrCore_String_GetNativeString(range.source) != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(range.source), name) == 0 &&
            range.start.offset == start && range.end.offset == end &&
            range.start.line == 1 && range.end.line == 1 &&
            range.start.column == (TZrInt32)start + 1 && range.end.column == (TZrInt32)end + 1;
}

static TZrBool semantic_range_interval_is(SZrAstOwnership *owner, SZrFileRange range,
        TZrUInt32 first, TZrUInt32 last) {
    SZrParserRangeEvidenceDescription description;

    return ZrParser_RangeEvidence_Describe(owner, range, &description) &&
            description.kind == ZR_PARSER_RANGE_EVIDENCE_INTERVAL &&
            description.first == first && description.last == last;
}

static TZrBool semantic_range_preserved(SZrAstOwnership *owner, SZrFileRange actual,
        SZrFileRange expected) {
    SZrParserRangeEvidenceDescription actualDescription;
    SZrParserRangeEvidenceDescription expectedDescription;

    return ZrParser_RangeEvidence_Describe(owner, actual, &actualDescription) &&
            ZrParser_RangeEvidence_Describe(owner, expected, &expectedDescription) &&
            actualDescription.kind != ZR_PARSER_RANGE_EVIDENCE_NONE &&
            actualDescription.kind == expectedDescription.kind &&
            actualDescription.first == expectedDescription.first &&
            actualDescription.last == expectedDescription.last &&
            actual.originKey == expected.originKey && actual.ownerKey == expected.ownerKey &&
            actual.flags == expected.flags && semantic_range_position_equal(actual.start, expected.start) &&
            semantic_range_position_equal(actual.end, expected.end) && actual.source != ZR_NULL &&
            expected.source != ZR_NULL && ZrCore_String_Equal(actual.source, expected.source);
}

static void semantic_range_compiler(void) {
    ZrParser_CompilerState_Init(&g_tokenSemanticRange.compiler, g_state);
    g_tokenSemanticRange.compilerInitialized = ZR_TRUE;
    g_tokenSemanticRange.compiler.suppressErrorOutput = ZR_TRUE;
    g_tokenSemanticRange.compiler.scriptAst = g_tokenSemanticRange.ast;
    TEST_ASSERT_TRUE_MESSAGE(g_tokenSemanticRange.compiler.typeEnv != ZR_NULL &&
            g_tokenSemanticRange.compiler.semanticContext != ZR_NULL,
            "Fixture prerequisite: semantic range compiler environment allocation failed");
    ZrParser_InferredType_Init(g_state, &g_tokenSemanticRange.inferred, ZR_VALUE_TYPE_UNKNOWN);
    g_tokenSemanticRange.inferredInitialized = ZR_TRUE;
}

static TZrTypeId semantic_range_canonical_type(const char *expectedText) {
    TZrTypeId typeId = ZrParser_CanonicalType_FromInferred(
            g_tokenSemanticRange.compiler.semanticContext, &g_tokenSemanticRange.inferred);
    char buffer[96];

    TEST_ASSERT_TRUE_MESSAGE(typeId != ZR_SEMANTIC_ID_INVALID &&
            ZrParser_CanonicalType_Find(g_tokenSemanticRange.compiler.semanticContext, typeId) != ZR_NULL &&
            ZrParser_SemanticDisplay_FormatType(g_tokenSemanticRange.compiler.semanticContext,
                    typeId, buffer, sizeof(buffer)) && strcmp(buffer, expectedText) == 0,
            "Fixture prerequisite: semantic range canonical type identity or format changed");
    return typeId;
}

static const SZrSemanticTypeDisplayAliasFact *semantic_range_alias_fact(
        TZrTypeId typeId, const char *aliasText) {
    SZrArray *facts = &g_tokenSemanticRange.compiler.semanticContext->typeDisplayAliasFacts;
    const SZrSemanticTypeDisplayAliasFact *result = ZR_NULL;
    TZrSize matches = 0U;
    TZrSize index;

    TEST_ASSERT_TRUE_MESSAGE(facts->isValid && facts->head != ZR_NULL &&
            facts->elementSize == sizeof(SZrSemanticTypeDisplayAliasFact),
            "Fixture prerequisite: semantic range alias fact storage is unavailable");
    for (index = 0U; index < facts->length; ++index) {
        const SZrSemanticTypeDisplayAliasFact *fact =
                (const SZrSemanticTypeDisplayAliasFact *)ZrCore_Array_Get(facts, index);
        if (fact->typeId == typeId && fact->alias != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(fact->alias), aliasText) == 0) {
            result = fact;
            matches++;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(matches == 1U && result != ZR_NULL,
            "Fixture prerequisite: semantic range conversion must publish one matching alias fact");
    return result;
}

static SZrType *semantic_range_qualified_type(void) {
    SZrAstNode *declaration = g_tokenSemanticRange.ast->data.script.statements->nodes[0];
    SZrType *typeUse;

    TEST_ASSERT_TRUE_MESSAGE(declaration != ZR_NULL && declaration->type == ZR_AST_VARIABLE_DECLARATION &&
            declaration->data.variableDeclaration.pattern != ZR_NULL &&
            declaration->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
            declaration->data.variableDeclaration.value != ZR_NULL &&
            declaration->data.variableDeclaration.value->type == ZR_AST_NULL_LITERAL,
            "Fixture prerequisite: qualified alias declaration AST shape changed");
    typeUse = declaration->data.variableDeclaration.typeInfo;
    TEST_ASSERT_TRUE_MESSAGE(typeUse != ZR_NULL && typeUse->name != ZR_NULL &&
            typeUse->name->type == ZR_AST_IDENTIFIER_LITERAL && typeUse->subType != ZR_NULL &&
            typeUse->subType->name != ZR_NULL && typeUse->subType->name->type == ZR_AST_IDENTIFIER_LITERAL &&
            typeUse->subType->subType == ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(typeUse->name->data.identifier.name), "declaration") == 0 &&
            strcmp(ZrCore_String_GetNativeString(typeUse->subType->name->data.identifier.name), "Patch") == 0 &&
            typeUse->name->ownership == g_tokenSemanticRange.ast->ownership &&
            typeUse->subType->name->ownership == g_tokenSemanticRange.ast->ownership,
            "Fixture prerequisite: qualified alias names or AST ownership changed");
    return typeUse;
}

static TZrTypeId semantic_range_convert_qualified(SZrType *typeUse) {
    const SZrParserCompileToolModuleDescriptor *provider =
            ZrParser_CompileTool_FindModule("zr.compile.declaration");

    semantic_range_compiler();
    TEST_ASSERT_NOT_NULL_MESSAGE(provider,
            "Fixture prerequisite: qualified alias provider is unavailable");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_CompileToolBinding_DeclareProvider(&g_tokenSemanticRange.compiler,
            typeUse->name->data.identifier.name, provider) &&
            ZrParser_AstTypeToInferredType_Convert(&g_tokenSemanticRange.compiler,
                    typeUse, &g_tokenSemanticRange.inferred) &&
            !g_tokenSemanticRange.compiler.hasError,
            "Fixture prerequisite: qualified alias provider binding or conversion failed");
    return semantic_range_canonical_type("zr.compile.declaration.Patch");
}

static void semantic_range_qualified_text_control(void) {
    SZrType *typeUse;
    SZrFileRange expected;
    TZrTypeId typeId;
    SZrString *alias;
    const SZrSemanticTypeDisplayAliasFact *fact;

    semantic_range_parse_text("var patch: declaration.Patch = null;");
    typeUse = semantic_range_qualified_type();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_RangeEvidence_Merge(typeUse->name->ownership,
            typeUse->name->location, typeUse->subType->name->location, &expected),
            "Fixture prerequisite: ordinary qualified type range could not be joined");
    typeId = semantic_range_convert_qualified(typeUse);
    fact = semantic_range_alias_fact(typeId, "declaration.Patch");
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            g_tokenSemanticRange.compiler.semanticContext, typeId, &expected);
    TEST_ASSERT_TRUE_MESSAGE(semantic_range_preserved(typeUse->name->ownership, fact->useRange, expected) &&
            alias != ZR_NULL && strcmp(ZrCore_String_GetNativeString(alias), "declaration.Patch") == 0,
            "Fixture prerequisite: ordinary qualified type display alias control failed");
    token_source_semantic_range_cleanup();
}

static SZrAstNode *semantic_range_generic_type(SZrType **outTypeUse) {
    SZrAstNode *declaration = g_tokenSemanticRange.ast->data.script.statements->nodes[0];
    SZrAstNode *generic;

    TEST_ASSERT_TRUE_MESSAGE(declaration != ZR_NULL && declaration->type == ZR_AST_VARIABLE_DECLARATION &&
            declaration->data.variableDeclaration.typeInfo != ZR_NULL &&
            declaration->data.variableDeclaration.value != ZR_NULL &&
            declaration->data.variableDeclaration.value->type == ZR_AST_NULL_LITERAL,
            "Fixture prerequisite: generic alias variable type annotation or initializer is missing");
    *outTypeUse = declaration->data.variableDeclaration.typeInfo;
    generic = (*outTypeUse)->name;
    TEST_ASSERT_TRUE_MESSAGE(generic != ZR_NULL && generic->type == ZR_AST_GENERIC_TYPE &&
            generic->ownership == g_tokenSemanticRange.ast->ownership && (*outTypeUse)->subType == ZR_NULL &&
            generic->data.genericType.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(generic->data.genericType.name->name), "Box") == 0 &&
            generic->data.genericType.params != ZR_NULL && generic->data.genericType.params->count == 1U &&
            generic->data.genericType.params->nodes[0] != ZR_NULL &&
            generic->data.genericType.params->nodes[0]->type == ZR_AST_TYPE,
            "Fixture prerequisite: generic alias AST shape or ownership changed");
    return generic;
}

static TZrTypeId semantic_range_convert_generic(SZrType *typeUse) {
    semantic_range_compiler();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TypeEnvironment_RegisterType(g_state,
            g_tokenSemanticRange.compiler.typeEnv, typeUse->name->data.genericType.name->name) &&
            ZrParser_AstTypeToInferredType_Convert(&g_tokenSemanticRange.compiler,
                    typeUse, &g_tokenSemanticRange.inferred) && !g_tokenSemanticRange.compiler.hasError,
            "Fixture prerequisite: generic alias type registration or conversion failed");
    return semantic_range_canonical_type("Box<int>");
}

static void semantic_range_generic_text_control(void) {
    SZrType *typeUse = ZR_NULL;
    SZrAstNode *generic;
    SZrFileRange expected;
    TZrTypeId typeId;
    SZrString *alias;
    const SZrSemanticTypeDisplayAliasFact *fact;

    semantic_range_parse_text("var value: Box<i64> = null;");
    generic = semantic_range_generic_type(&typeUse);
    expected = generic->data.genericType.wholeRange;
    TEST_ASSERT_TRUE_MESSAGE(semantic_range_physical_is(expected, "semantic_plain.zr", 11U, 19U),
            "Fixture prerequisite: ordinary generic type whole range changed");
    typeId = semantic_range_convert_generic(typeUse);
    fact = semantic_range_alias_fact(typeId, "Box<i64>");
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            g_tokenSemanticRange.compiler.semanticContext, typeId, &expected);
    TEST_ASSERT_TRUE_MESSAGE(semantic_range_preserved(generic->ownership, fact->useRange, expected) &&
            alias != ZR_NULL && strcmp(ZrCore_String_GetNativeString(alias), "Box<i64>") == 0,
            "Fixture prerequisite: ordinary generic type display alias control failed");
    token_source_semantic_range_cleanup();
}

static void test_qualified_alias_preserves_mixed_type_use_evidence(void) {
    SZrTokenStream *prefix;
    SZrTokenStream *left;
    SZrTokenStream *right;
    SZrTokenStream *combined;
    SZrType *typeUse;
    SZrAstOwnership *owner;
    SZrFileRange expected;
    SZrFileRange outerBefore;
    SZrFileRange terminalBefore;
    const SZrSemanticTypeDisplayAliasFact *fact;
    const char *longName;
    TZrTypeId typeId;
    SZrString *alias;
    TZrBool proof;
    TZrBool query;
    TZrBool astUnchanged;
    TZrBool passed;

    semantic_range_qualified_text_control();
    semantic_range_table();
    longName = semantic_range_long_name();
    prefix = semantic_range_from_text("prefix.zr", "var patch: ");
    left = semantic_range_from_text(longName, "declaration.");
    right = semantic_range_from_text("terminal.zr", semantic_range_padded("Patch = null;"));
    combined = semantic_range_concat(semantic_range_concat(prefix, left), right);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(9U, ZrParser_TokenStream_Count(combined),
            "Fixture prerequisite: mixed qualified type token count changed");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 0U),
            ZR_TK_VAR, "prefix.zr", 0U, 3U, "var");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 1U),
            ZR_TK_IDENTIFIER, "prefix.zr", 4U, 9U, "patch");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 2U),
            ZR_TK_COLON, "prefix.zr", 9U, 10U, ":");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 3U),
            ZR_TK_IDENTIFIER, longName, 0U, 11U, "declaration");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 4U),
            ZR_TK_DOT, longName, 11U, 12U, ".");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 5U),
            ZR_TK_IDENTIFIER, "terminal.zr", 64U, 69U, "Patch");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 6U),
            ZR_TK_EQUALS, "terminal.zr", 70U, 71U, "=");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 7U),
            ZR_TK_NULL, "terminal.zr", 72U, 76U, "null");
    semantic_range_require_leaf(combined, ZrParser_TokenStream_At(combined, 8U),
            ZR_TK_SEMICOLON, "terminal.zr", 76U, 77U, ";");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_Span(ZrParser_TokenStream_At(combined, 3U)).sourceId !=
            ZrParser_TokenTree_Span(ZrParser_TokenStream_At(combined, 5U)).sourceId,
            "Fixture prerequisite: mixed qualified type physical source IDs must differ");
    semantic_range_parse_stream(combined);
    typeUse = semantic_range_qualified_type();
    owner = typeUse->name->ownership;
    TEST_ASSERT_TRUE_MESSAGE(semantic_range_physical_is(typeUse->name->location, longName, 0U, 11U) &&
            semantic_range_physical_is(typeUse->subType->name->location, "terminal.zr", 64U, 69U) &&
            semantic_range_interval_is(owner, typeUse->name->location, 4U, 4U) &&
            semantic_range_interval_is(owner, typeUse->subType->name->location, 6U, 6U) &&
            typeUse->name->location.originKey != typeUse->subType->name->location.originKey &&
            ZrCore_String_GetByteLength(typeUse->name->location.source) > ZR_VM_SHORT_STRING_MAX &&
            ZrParser_RangeEvidence_Merge(owner, typeUse->name->location,
                    typeUse->subType->name->location, &expected) &&
            semantic_range_interval_is(owner, expected, 4U, 6U) &&
            expected.flags == ZR_FILE_RANGE_FLAG_COMPOSITE &&
            semantic_range_physical_is(expected, longName, 0U, 11U),
            "Fixture prerequisite: mixed qualified type AST leaves or complete expected interval changed");
    outerBefore = typeUse->name->location;
    terminalBefore = typeUse->subType->name->location;
    typeId = semantic_range_convert_qualified(typeUse);
    fact = semantic_range_alias_fact(typeId, "declaration.Patch");
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            g_tokenSemanticRange.compiler.semanticContext, typeId, &expected);
    proof = semantic_range_preserved(owner, fact->useRange, expected);
    query = alias != ZR_NULL && strcmp(ZrCore_String_GetNativeString(alias), "declaration.Patch") == 0;
    astUnchanged = memcmp(&outerBefore, &typeUse->name->location, sizeof(outerBefore)) == 0 &&
            memcmp(&terminalBefore, &typeUse->subType->name->location, sizeof(terminalBefore)) == 0;
    passed = proof && query && astUnchanged;
    printf("Semantic qualified alias proof=%u query=%u ast=%u range=%llu..%llu owner=%llu index=%u flags=%u sourceSame=%u\n",
            (unsigned)proof, (unsigned)query, (unsigned)astUnchanged,
            (unsigned long long)fact->useRange.start.offset, (unsigned long long)fact->useRange.end.offset,
            (unsigned long long)fact->useRange.ownerKey, (unsigned)fact->useRange.evidenceIndex,
            (unsigned)fact->useRange.flags, (unsigned)(fact->useRange.source == expected.source));
    token_source_semantic_range_cleanup();
    if (!passed) {
        TEST_FAIL_MESSAGE("Qualified type display aliases must preserve the complete native interval and first real anchor");
    }
}

static void test_generic_alias_preserves_long_source_evidence(void) {
    static const char source[] = "var value: Box<i64> = null;";
    SZrTokenStream *stream;
    SZrType *typeUse = ZR_NULL;
    SZrAstNode *generic;
    SZrFileRange expected;
    const SZrSemanticTypeDisplayAliasFact *fact;
    const char *longName;
    SZrString *alias;
    TZrTypeId typeId;
    TZrBool proof;
    TZrBool query;
    TZrBool astUnchanged;
    TZrBool passed;

    semantic_range_generic_text_control();
    semantic_range_table();
    longName = semantic_range_long_name();
    stream = semantic_range_from_text(longName, source);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(10U, ZrParser_TokenStream_Count(stream),
            "Fixture prerequisite: generic alias native token count changed");
    semantic_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 3U),
            ZR_TK_IDENTIFIER, longName, 11U, 14U, "Box");
    semantic_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 4U),
            ZR_TK_LESS_THAN, longName, 14U, 15U, "<");
    semantic_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 5U),
            ZR_TK_IDENTIFIER, longName, 15U, 18U, "i64");
    semantic_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 6U),
            ZR_TK_GREATER_THAN, longName, 18U, 19U, ">");
    semantic_range_parse_stream(stream);
    generic = semantic_range_generic_type(&typeUse);
    expected = generic->data.genericType.wholeRange;
    TEST_ASSERT_TRUE_MESSAGE(semantic_range_interval_is(generic->ownership, expected, 4U, 7U) &&
            semantic_range_physical_is(expected, longName, 11U, 19U) && expected.flags == 0U &&
            ZrCore_String_GetByteLength(expected.source) > ZR_VM_SHORT_STRING_MAX,
            "Fixture prerequisite: generic alias must start with an unchanged complete long-name interval");
    typeId = semantic_range_convert_generic(typeUse);
    fact = semantic_range_alias_fact(typeId, "Box<i64>");
    alias = ZrParser_SemanticQuery_TypeDisplayAliasAt(
            g_tokenSemanticRange.compiler.semanticContext, typeId, &expected);
    proof = semantic_range_preserved(generic->ownership, fact->useRange, expected);
    query = alias != ZR_NULL && strcmp(ZrCore_String_GetNativeString(alias), "Box<i64>") == 0;
    astUnchanged = memcmp(&expected, &generic->data.genericType.wholeRange, sizeof(expected)) == 0;
    passed = proof && query && astUnchanged;
    printf("Semantic long-name alias proof=%u query=%u ast=%u range=%llu..%llu owner=%llu index=%u flags=%u sourceSame=%u\n",
            (unsigned)proof, (unsigned)query, (unsigned)astUnchanged,
            (unsigned long long)fact->useRange.start.offset, (unsigned long long)fact->useRange.end.offset,
            (unsigned long long)fact->useRange.ownerKey, (unsigned)fact->useRange.evidenceIndex,
            (unsigned)fact->useRange.flags, (unsigned)(fact->useRange.source == expected.source));
    token_source_semantic_range_cleanup();
    if (!passed) {
        TEST_FAIL_MESSAGE("Type display aliases must retain valid complete evidence for long source names");
    }
}

static SZrTokenSpan semantic_range_span(TZrTokenSourceId sourceId, TZrSize start, TZrSize end) {
    SZrTokenSpan span;

    span.sourceId = sourceId;
    span.startOffset = start;
    span.endOffset = end;
    span.startLine = 1;
    span.endLine = 1;
    span.startColumn = (TZrInt32)start + 1;
    span.endColumn = (TZrInt32)end + 1;
    return span;
}

static void semantic_range_require_span(SZrTokenStream *stream, SZrTokenSpan span,
        const char *name, TZrSize start, TZrSize end, const char *text) {
    TZrSize nameLength = 0U;
    TZrSize sourceLength = 0U;
    const char *sourceName = ZrParser_TokenStream_SourceName(stream, span.sourceId, &nameLength);
    const char *source = ZrParser_TokenStream_SourceBytes(stream, span.sourceId, &sourceLength);

    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenSpan_IsValid(span) && sourceName != ZR_NULL &&
            source != ZR_NULL && nameLength == strlen(name) && memcmp(sourceName, name, nameLength) == 0 &&
            start < end && end <= sourceLength && span.startOffset == start && span.endOffset == end &&
            span.startLine == 1 && span.endLine == 1 &&
            span.startColumn == (TZrInt32)start + 1 && span.endColumn == (TZrInt32)end + 1 &&
            strlen(text) == end - start && memcmp(source + start, text, end - start) == 0,
            "Fixture prerequisite: semantic call original delimiter or group Span, raw bytes or source changed");
}

static SZrTokenStream *semantic_range_foreign_argument_stream(void) {
    SZrTokenStream *children = semantic_range_from_text("argument.zr", semantic_range_padded("1"));
    SZrTokenStream **slot = semantic_range_stream_slot();
    SZrTokenStream *stream;
    TZrTokenSourceId sourceId = ZR_TOKEN_SOURCE_INVALID;
    SZrTokenNativeSemInfo semInfo = {0};
    SZrTokenText raw;
    const SZrTokenTree *child;
    TZrSize index = 0U;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1U, ZrParser_TokenStream_Count(children),
            "Fixture prerequisite: foreign call argument stream must have one literal");
    child = ZrParser_TokenStream_At(children, 0U);
    semantic_range_require_leaf(children, child, ZR_TK_INTEGER, "argument.zr", 64U, 65U, "1");
    *slot = ZrParser_TokenStream_Create(g_state->global, g_tokenSemanticRange.table, ZR_NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(*slot,
            "Fixture prerequisite: foreign call argument builder allocation failed");
    stream = *slot;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AddSource(stream, "callee.zr", 9U,
            "target();", 9U, &sourceId),
            "Fixture prerequisite: foreign call argument callee source allocation failed");
    raw.sourceId = sourceId;
    raw.offset = 0U;
    raw.length = 6U;
    semInfo.token = ZR_TK_IDENTIFIER;
    semInfo.text = raw;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AppendIdent(stream, semInfo, raw,
            semantic_range_span(sourceId, 0U, 6U), ZrParser_SyntaxContext_TableRoot(g_tokenSemanticRange.table),
            ZR_NULL, 0U, ZR_NULL, 0U, &index),
            "Fixture prerequisite: foreign call argument callee append failed");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AppendGroup(stream, ZR_TOKEN_DELIMITER_PAREN,
            semantic_range_span(sourceId, 6U, 7U), semantic_range_span(sourceId, 7U, 8U),
            &child, 1U, semantic_range_span(sourceId, 6U, 8U),
            ZrParser_SyntaxContext_TableRoot(g_tokenSemanticRange.table),
            ZR_NULL, 0U, ZR_NULL, 0U, &index),
            "Fixture prerequisite: foreign call argument group import failed");
    memset(&semInfo, 0, sizeof(semInfo));
    semInfo.token = ZR_TK_SEMICOLON;
    raw.offset = 8U;
    raw.length = 1U;
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenStream_AppendPunct(stream, semInfo, raw, ';',
            ZR_TOKEN_SPACING_ALONE, semantic_range_span(sourceId, 8U, 9U),
            ZrParser_SyntaxContext_TableRoot(g_tokenSemanticRange.table),
            ZR_NULL, 0U, ZR_NULL, 0U, &index) && ZrParser_TokenStream_Seal(stream),
            "Fixture prerequisite: foreign call argument stream completion failed");
    return stream;
}

static void semantic_range_require_call_input(SZrTokenStream *stream, TZrSize variant) {
    const SZrTokenTree *target;
    const SZrTokenTree *group;
    SZrTokenSpan open;
    SZrTokenSpan close;
    SZrTokenSpan whole;
    TZrSize callStart = variant == 1U ? 64U : 6U;
    TZrSize callEnd = variant == 1U ? 67U : (variant == 2U ? 8U : 9U);
    TZrSize argumentStart = variant == 1U ? 65U : (variant == 2U ? 64U : 7U);
    const char *callSource = variant == 1U ? "call.zr" : "callee.zr";
    const char *argumentSource = variant == 2U ? "argument.zr" : callSource;

    TEST_ASSERT_EQUAL_UINT64_MESSAGE(3U, ZrParser_TokenStream_Count(stream),
            "Fixture prerequisite: semantic call input must contain callee, group and semicolon");
    target = ZrParser_TokenStream_At(stream, 0U);
    group = ZrParser_TokenStream_At(stream, 1U);
    semantic_range_require_leaf(stream, target, ZR_TK_IDENTIFIER, "callee.zr", 0U, 6U, "target");
    TEST_ASSERT_TRUE_MESSAGE(group != ZR_NULL && ZrParser_TokenTree_Kind(group) == ZR_TOKEN_TREE_GROUP &&
            ZrParser_TokenTree_GroupDelimiter(group) == ZR_TOKEN_DELIMITER_PAREN &&
            ZrParser_TokenTree_GroupChildCount(group) == 1U &&
            ZrParser_SyntaxContext_Equal(ZrParser_TokenTree_Context(group),
                    ZrParser_SyntaxContext_TableRoot(g_tokenSemanticRange.table)),
            "Fixture prerequisite: semantic call input group shape or context changed");
    open = ZrParser_TokenTree_GroupOpenSpan(group);
    close = ZrParser_TokenTree_GroupCloseSpan(group);
    whole = ZrParser_TokenTree_Span(group);
    semantic_range_require_span(stream, open, callSource, callStart, callStart + 1U, "(");
    semantic_range_require_span(stream, close, callSource, callEnd - 1U, callEnd, ")");
    semantic_range_require_span(stream, whole, callSource, callStart, callEnd,
            variant == 2U ? "()" : "(1)");
    TEST_ASSERT_TRUE_MESSAGE(open.sourceId == close.sourceId && open.sourceId == whole.sourceId &&
            ((ZrParser_TokenTree_Span(target).sourceId == open.sourceId) == (variant != 1U)) &&
            ((ZrParser_TokenTree_Span(ZrParser_TokenTree_GroupChildAt(group, 0U)).sourceId ==
                    open.sourceId) == (variant != 2U)),
            "Fixture prerequisite: semantic call delimiter and argument physical source identities changed");
    semantic_range_require_leaf(stream, ZrParser_TokenTree_GroupChildAt(group, 0U),
            ZR_TK_INTEGER, argumentSource, argumentStart, argumentStart + 1U, "1");
    semantic_range_require_leaf(stream, ZrParser_TokenStream_At(stream, 2U),
            ZR_TK_SEMICOLON, callSource, callEnd, callEnd + 1U, ";");
    if (variant != 0U) {
        TEST_ASSERT_TRUE_MESSAGE(ZrParser_TokenTree_Span(target).sourceId !=
                ZrParser_TokenTree_Span(ZrParser_TokenTree_GroupChildAt(group, 0U)).sourceId,
                "Fixture prerequisite: semantic call mixed argument must have a distinct physical source ID");
    }
}

static SZrAstNode *semantic_range_primary(SZrAstNode **outCall, SZrAstNode **outArgument) {
    SZrAstNode *statement = g_tokenSemanticRange.ast->data.script.statements->nodes[0];
    SZrAstNode *primary;
    SZrAstNode *call;

    TEST_ASSERT_TRUE_MESSAGE(statement != ZR_NULL && statement->type == ZR_AST_EXPRESSION_STATEMENT,
            "Fixture prerequisite: semantic call expression statement is missing");
    primary = statement->data.expressionStatement.expr;
    TEST_ASSERT_TRUE_MESSAGE(primary != ZR_NULL && primary->type == ZR_AST_PRIMARY_EXPRESSION &&
            primary->data.primaryExpression.property != ZR_NULL &&
            primary->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
            primary->data.primaryExpression.property->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(primary->data.primaryExpression.property->data.identifier.name),
                    "target") == 0 &&
            primary->data.primaryExpression.members != ZR_NULL &&
            primary->data.primaryExpression.members->count == 1U,
            "Fixture prerequisite: semantic call PRIMARY shape changed");
    call = primary->data.primaryExpression.members->nodes[0];
    TEST_ASSERT_TRUE_MESSAGE(call != ZR_NULL && call->type == ZR_AST_FUNCTION_CALL &&
            call->data.functionCall.args != ZR_NULL && call->data.functionCall.args->count == 1U &&
            call->data.functionCall.args->nodes[0] != ZR_NULL &&
            call->data.functionCall.args->nodes[0]->type == ZR_AST_INTEGER_LITERAL &&
            call->data.functionCall.args->nodes[0]->data.integerLiteral.value == 1 &&
            primary->ownership == g_tokenSemanticRange.ast->ownership && call->ownership == primary->ownership &&
            primary->data.primaryExpression.property->ownership == primary->ownership &&
            call->data.functionCall.args->nodes[0]->ownership == primary->ownership,
            "Fixture prerequisite: semantic call AST member, argument value or ownership changed");
    *outCall = call;
    *outArgument = call->data.functionCall.args->nodes[0];
    return primary;
}

static void semantic_range_register_target(void) {
    SZrString *name;

    semantic_range_compiler();
    ZrParser_InferredType_Init(g_state, &g_tokenSemanticRange.returnType, ZR_VALUE_TYPE_INT64);
    ZrParser_InferredType_Init(g_state, &g_tokenSemanticRange.parameterType, ZR_VALUE_TYPE_INT64);
    g_tokenSemanticRange.callableTypesInitialized = ZR_TRUE;
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_Array_TryInit(g_state, &g_tokenSemanticRange.parameterTypes,
            sizeof(SZrInferredType), 1U),
            "Fixture prerequisite: semantic call parameter type storage allocation failed");
    ZrCore_Array_Push(g_state, &g_tokenSemanticRange.parameterTypes, &g_tokenSemanticRange.parameterType);
    name = ZrCore_String_CreateFromNative(g_state, "target");
    TEST_ASSERT_TRUE_MESSAGE(name != ZR_NULL && ZrParser_TypeEnvironment_RegisterFunction(g_state,
            g_tokenSemanticRange.compiler.typeEnv, name, &g_tokenSemanticRange.returnType,
            &g_tokenSemanticRange.parameterTypes),
            "Fixture prerequisite: semantic call target registration failed");
}

static TZrBool semantic_range_primary_case(TZrSize variant) {
    SZrTokenStream *stream;
    SZrAstNode *primary;
    SZrAstNode *call = ZR_NULL;
    SZrAstNode *argument = ZR_NULL;
    SZrAstNode *target;
    SZrFileRange expected;
    const SZrSemanticExpressionFact *fact;
    TZrTypeId typeId;
    TZrBool proof;
    TZrBool astUnchanged;
    TZrBool passed;
    TZrSize callStart = variant == 1U ? 64U : 6U;
    TZrSize callEnd = variant == 1U ? 67U : (variant == 2U ? 7U : 9U);
    TZrSize argumentStart = variant == 1U ? 65U : (variant == 2U ? 64U : 7U);
    const char *callSource = variant == 1U ? "call.zr" : "callee.zr";
    const char *argumentSource = variant == 2U ? "argument.zr" : callSource;

    semantic_range_table();
    if (variant == 0U) {
        stream = semantic_range_from_text("callee.zr", "target(1);");
    } else if (variant == 1U) {
        SZrTokenStream *left = semantic_range_from_text("callee.zr", "target");
        SZrTokenStream *right = semantic_range_from_text("call.zr", semantic_range_padded("(1);"));
        stream = semantic_range_concat(left, right);
    } else {
        stream = semantic_range_foreign_argument_stream();
    }
    semantic_range_require_call_input(stream, variant);
    semantic_range_parse_stream(stream);
    semantic_range_require_call_input(stream, variant);
    primary = semantic_range_primary(&call, &argument);
    target = primary->data.primaryExpression.property;
    expected = primary->location;
    TEST_ASSERT_TRUE_MESSAGE(semantic_range_physical_is(target->location, "callee.zr", 0U, 6U) &&
            semantic_range_physical_is(call->location, callSource, callStart, callEnd) &&
            semantic_range_physical_is(argument->location, argumentSource, argumentStart, argumentStart + 1U) &&
            semantic_range_interval_is(primary->ownership, target->location, 1U, 1U) &&
            semantic_range_interval_is(primary->ownership, call->location, 2U, 4U) &&
            semantic_range_interval_is(primary->ownership, argument->location, 3U, 3U) &&
            semantic_range_interval_is(primary->ownership, expected, 1U, 4U) &&
            target->location.originKey != 0U && call->location.originKey != 0U &&
            argument->location.originKey != 0U &&
            ((target->location.originKey == argument->location.originKey) == (variant == 0U)) &&
            ((target->location.originKey == call->location.originKey) == (variant != 1U)) &&
            semantic_range_physical_is(expected, "callee.zr", 0U, variant == 0U ? 9U : 6U) &&
            expected.flags == (variant == 0U ? 0U : ZR_FILE_RANGE_FLAG_COMPOSITE),
            "Fixture prerequisite: semantic call parsed ranges or complete parent interval changed");
    semantic_range_register_target();
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExpressionType_Infer(&g_tokenSemanticRange.compiler,
            primary, &g_tokenSemanticRange.inferred) && !g_tokenSemanticRange.compiler.hasError &&
            g_tokenSemanticRange.inferred.baseType == ZR_VALUE_TYPE_INT64,
            "Fixture prerequisite: semantic call public type inference failed");
    typeId = semantic_range_canonical_type("int");
    fact = ZrParser_SemanticFacts_FindExpressionByNode(
            g_tokenSemanticRange.compiler.semanticContext, primary);
    TEST_ASSERT_TRUE_MESSAGE(fact != ZR_NULL && fact->node == primary &&
            fact->kind == ZR_SEMANTIC_EXPRESSION_FACT_MEMBER && fact->exactness == ZR_SEMANTIC_FACT_EXACT &&
            fact->inferredType.baseType == ZR_VALUE_TYPE_INT64 && fact->typeId == typeId &&
            fact->hasCallInfo && fact->callTargetName != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(fact->callTargetName), "target") == 0 &&
            fact->argumentCount == 1U && !fact->hasNamedArguments && !fact->isMemberCall &&
            semantic_range_preserved(primary->ownership, fact->callTargetRange, target->location),
            "Fixture prerequisite: semantic call fact must retain canonical type and resolved call payload");
    proof = semantic_range_preserved(primary->ownership, fact->range, expected);
    astUnchanged = memcmp(&expected, &primary->location, sizeof(expected)) == 0;
    passed = proof && astUnchanged;
    printf("Semantic PRIMARY variant=%u proof=%u ast=%u range=%llu..%llu parent=%llu..%llu owner=%llu index=%u flags=%u type=%u call=%u\n",
            (unsigned)variant, (unsigned)proof, (unsigned)astUnchanged,
            (unsigned long long)fact->range.start.offset, (unsigned long long)fact->range.end.offset,
            (unsigned long long)expected.start.offset, (unsigned long long)expected.end.offset,
            (unsigned long long)fact->range.ownerKey, (unsigned)fact->range.evidenceIndex,
            (unsigned)fact->range.flags, (unsigned)fact->typeId, (unsigned)fact->hasCallInfo);
    token_source_semantic_range_cleanup();
    return passed;
}

static void test_primary_facts_preserve_mixed_call_evidence(void) {
    TZrBool callRange;
    TZrBool argumentRange;

    TEST_ASSERT_TRUE_MESSAGE(semantic_range_primary_case(0U),
            "Fixture prerequisite: ordinary single-source PRIMARY semantic fact control failed");
    callRange = semantic_range_primary_case(1U);
    argumentRange = semantic_range_primary_case(2U);
    if (!callRange || !argumentRange) {
        TEST_FAIL_MESSAGE("PRIMARY expression facts must retain the complete parsed range without rewriting foreign endpoints");
    }
}

#endif
