#ifndef ZR_TEST_SEMANTIC_ANALYZER_DECLARATION_CONTEXT_CASES_H
#define ZR_TEST_SEMANTIC_ANALYZER_DECLARATION_CONTEXT_CASES_H

#include "semantic/analyzer/semantic_analyzer_internal.h"
#include "semantic/type_declaration_context.h"
#include "type_inference/type_environment_declaration_binding.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/syntax_context.h"

typedef struct SZrTestDeclarationContextFixture {
    SZrSemanticAnalyzer *analyzer;
    SZrAstNode *ast;
    SZrSyntaxContextTable *table;
    SZrSyntaxContext marked;
    SZrSyntaxContext mixed;
    SZrSyntaxContext context;
} SZrTestDeclarationContextFixture;

typedef struct SZrTestDeclarationContextSnapshot {
    TZrSize symbols, types, canonical, definitions, overloads, members;
    TZrSize variables, named, names;
    TZrTypeId nextType;
    TZrSymbolId nextSymbol;
    TZrOverloadSetId nextOverload;
} SZrTestDeclarationContextSnapshot;

static void declaration_context_fixture_free(
        SZrState *state, SZrTestDeclarationContextFixture *fixture) {
    ZrLanguageServer_SemanticAnalyzer_Free(state, fixture->analyzer);
    if (fixture->ast != ZR_NULL) {
        ZrParser_Ast_Free(state, fixture->ast);
    }
    /* The AST and semantic snapshot may both borrow the mark table. */
    ZrParser_SyntaxContextTable_Release(fixture->table);
}

static TZrBool declaration_context_fixture_init(
        SZrState *state, SZrTestDeclarationContextFixture *fixture,
        const char *source, TZrBool marked) {
    SZrSyntaxExpansionInfo expansion = {0};
    TZrSyntaxExpansionId expansionId;
    SZrString *sourceName;

    memset(fixture, 0, sizeof(*fixture));
    sourceName = ZrCore_String_CreateFromNative(state, "lsp_declaration_context.zr");
    fixture->analyzer = ZrLanguageServer_SemanticAnalyzer_New(state);
    fixture->table = ZrParser_SyntaxContextTable_Create(state->global, ZR_NULL);
    if (sourceName == ZR_NULL || fixture->analyzer == ZR_NULL || fixture->table == ZR_NULL) {
        return ZR_FALSE;
    }
    fixture->ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (fixture->ast == ZR_NULL || fixture->ast->type != ZR_AST_SCRIPT ||
        fixture->ast->data.script.statements == ZR_NULL ||
        fixture->ast->data.script.statements->count == 0U) {
        return ZR_FALSE;
    }
    expansion.stableIdentity = 0x4D314443U;
    expansion.callSiteSpan = ZrParser_TokenSpan_None();
    expansion.definitionSiteSpan = ZrParser_TokenSpan_None();
    if (!ZrParser_SyntaxContextTable_RegisterExpansion(
                fixture->table, &expansion, &expansionId)) {
        return ZR_FALSE;
    }
    fixture->marked = ZrParser_SyntaxContext_DefSite(fixture->table, expansionId);
    fixture->mixed = ZrParser_SyntaxContext_MixedSite(fixture->table, expansionId);
    fixture->context = marked ? fixture->marked : ZrParser_SyntaxContext_Root();
    return ZrParser_SyntaxContext_IsValid(fixture->marked) &&
           ZrParser_SyntaxContext_IsValid(fixture->mixed) &&
           ZrParser_SyntaxContext_Equal(
                   ZrParser_SyntaxContext_Normalize(fixture->mixed, ZR_SYNTAX_NAMESPACE_ITEM),
                   ZrParser_SyntaxContext_Root()) &&
           !ZrParser_SyntaxContext_Equal(
                   ZrParser_SyntaxContext_Normalize(fixture->marked, ZR_SYNTAX_NAMESPACE_ITEM),
                   ZrParser_SyntaxContext_Root()) &&
           !ZrParser_SyntaxContext_Equal(
                   ZrParser_SyntaxContext_Normalize(fixture->marked, ZR_SYNTAX_NAMESPACE_LOCAL),
                   ZrParser_SyntaxContext_Root());
}

static SZrAstNode *declaration_context_node(
        SZrTestDeclarationContextFixture *fixture, TZrSize index) {
    SZrAstNodeArray *statements = fixture->ast->data.script.statements;
    SZrAstNode *node = index < statements->count ? statements->nodes[index] : ZR_NULL;
    if (node != ZR_NULL && node->type == ZR_AST_EXTERN_BLOCK) {
        SZrAstNodeArray *declarations = node->data.externBlock.declarations;
        return declarations != ZR_NULL && declarations->count == 1U
                       ? declarations->nodes[0] : ZR_NULL;
    }
    return node;
}

static SZrIdentifier *declaration_context_identifier(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }
    switch (node->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            return &node->data.identifier;
        case ZR_AST_VARIABLE_DECLARATION:
            return declaration_context_identifier(node->data.variableDeclaration.pattern);
        case ZR_AST_PARAMETER:
            return node->data.parameter.name;
        case ZR_AST_FUNCTION_DECLARATION:
            return node->data.functionDeclaration.name;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return node->data.externFunctionDeclaration.name;
        default:
            return (SZrIdentifier *)type_declaration_identifier(node);
    }
}

static TZrBool declaration_context_prepare(
        SZrState *state, SZrTestDeclarationContextFixture *fixture) {
    SZrCompilerState *compiler;
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(state, fixture->analyzer, fixture->ast)) {
        return ZR_FALSE;
    }
    compiler = fixture->analyzer->compilerState;
    return compiler != ZR_NULL && compiler->state == state && compiler->typeEnv != ZR_NULL &&
           fixture->analyzer->semanticContext != ZR_NULL &&
           compiler->semanticContext == fixture->analyzer->semanticContext &&
           compiler->typeEnv->semanticContext == fixture->analyzer->semanticContext;
}

static SZrTestDeclarationContextSnapshot declaration_context_snapshot(
        SZrTestDeclarationContextFixture *fixture) {
    SZrTestDeclarationContextSnapshot snapshot = {0};
    SZrSemanticContext *semantic = fixture->analyzer->semanticContext;
    SZrTypeEnvironment *env = fixture->analyzer->compilerState->typeEnv;
    snapshot.symbols = semantic->symbols.length;
    snapshot.types = semantic->types.length;
    snapshot.canonical = semantic->canonicalTypes.length;
    snapshot.definitions = semantic->canonicalTypeDefinitions.length;
    snapshot.overloads = semantic->overloadSets.length;
    for (TZrSize index = 0U; index < semantic->overloadSets.length; index++) {
        const SZrSemanticOverloadSetRecord *record = ZrCore_Array_Get(&semantic->overloadSets, index);
        if (record != ZR_NULL) {
            snapshot.members += record->members.length;
        }
    }
    snapshot.variables = env->variableTypes.length;
    snapshot.named = env->namedTypeBindings.length;
    snapshot.names = env->typeNames.length;
    snapshot.nextType = semantic->nextTypeId;
    snapshot.nextSymbol = semantic->nextSymbolId;
    snapshot.nextOverload = semantic->nextOverloadSetId;
    return snapshot;
}

static TZrBool declaration_context_unchanged(
        SZrTestDeclarationContextSnapshot before, SZrTestDeclarationContextSnapshot after) {
    return before.symbols == after.symbols && before.types == after.types &&
           before.canonical == after.canonical && before.definitions == after.definitions &&
           before.overloads == after.overloads && before.members == after.members &&
           before.variables == after.variables && before.named == after.named &&
           before.names == after.names && before.nextType == after.nextType &&
           before.nextSymbol == after.nextSymbol && before.nextOverload == after.nextOverload;
}

static TZrBool declaration_context_equal(
        SZrSyntaxContext left, SZrSyntaxContext right, EZrSemanticSymbolKind kind) {
    EZrSyntaxNamespace space = kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
                                      kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION
                              ? ZR_SYNTAX_NAMESPACE_ITEM : ZR_SYNTAX_NAMESPACE_LOCAL;
    return ZrParser_SyntaxContext_IsValid(left) && ZrParser_SyntaxContext_IsValid(right) &&
           ZrParser_SyntaxContext_Equal(ZrParser_SyntaxContext_Normalize(left, space),
                                       ZrParser_SyntaxContext_Normalize(right, space));
}

static TZrBool declaration_context_record(
        SZrTestDeclarationContextFixture *fixture, SZrAstNode *node,
        EZrSemanticSymbolKind kind, SZrSyntaxContext context,
        const SZrSemanticSymbolRecord **outRecord) {
    SZrIdentifier *identifier = declaration_context_identifier(node);
    if (outRecord == ZR_NULL) {
        return ZR_FALSE;
    }
    *outRecord = ZR_NULL;
    if (identifier == ZR_NULL || identifier->name == ZR_NULL) {
        return ZR_FALSE;
    }
    return type_environment_find_declaration_symbol(
            fixture->analyzer->semanticContext, node, identifier->name, kind, context, outRecord);
}

static const SZrSemanticTypeRecord *declaration_context_type(
        SZrTestDeclarationContextFixture *fixture, TZrTypeId typeId) {
    SZrArray *types = &fixture->analyzer->semanticContext->types;
    for (TZrSize index = 0U; index < types->length; index++) {
        const SZrSemanticTypeRecord *record = ZrCore_Array_Get(types, index);
        if (record != ZR_NULL && record->id == typeId) {
            return record;
        }
    }
    return ZR_NULL;
}

static TZrBool declaration_context_overload_contains(
        SZrSemanticContext *semantic, const char *name,
        TZrOverloadSetId overloadId, TZrSymbolId symbolId) {
    const SZrSemanticOverloadSetRecord *overload = find_overload_set_record(semantic, name);
    if (overload == ZR_NULL || overloadId == ZR_SEMANTIC_ID_INVALID || overload->id != overloadId) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < overload->members.length; index++) {
        const TZrSymbolId *member = ZrCore_Array_Get((SZrArray *)&overload->members, index);
        if (member != ZR_NULL && *member == symbolId) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static SZrSymbol *declaration_context_symbol(
        SZrTestDeclarationContextFixture *fixture, SZrAstNode *node) {
    SZrArray *scopes = &fixture->analyzer->symbolTable->allScopes;
    for (TZrSize scopeIndex = 0U; scopeIndex < scopes->length; scopeIndex++) {
        SZrSymbolScope **scope = ZrCore_Array_Get(scopes, scopeIndex);
        if (scope == ZR_NULL || *scope == ZR_NULL) {
            continue;
        }
        for (TZrSize index = 0U; index < (*scope)->symbols.length; index++) {
            SZrSymbol **symbol = ZrCore_Array_Get(&(*scope)->symbols, index);
            if (symbol != ZR_NULL && *symbol != ZR_NULL && (*symbol)->astNode == node) {
                return *symbol;
            }
        }
    }
    return ZR_NULL;
}

static SZrSymbol *declaration_context_add_symbol(
        SZrState *state, SZrTestDeclarationContextFixture *fixture, SZrAstNode *node,
        EZrSemanticSymbolKind kind, const SZrInferredType *type) {
    SZrSymbol *symbol = ZR_NULL;
    SZrIdentifier *identifier = declaration_context_identifier(node);
    EZrSymbolType displayKind = kind == ZR_SEMANTIC_SYMBOL_KIND_PARAMETER ? ZR_SYMBOL_PARAMETER :
                               kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ? ZR_SYMBOL_FUNCTION :
                               kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE ? ZR_SYMBOL_STRUCT : ZR_SYMBOL_VARIABLE;
    if (identifier == ZR_NULL || identifier->name == ZR_NULL ||
        !ZrLanguageServer_SymbolTable_AddSymbolEx(
                state, fixture->analyzer->symbolTable, displayKind, identifier->name,
                node->location, (SZrInferredType *)type, ZR_ACCESS_PUBLIC, node, &symbol)) {
        return ZR_NULL;
    }
    return symbol;
}

static TZrBool declaration_context_text(SZrString *value, const char *expected) {
    const char *text = value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
    return text != ZR_NULL && strcmp(text, expected) == 0;
}

static TZrBool declaration_context_range(SZrFileRange left, SZrFileRange right) {
    return left.source == right.source && left.start.line == right.start.line &&
           left.start.column == right.start.column && left.start.offset == right.start.offset &&
           left.end.line == right.end.line && left.end.column == right.end.column &&
           left.end.offset == right.end.offset;
}

static TZrBool declaration_context_duplicate(
        SZrCompilerState *compiler, SZrFileRange current, SZrFileRange previous) {
    const SZrStructuredDiagnostic *diagnostic = &compiler->structuredError;
    const char *message = diagnostic->message != ZR_NULL
                                  ? ZrCore_String_GetNativeString(diagnostic->message) : ZR_NULL;
    const SZrStructuredDiagnosticRelatedInformation *related =
            diagnostic->relatedInformation.length == 1U
                    ? ZrCore_Array_Get(&compiler->structuredError.relatedInformation, 0U) : ZR_NULL;
    return compiler->hasError && compiler->hasStructuredError &&
           declaration_context_text(diagnostic->code, "duplicate_type") &&
           message != ZR_NULL && strstr(message, "already declared in this context") != ZR_NULL &&
           diagnostic->noFixReason == ZR_DIAGNOSTIC_NO_FIX_REASON_REQUIRES_USER_DECISION &&
           diagnostic->fixes.length == 0U && declaration_context_range(diagnostic->location, current) &&
           related != ZR_NULL && declaration_context_range(related->location, previous);
}

static void declaration_context_clear_error(SZrCompilerState *compiler) {
    ZrParser_Compiler_ClearStructuredError(compiler);
    compiler->hasError = ZR_FALSE;
    compiler->errorMessage = ZR_NULL;
}

static const char *const declaration_context_nominal_sources[] = {
    "class ContextClass {}",
    "struct ContextStruct {}",
    "enum ContextEnum { First; }",
    "union ContextUnion { Empty; }",
    "interface ContextInterface {}",
    "native extern(\"fixture\") { delegate ContextDelegate(value:i32):void; }"
};

/* Negative results describe fixture failures; zero describes the behavior under test. */
static int declaration_context_unowned_local(SZrState *state, TZrBool parameter) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize marked = 0U; marked < 2U; marked++) {
        SZrTestDeclarationContextFixture fixture;
        SZrAstNode *node;
        SZrIdentifier *identifier;
        SZrSymbol *symbol;
        const SZrSemanticSymbolRecord *record;
        EZrSemanticSymbolKind kind = parameter ? ZR_SEMANTIC_SYMBOL_KIND_PARAMETER : ZR_SEMANTIC_SYMBOL_KIND_VARIABLE;
        const char *source = parameter ? "fn contextEcho(value:int):int { return value; }" : "var contextValue:int=1;";
        if (!declaration_context_fixture_init(state, &fixture, source, marked != 0U)) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        node = declaration_context_node(&fixture, 0U);
        if (parameter && node != ZR_NULL && node->type == ZR_AST_FUNCTION_DECLARATION &&
            node->data.functionDeclaration.params != ZR_NULL && node->data.functionDeclaration.params->count == 1U) {
            node = node->data.functionDeclaration.params->nodes[0];
        }
        identifier = declaration_context_identifier(node);
        if (node == ZR_NULL || identifier == ZR_NULL ||
            node->type != (parameter ? ZR_AST_PARAMETER : ZR_AST_VARIABLE_DECLARATION)) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        identifier->syntaxContext = fixture.context;
        if (!declaration_context_prepare(state, &fixture) ||
            !declaration_context_record(&fixture, node, kind, fixture.context, &record) || record != ZR_NULL ||
            !declaration_context_record(&fixture, node, kind, ZrParser_SyntaxContext_Root(), &record) || record != ZR_NULL) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, fixture.analyzer, fixture.ast);
        symbol = declaration_context_symbol(&fixture, node);
        if (symbol == ZR_NULL) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        record = ZrParser_Semantic_FindSymbolById(fixture.analyzer->semanticContext, symbol->semanticId);
        passed = passed && record != ZR_NULL && record->astNode == node && record->kind == kind &&
                 declaration_context_equal(record->syntaxContext, fixture.context, kind) &&
                 symbol->semanticTypeId == record->typeId && record->typeId != ZR_SEMANTIC_ID_INVALID;
        if (marked != 0U) {
            passed = passed && declaration_context_record(
                    &fixture, node, kind, ZrParser_SyntaxContext_Root(), &record) && record == ZR_NULL;
        }
        if (!parameter) {
            const SZrTypeBinding *binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                    fixture.analyzer->compilerState->typeEnv, identifier->name, fixture.context);
            passed = passed && binding != ZR_NULL && symbol != ZR_NULL &&
                     binding->symbolId == symbol->semanticId && binding->typeId == symbol->semanticTypeId;
            if (marked != 0U) {
                passed = passed && ZrParser_TypeEnvironment_FindVariableBindingWithContext(
                        fixture.analyzer->compilerState->typeEnv, identifier->name,
                        ZrParser_SyntaxContext_Root()) == ZR_NULL;
            }
        }
        declaration_context_fixture_free(state, &fixture);
    }
    return passed ? 1 : 0;
}

static int declaration_context_preowned_callable(SZrState *state) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize external = 0U; external < 2U; external++) {
        for (TZrSize marked = 0U; marked < 2U; marked++) {
            SZrTestDeclarationContextFixture fixture;
            SZrAstNode *node;
            SZrIdentifier *identifier;
            SZrInferredType type;
            SZrArray parameters;
            SZrSymbol *symbol;
            TZrSymbolId symbolId;
            TZrTypeId typeId;
            TZrOverloadSetId overloadId;
            const SZrSemanticSymbolRecord *record;
            const SZrCanonicalTypeNode *canonical;
            SZrTestDeclarationContextSnapshot before;
            const char *source = external ? "native extern(\"fixture\") { fn contextEcho(value:i32):i32; }"
                                          : "fn contextEcho(value:int):int { return value; }";
            if (!declaration_context_fixture_init(state, &fixture, source, marked != 0U)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            node = declaration_context_node(&fixture, 0U);
            identifier = declaration_context_identifier(node);
            if (identifier == ZR_NULL || identifier->name == ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            identifier->syntaxContext = fixture.context;
            if (!declaration_context_prepare(state, &fixture)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            ZrParser_InferredType_Init(state, &type, ZR_VALUE_TYPE_INT64);
            ZrCore_Array_Init(state, &parameters, sizeof(SZrInferredType), 1U);
            ZrCore_Array_Push(state, &parameters, &type);
            if (external) {
                (void)ZrLanguageServer_SemanticAnalyzer_RegisterCanonicalExternFunctionBindings(
                        state, fixture.analyzer, node, &node->data.externFunctionDeclaration);
            } else {
                (void)ZrParser_TypeEnvironment_RegisterFunctionEx(
                        state, fixture.analyzer->compilerState->typeEnv, identifier->name,
                        &type, &parameters, ZR_NULL, ZR_NULL, node);
            }
            ZrCore_Array_Free(state, &parameters);
            TZrBool recordFound = declaration_context_record(
                    &fixture, node, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, fixture.context, &record);
            canonical = record != ZR_NULL ? ZrParser_CanonicalType_Find(
                    fixture.analyzer->semanticContext, record->typeId) : ZR_NULL;
            if (!recordFound || record == ZR_NULL || canonical == ZR_NULL || canonical->kind != ZR_CANONICAL_TYPE_FUNCTION ||
                canonical->data.function.parameterContracts.length != 1U) {
                ZrParser_InferredType_Free(state, &type);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            symbolId = record->id;
            typeId = record->typeId;
            overloadId = record->overloadSetId;
            if (!declaration_context_overload_contains(
                        fixture.analyzer->semanticContext, "contextEcho", overloadId, symbolId)) {
                ZrParser_InferredType_Free(state, &type);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            if (marked != 0U && ZrParser_Semantic_RegisterSymbolWithContext(
                    fixture.analyzer->semanticContext, identifier->name, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                    ZR_SEMANTIC_ID_INVALID, ZR_SEMANTIC_ID_INVALID, node, node->location,
                    ZrParser_SyntaxContext_Root()) == ZR_SEMANTIC_ID_INVALID) {
                ZrParser_InferredType_Free(state, &type);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            symbol = declaration_context_add_symbol(state, &fixture, node, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, &type);
            if (symbol == ZR_NULL) {
                ZrParser_InferredType_Free(state, &type);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            before = declaration_context_snapshot(&fixture);
            passed = ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
                    fixture.analyzer, symbol, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, &type,
                    ZR_SEMANTIC_TYPE_KIND_UNKNOWN) && passed;
            passed = passed && symbol->semanticId == symbolId && symbol->semanticTypeId == typeId &&
                     symbol->overloadSetId == overloadId && overloadId != ZR_SEMANTIC_ID_INVALID &&
                     declaration_context_overload_contains(
                             fixture.analyzer->semanticContext, "contextEcho", overloadId, symbolId) &&
                     declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
            canonical = ZrParser_CanonicalType_Find(fixture.analyzer->semanticContext, typeId);
            if (canonical == ZR_NULL || canonical->kind != ZR_CANONICAL_TYPE_FUNCTION) {
                passed = ZR_FALSE;
            } else {
                const SZrCanonicalParameterContract *parameter = ZrCore_Array_Get(
                        (SZrArray *)&canonical->data.function.parameterContracts, 0U);
                passed = passed && parameter != ZR_NULL &&
                         parameter->typeId == canonical->data.function.returnTypeId;
            }
            ZrParser_InferredType_Free(state, &type);
            declaration_context_fixture_free(state, &fixture);
        }
    }
    return passed ? 1 : 0;
}

static int declaration_context_preowned_nominal(SZrState *state) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize index = 0U; index < 6U; index++) {
        for (TZrSize marked = 0U; marked < 2U; marked++) {
            SZrTestDeclarationContextFixture fixture;
            SZrAstNode *node;
            SZrIdentifier *identifier;
            SZrSymbol *symbol;
            const SZrSemanticSymbolRecord *record;
            const SZrSemanticTypeRecord *type;
            TZrSymbolId symbolId;
            TZrTypeId typeId;
            SZrTestDeclarationContextSnapshot before;
            if (!declaration_context_fixture_init(state, &fixture, declaration_context_nominal_sources[index], marked != 0U)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            node = declaration_context_node(&fixture, 0U);
            identifier = declaration_context_identifier(node);
            if (identifier == ZR_NULL || identifier->name == ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            identifier->syntaxContext = fixture.context;
            if (!declaration_context_prepare(state, &fixture) ||
                !ZrParser_TypeEnvironment_RegisterTypeDeclaration(
                        state, fixture.analyzer->compilerState->typeEnv, identifier->name, node)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            TZrBool recordFound = declaration_context_record(
                    &fixture, node, ZR_SEMANTIC_SYMBOL_KIND_TYPE, fixture.context, &record);
            type = record != ZR_NULL ? declaration_context_type(&fixture, record->typeId) : ZR_NULL;
            if (!recordFound || record == ZR_NULL || type == ZR_NULL || type->kind != ZR_SEMANTIC_TYPE_KIND_UNKNOWN) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            symbolId = record->id;
            typeId = record->typeId;
            if (marked != 0U && ZrParser_Semantic_RegisterSymbolWithContext(
                    fixture.analyzer->semanticContext, identifier->name, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    ZR_SEMANTIC_ID_INVALID, ZR_SEMANTIC_ID_INVALID, node, node->location,
                    ZrParser_SyntaxContext_Root()) == ZR_SEMANTIC_ID_INVALID) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            symbol = declaration_context_add_symbol(state, &fixture, node, ZR_SEMANTIC_SYMBOL_KIND_TYPE, ZR_NULL);
            if (symbol == ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            before = declaration_context_snapshot(&fixture);
            passed = ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
                    fixture.analyzer, symbol, ZR_SEMANTIC_SYMBOL_KIND_TYPE, ZR_NULL,
                    ZR_SEMANTIC_TYPE_KIND_REFERENCE) && passed;
            type = declaration_context_type(&fixture, typeId);
            passed = passed && symbol->semanticId == symbolId && symbol->semanticTypeId == typeId &&
                     type != ZR_NULL && type->kind == ZR_SEMANTIC_TYPE_KIND_UNKNOWN &&
                     declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
            declaration_context_fixture_free(state, &fixture);
        }
    }
    return passed ? 1 : 0;
}

static int declaration_context_missing_nominal(SZrState *state) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize index = 4U; index < 6U; index++) {
        for (TZrSize marked = 0U; marked < 2U; marked++) {
            SZrTestDeclarationContextFixture fixture;
            SZrAstNode *node;
            SZrIdentifier *identifier;
            SZrInferredType projection;
            SZrSymbol *symbol;
            const SZrSemanticSymbolRecord *record;
            const SZrSemanticTypeRecord *type;
            const SZrNamedTypeBinding *binding;
            const SZrCanonicalTypeNode *canonical;
            SZrTestDeclarationContextSnapshot before, after;
            TZrSymbolId symbolId;
            TZrTypeId typeId;
            if (!declaration_context_fixture_init(state, &fixture, declaration_context_nominal_sources[index], marked != 0U)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            node = declaration_context_node(&fixture, 0U);
            identifier = declaration_context_identifier(node);
            if (identifier == ZR_NULL || identifier->name == ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            identifier->syntaxContext = fixture.context;
            if (!declaration_context_prepare(state, &fixture) ||
                !declaration_context_record(&fixture, node, ZR_SEMANTIC_SYMBOL_KIND_TYPE, fixture.context, &record) ||
                record != ZR_NULL ||
                ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                        fixture.analyzer->compilerState->typeEnv, identifier->name, fixture.context) != ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            ZrParser_InferredType_InitFull(state, &projection, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, identifier->name);
            symbol = declaration_context_add_symbol(state, &fixture, node, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                                     index == 5U ? &projection : ZR_NULL);
            if (symbol == ZR_NULL) {
                ZrParser_InferredType_Free(state, &projection);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            before = declaration_context_snapshot(&fixture);
            passed = ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
                    fixture.analyzer, symbol, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    index == 5U ? &projection : ZR_NULL, ZR_SEMANTIC_TYPE_KIND_REFERENCE) && passed;
            record = ZrParser_Semantic_FindSymbolById(fixture.analyzer->semanticContext, symbol->semanticId);
            type = declaration_context_type(&fixture, symbol->semanticTypeId);
            canonical = ZrParser_CanonicalType_Find(fixture.analyzer->semanticContext, symbol->semanticTypeId);
            binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                    fixture.analyzer->compilerState->typeEnv, identifier->name, fixture.context);
            after = declaration_context_snapshot(&fixture);
            passed = passed && record != ZR_NULL && record->astNode == node &&
                     declaration_context_equal(record->syntaxContext, fixture.context, ZR_SEMANTIC_SYMBOL_KIND_TYPE) &&
                     type != ZR_NULL && type->kind == ZR_SEMANTIC_TYPE_KIND_REFERENCE &&
                     type->inferredType.baseType == ZR_VALUE_TYPE_OBJECT &&
                     type->inferredType.typeName != ZR_NULL &&
                     ZrCore_String_Equal(type->inferredType.typeName, identifier->name) &&
                     canonical != ZR_NULL && canonical->kind == ZR_CANONICAL_TYPE_NOMINAL &&
                     binding != ZR_NULL && binding->declarationNode == node &&
                     binding->symbolId == symbol->semanticId && binding->typeId == symbol->semanticTypeId &&
                     after.symbols == before.symbols + 1U && after.named == before.named + 1U &&
                     after.names == before.names + (marked == 0U ? 1U : 0U);
            if (marked != 0U) {
                passed = passed && ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                        fixture.analyzer->compilerState->typeEnv, identifier->name, ZrParser_SyntaxContext_Root()) == ZR_NULL &&
                         declaration_context_record(&fixture, node, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                                     ZrParser_SyntaxContext_Root(), &record) && record == ZR_NULL;
            }
            symbolId = symbol->semanticId;
            typeId = symbol->semanticTypeId;
            passed = ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
                    fixture.analyzer, symbol, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                    index == 5U ? &projection : ZR_NULL, ZR_SEMANTIC_TYPE_KIND_REFERENCE) && passed;
            passed = passed && symbol->semanticId == symbolId && symbol->semanticTypeId == typeId &&
                     declaration_context_unchanged(after, declaration_context_snapshot(&fixture));
            ZrParser_InferredType_Free(state, &projection);
            declaration_context_fixture_free(state, &fixture);
        }
    }
    return passed ? 1 : 0;
}

static int declaration_context_nominal_collector(SZrState *state) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize index = 0U; index < 6U; index++) {
        for (TZrSize marked = 0U; marked < 2U; marked++) {
            SZrTestDeclarationContextFixture fixture;
            SZrAstNode *node;
            SZrIdentifier *identifier;
            SZrSymbol *symbol;
            const SZrSemanticSymbolRecord *record;
            const SZrNamedTypeBinding *binding;
            if (!declaration_context_fixture_init(state, &fixture, declaration_context_nominal_sources[index], marked != 0U)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            node = declaration_context_node(&fixture, 0U);
            identifier = declaration_context_identifier(node);
            if (identifier == ZR_NULL || identifier->name == ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            identifier->syntaxContext = fixture.context;
            if (!declaration_context_prepare(state, &fixture)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, fixture.analyzer, fixture.ast);
            symbol = declaration_context_symbol(&fixture, node);
            if (symbol == ZR_NULL) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            record = ZrParser_Semantic_FindSymbolById(fixture.analyzer->semanticContext, symbol->semanticId);
            binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                    fixture.analyzer->compilerState->typeEnv, identifier->name, fixture.context);
            passed = passed && record != ZR_NULL && record->astNode == node &&
                     record->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
                     declaration_context_equal(record->syntaxContext, fixture.context, ZR_SEMANTIC_SYMBOL_KIND_TYPE) &&
                     binding != ZR_NULL && binding->declarationNode == node &&
                     binding->symbolId == symbol->semanticId && binding->typeId == symbol->semanticTypeId;
            if (marked != 0U) {
                passed = passed && ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                        fixture.analyzer->compilerState->typeEnv, identifier->name, ZrParser_SyntaxContext_Root()) == ZR_NULL;
            }
            declaration_context_fixture_free(state, &fixture);
        }
    }
    return passed ? 1 : 0;
}

static int declaration_context_nominal_owner(SZrState *state) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize control = 0U; control < 3U; control++) {
        SZrTestDeclarationContextFixture fixture;
        SZrAstNode *first, *second;
        SZrIdentifier *firstName, *secondName;
        SZrCompilerState *compiler;
        SZrTestDeclarationContextSnapshot before;
        const SZrNamedTypeBinding *binding;
        if (!declaration_context_fixture_init(state, &fixture,
                "struct ContextOwner {} struct ContextOwner {}", control == 1U)) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        if (control == 2U) {
            fixture.context = fixture.mixed;
        }
        first = declaration_context_node(&fixture, 0U);
        second = declaration_context_node(&fixture, 1U);
        firstName = declaration_context_identifier(first);
        secondName = declaration_context_identifier(second);
        if (firstName == ZR_NULL || secondName == ZR_NULL) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        firstName->syntaxContext = secondName->syntaxContext = fixture.context;
        if (!declaration_context_prepare(state, &fixture)) {
            declaration_context_fixture_free(state, &fixture);
            return -1;
        }
        compiler = fixture.analyzer->compilerState;
        passed = ZrParser_Compiler_RegisterTypeBinding(compiler, firstName->name, first->location, first) && passed;
        binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(compiler->typeEnv, firstName->name, fixture.context);
        passed = passed && binding != ZR_NULL && binding->declarationNode == first;
        if (control == 2U) {
            passed = passed && binding == ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                    compiler->typeEnv, firstName->name, ZrParser_SyntaxContext_Root());
            firstName->syntaxContext = ZrParser_SyntaxContext_Root();
        }
        before = declaration_context_snapshot(&fixture);
        passed = ZrParser_Compiler_RegisterTypeBinding(compiler, firstName->name, first->location, first) && passed;
        passed = passed && !compiler->hasError && declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
        declaration_context_clear_error(compiler);
        before = declaration_context_snapshot(&fixture);
        if (control == 2U) {
            secondName->syntaxContext = ZrParser_SyntaxContext_Root();
        }
        passed = !ZrParser_Compiler_RegisterTypeBinding(compiler, secondName->name, second->location, second) && passed;
        passed = passed && declaration_context_duplicate(compiler, second->location, first->location) &&
                 declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
        declaration_context_clear_error(compiler);
        secondName->syntaxContext = control == 1U ? ZrParser_SyntaxContext_Root() : fixture.marked;
        passed = ZrParser_Compiler_RegisterTypeBinding(compiler, secondName->name, second->location, second) && passed;
        binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(compiler->typeEnv, secondName->name, secondName->syntaxContext);
        passed = passed && !compiler->hasError && binding != ZR_NULL && binding->declarationNode == second;
        declaration_context_fixture_free(state, &fixture);
    }
    return passed ? 1 : 0;
}

static int declaration_context_invalid(SZrState *state) {
    TZrBool passed = ZR_TRUE;
    for (TZrSize index = 0U; index < 10U; index++) {
        for (TZrSize seeded = 0U; seeded < 2U; seeded++) {
            SZrTestDeclarationContextFixture fixture;
            SZrAstNode *node;
            SZrIdentifier *identifier;
            SZrInferredType type;
            TZrTypeId typeId;
            TZrSymbolId rootId = ZR_SEMANTIC_ID_INVALID;
            SZrSymbol *symbol;
            SZrTestDeclarationContextSnapshot before;
            EZrSemanticSymbolKind kind = index == 0U ? ZR_SEMANTIC_SYMBOL_KIND_VARIABLE :
                    index == 1U ? ZR_SEMANTIC_SYMBOL_KIND_PARAMETER :
                    index == 2U ? ZR_SEMANTIC_SYMBOL_KIND_FUNCTION : ZR_SEMANTIC_SYMBOL_KIND_TYPE;
            const char *source = index == 0U || index == 9U ? "var contextInvalid:int=1;" :
                    index == 1U || index == 2U ? "fn contextInvalid(value:int):int { return value; }" :
                    declaration_context_nominal_sources[index - 3U];
            if (!declaration_context_fixture_init(state, &fixture, source, ZR_TRUE)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            node = declaration_context_node(&fixture, 0U);
            if (index == 1U && node != ZR_NULL && node->type == ZR_AST_FUNCTION_DECLARATION &&
                node->data.functionDeclaration.params != ZR_NULL && node->data.functionDeclaration.params->count == 1U) {
                node = node->data.functionDeclaration.params->nodes[0];
            }
            identifier = declaration_context_identifier(node);
            if (identifier == ZR_NULL || identifier->name == ZR_NULL || !declaration_context_prepare(state, &fixture)) {
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            ZrParser_InferredType_Init(state, &type, ZR_VALUE_TYPE_INT64);
            if (seeded != 0U) {
                typeId = ZrParser_Semantic_RegisterInferredType(fixture.analyzer->semanticContext,
                        &type, ZR_SEMANTIC_TYPE_KIND_VALUE, ZR_NULL, node);
                rootId = ZrParser_Semantic_RegisterSymbolWithContext(fixture.analyzer->semanticContext,
                        identifier->name, kind, typeId, ZR_SEMANTIC_ID_INVALID, node, node->location,
                        ZrParser_SyntaxContext_Root());
                if (typeId == ZR_SEMANTIC_ID_INVALID || rootId == ZR_SEMANTIC_ID_INVALID) {
                    ZrParser_InferredType_Free(state, &type);
                    declaration_context_fixture_free(state, &fixture);
                    return -1;
                }
            }
            symbol = declaration_context_add_symbol(state, &fixture, node, kind, &type);
            if (symbol == ZR_NULL) {
                ZrParser_InferredType_Free(state, &type);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            identifier->syntaxContext = index == 9U ? fixture.context :
                    (SZrSyntaxContext){fixture.table, ZR_SYNTAX_CONTEXT_INVALID_MARK_ID};
            if (index != 9U && ZrParser_SyntaxContext_IsValid(identifier->syntaxContext)) {
                ZrParser_InferredType_Free(state, &type);
                declaration_context_fixture_free(state, &fixture);
                return -1;
            }
            before = declaration_context_snapshot(&fixture);
            passed = !ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(
                    fixture.analyzer, symbol, kind, kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE ? ZR_NULL : &type,
                    ZR_SEMANTIC_TYPE_KIND_REFERENCE) && passed;
            passed = passed && symbol->semanticId == ZR_SEMANTIC_ID_INVALID &&
                     symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID &&
                     symbol->overloadSetId == ZR_SEMANTIC_ID_INVALID &&
                     declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
            if (seeded != 0U && (index == 0U || index == 1U)) {
                symbol->semanticId = rootId;
                symbol->semanticTypeId = typeId;
                before = declaration_context_snapshot(&fixture);
                ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
                        state, fixture.analyzer, identifier->name, &type, node);
                passed = passed && declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
            }
            if (kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE && index != 9U) {
                before = declaration_context_snapshot(&fixture);
                passed = !ZrParser_Compiler_RegisterTypeBinding(fixture.analyzer->compilerState,
                        identifier->name, node->location, node) && passed;
                passed = passed && declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
            }
            ZrParser_InferredType_Free(state, &type);
            declaration_context_fixture_free(state, &fixture);
        }
    }
    return passed ? 1 : 0;
}

static SZrAstNode *declaration_context_alias_key(SZrAstNode *node) {
    SZrAstNode *pattern = node != ZR_NULL && node->type == ZR_AST_VARIABLE_DECLARATION
                                 ? node->data.variableDeclaration.pattern : ZR_NULL;
    SZrAstNodeArray *keys = pattern != ZR_NULL && pattern->type == ZR_AST_DESTRUCTURING_OBJECT
                                  ? pattern->data.destructuringObject.keys : ZR_NULL;
    SZrAstNode *key = keys != ZR_NULL && keys->count == 1U ? keys->nodes[0] : ZR_NULL;
    return key != ZR_NULL && key->type == ZR_AST_IDENTIFIER_LITERAL ? key : ZR_NULL;
}

static int declaration_context_root_alias(SZrState *state) {
    SZrTestDeclarationContextFixture fixture;
    SZrAstNode *first, *second, *firstKey, *secondKey;
    SZrCompilerState *compiler;
    const SZrNamedTypeBinding *binding;
    SZrTestDeclarationContextSnapshot before;
    TZrBool moduleReady = ZR_FALSE, memberReady = ZR_FALSE, typeReady = ZR_FALSE;
    TZrBool passed;
    if (!declaration_context_fixture_init(state, &fixture,
            "var {Pair}=import(\"zr.container\"); var {Pair}=import(\"zr.container\");", ZR_FALSE)) {
        declaration_context_fixture_free(state, &fixture);
        return -1;
    }
    first = declaration_context_node(&fixture, 0U);
    second = declaration_context_node(&fixture, 1U);
    firstKey = declaration_context_alias_key(first);
    secondKey = declaration_context_alias_key(second);
    if (firstKey == ZR_NULL || secondKey == ZR_NULL || !declaration_context_prepare(state, &fixture)) {
        declaration_context_fixture_free(state, &fixture);
        return -1;
    }
    compiler = fixture.analyzer->compilerState;
    ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, fixture.analyzer, first);
    for (TZrSize index = 0U; index < compiler->typePrototypes.length; index++) {
        const SZrTypePrototypeInfo *prototype = ZrCore_Array_Get(&compiler->typePrototypes, index);
        if (prototype != ZR_NULL && prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE &&
            declaration_context_text(prototype->name, "zr.container")) {
            moduleReady = ZR_TRUE;
            for (TZrSize memberIndex = 0U; memberIndex < prototype->members.length; memberIndex++) {
                const SZrTypeMemberInfo *member = ZrCore_Array_Get((SZrArray *)&prototype->members, memberIndex);
                if (member != ZR_NULL && declaration_context_text(member->name, "Pair") &&
                    declaration_context_text(member->fieldTypeName, "Pair")) {
                    memberReady = ZR_TRUE;
                }
            }
        }
        if (prototype != ZR_NULL && declaration_context_text(prototype->name, "Pair")) {
            typeReady = ZR_TRUE;
        }
    }
    if (!moduleReady || !memberReady || !typeReady) {
        declaration_context_fixture_free(state, &fixture);
        return -1;
    }
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(compiler->typeEnv,
            firstKey->data.identifier.name, ZrParser_SyntaxContext_Root());
    passed = !compiler->hasError && binding != ZR_NULL && binding->declarationNode == firstKey;
    declaration_context_clear_error(compiler);
    before = declaration_context_snapshot(&fixture);
    passed = !ZrParser_Compiler_ReportDuplicateTypeDeclaration(compiler, firstKey, ZR_NULL) && passed;
    passed = passed && !compiler->hasError && declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
    passed = ZrParser_Compiler_RegisterTypeBinding(compiler, firstKey->data.identifier.name,
            firstKey->location, firstKey) && passed;
    passed = passed && !compiler->hasError && declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
    declaration_context_clear_error(compiler);
    before = declaration_context_snapshot(&fixture);
    passed = !ZrParser_Compiler_RegisterTypeBinding(compiler, secondKey->data.identifier.name,
            secondKey->location, secondKey) && passed;
    passed = passed && declaration_context_duplicate(compiler, secondKey->location, firstKey->location) &&
             declaration_context_unchanged(before, declaration_context_snapshot(&fixture));
    declaration_context_fixture_free(state, &fixture);
    return passed ? 1 : 0;
}

#define DECLARATION_CONTEXT_CASE(summary, reason, expression) do { \
    SZrTestTimer timer; \
    int result; \
    TEST_START(summary); \
    result = (expression); \
    if (result < 0) { \
        TEST_FAIL(timer, summary, "Hygiene fixture prerequisite failed before " summary); \
    } else if (result == 0) { \
        TEST_FAIL(timer, summary, reason); \
    } else { \
        TEST_PASS(timer, summary); \
    } \
} while (0)

static void test_semantic_analyzer_context_unowned_variable(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Unowned Variable",
            "Unowned variable collection must publish its declaration context without a Root binding",
            declaration_context_unowned_local(state, ZR_FALSE));
}

static void test_semantic_analyzer_context_unowned_parameter(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Unowned Parameter",
            "Unowned parameter collection must preserve its LOCAL declaration context",
            declaration_context_unowned_local(state, ZR_TRUE));
}

static void test_semantic_analyzer_context_preowned_callable(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Preowned Callable",
            "Callable reuse must preserve exact canonical IDs, full signature and overload membership",
            declaration_context_preowned_callable(state));
}

static void test_semantic_analyzer_context_preowned_nominal(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Preowned Nominal",
            "Nominal reuse must select its exact context and preserve the existing UNKNOWN projection",
            declaration_context_preowned_nominal(state));
}

static void test_semantic_analyzer_context_missing_nominal(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Missing Nominal",
            "Fresh TYPE publication must retain nominal context, REFERENCE kind and stable canonical IDs",
            declaration_context_missing_nominal(state));
}

static void test_semantic_analyzer_context_nominal_collector(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Nominal Collector",
            "Real nominal collection must retain AST-owned canonical identity without a marked Root binding",
            declaration_context_nominal_collector(state));
}

static void test_semantic_analyzer_context_nominal_owner(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Nominal Owner",
            "Nominal registration must be same-AST idempotent and diagnose only same-context distinct owners",
            declaration_context_nominal_owner(state));
}

static void test_semantic_analyzer_context_invalid_no_publication(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Invalid No Publication",
            "Invalid declaration contexts must not publish or reuse Root IDs, types, overloads or bindings",
            declaration_context_invalid(state));
}

static void test_semantic_analyzer_context_root_alias_owner(SZrState *state) {
    DECLARATION_CONTEXT_CASE("Semantic Analyzer Context Root Alias Owner",
            "Root shorthand aliases must retain identifier owners and preserve duplicate diagnostics",
            declaration_context_root_alias(state));
}

#undef DECLARATION_CONTEXT_CASE
#endif
