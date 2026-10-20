#include "zr_vm_parser/semantic_query.h"

static const ZrTestsFixtureSource member_semantic_fixtures[] = {
    {.path = "typed_shape_provider", .source =
        "module typed_shape_provider; pub fn answer(): int { return 17; } "
        "pub var number: int = 17;"}
};

typedef struct SZrMemberSemanticLocal {
    TZrBool queried, bindingFound, bindingMatches, declarationMatches;
    TZrBool irPublished, irMatches, placeMatches, instructionMatches;
    TZrBool memberFound;
    TZrSymbolId symbolId, memberSymbolId;
    TZrTypeId typeId, memberTypeId;
    TZrPlaceId lexicalPlaceId, irPlaceId;
    TZrUInt32 kind, primitive, argumentCount, returnKind, returnPrimitive;
} SZrMemberSemanticLocal;

typedef struct SZrMemberSemanticObservation {
    TZrBool stateCreated, parsed, declarationLocated, compiled, validated, annotationPreserved;
    TZrBool typeMismatch;
    SZrMemberSemanticLocal local[2];
    TZrChar error[256];
} SZrMemberSemanticObservation;

static TZrBool member_semantic_loader(SZrState *loaderState,
        TZrNativeString path, TZrNativeString hash, SZrIo *io) {
    return ZrTests_Fixture_SourceLoaderFromArray(loaderState, path, hash, io,
            member_semantic_fixtures, 1U);
}

static TZrBool member_semantic_same_range(SZrFileRange left, SZrFileRange right) {
    return (TZrBool)(left.source == right.source &&
            left.start.offset == right.start.offset && left.end.offset == right.end.offset &&
            left.start.line == right.start.line && left.end.line == right.end.line &&
            left.start.column == right.start.column && left.end.column == right.end.column);
}

static void member_semantic_copy_local(SZrCompilerState *cs, SZrAstNode *decl,
        SZrMemberSemanticLocal *out) {
    SZrParserSemanticSymbolQuery query;
    SZrParserSemanticQueryFacts facts;
    SZrAstNode *pattern, *value, *member;
    const SZrSemanticReferenceFact *declaration;
    const SZrTypeBinding *binding;
    const SZrCanonicalTypeNode *type, *returnType;
    const SZrSemanticIrFunction *ir = ZrParser_Compiler_PreSemanticIr(cs);
    if (decl == ZR_NULL || decl->type != ZR_AST_VARIABLE_DECLARATION ||
            cs->semanticContext == ZR_NULL) return;
    pattern = decl->data.variableDeclaration.pattern;
    if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL) return;
    out->queried = ZrParser_SemanticQuery_SymbolAt(
            cs->semanticContext, pattern->location, ZR_NULL, &query);
    if (out->queried) {
        out->symbolId = query.symbolId;
        out->typeId = query.typeId;
        declaration = ZrParser_SemanticQuery_DeclarationOf(
                cs->semanticContext, query.symbolId, ZR_NULL);
        out->declarationMatches = declaration != ZR_NULL &&
                declaration->typeId == query.typeId && query.declarationNode == decl &&
                query.kind == ZR_SEMANTIC_SYMBOL_KIND_VARIABLE &&
                member_semantic_same_range(declaration->range, pattern->location) &&
                member_semantic_same_range(query.declarationRange, pattern->location);
        type = ZrParser_CanonicalType_Find(cs->semanticContext, query.typeId);
        if (type != ZR_NULL) {
            out->kind = (TZrUInt32)type->kind;
            if (type->kind == ZR_CANONICAL_TYPE_PRIMITIVE)
                out->primitive = (TZrUInt32)type->data.primitive.valueType;
            if (type->kind == ZR_CANONICAL_TYPE_FUNCTION) {
                out->argumentCount = (TZrUInt32)type->data.function.parameterContracts.length;
                returnType = ZrParser_CanonicalType_Find(
                        cs->semanticContext, type->data.function.returnTypeId);
                if (returnType != ZR_NULL) {
                    out->returnKind = (TZrUInt32)returnType->kind;
                    if (returnType->kind == ZR_CANONICAL_TYPE_PRIMITIVE)
                        out->returnPrimitive = (TZrUInt32)returnType->data.primitive.valueType;
                }
            }
        }
    }
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            cs->typeEnv, pattern->data.identifier.name, pattern->data.identifier.syntaxContext);
    out->bindingFound = binding != ZR_NULL && binding->hasDeclarationRange &&
            member_semantic_same_range(binding->declarationRange, pattern->location);
    if (out->bindingFound) {
        out->lexicalPlaceId = binding->placeId;
        out->bindingMatches = out->queried && binding->symbolId == out->symbolId &&
                binding->typeId == out->typeId;
    }
    value = decl->data.variableDeclaration.value;
    if (value != ZR_NULL && value->type == ZR_AST_PRIMARY_EXPRESSION &&
            value->data.primaryExpression.members != ZR_NULL &&
            value->data.primaryExpression.members->count != 0U) {
        member = value->data.primaryExpression.members->nodes[
                value->data.primaryExpression.members->count - 1U];
        if (member != ZR_NULL && member->type == ZR_AST_MEMBER_EXPRESSION &&
                member->data.memberExpression.property != ZR_NULL &&
                ZrParser_SemanticQuery_FactsAt(cs->semanticContext,
                        member->data.memberExpression.property->location, ZR_NULL, &facts) &&
                facts.reference != ZR_NULL &&
                facts.reference->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS) {
            out->memberFound = ZR_TRUE;
            out->memberTypeId = facts.reference->typeId;
            out->memberSymbolId = facts.reference->symbolId;
        }
    }
    if (ir == ZR_NULL) return;
    for (TZrSize i = 0U; i < ir->locals.length; i++) {
        const SZrSemanticIrLocal *local = ZrCore_Array_Get((SZrArray *)&ir->locals, i);
        const SZrParserPlace *place = local != ZR_NULL
                ? ZrParser_PlaceGraph_Get(&ir->places, local->placeId) : ZR_NULL;
        if (place == ZR_NULL || !member_semantic_same_range(
                place->sourceRange, pattern->location)) continue;
        out->irPublished = ZR_TRUE;
        out->irPlaceId = local->placeId;
        out->irMatches = out->queried && local->symbolId == out->symbolId &&
                local->typeId == out->typeId && !local->isParameter;
        out->placeMatches = place->id == local->placeId && place->typeId == out->typeId &&
                place->base.kind == ZR_PARSER_PLACE_BASE_LOCAL &&
                place->parentId == ZR_PLACE_ID_INVALID;
    }
    for (TZrSize i = 0U; i < ir->instructions.length; i++) {
        const SZrSemanticIrInstruction *instruction =
                ZrCore_Array_Get((SZrArray *)&ir->instructions, i);
        if (instruction != ZR_NULL && instruction->opcode == ZR_SEMANTIC_IR_PLACE_BASE &&
                instruction->symbolId == out->symbolId && instruction->typeId == out->typeId &&
                instruction->placeId == out->irPlaceId &&
                member_semantic_same_range(instruction->sourceRange, pattern->location))
            out->instructionMatches = ZR_TRUE;
    }
}

static SZrMemberSemanticObservation observe_member_semantics(const TZrChar *source,
        TZrBool shadow) {
    SZrMemberSemanticObservation out = {0};
    SZrState *caseState = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrCompilerState cs = {0};
    SZrAstNode *ast = ZR_NULL, *decl = ZR_NULL, *inner = ZR_NULL;
    SZrFunction *root = ZR_NULL;
    SZrString *sourceName;
    const SZrType *annotation = ZR_NULL;
    TZrBool initialized = ZR_FALSE, ignored = ZR_FALSE;
    TZrBool (*previousLoader)(SZrState *, TZrNativeString, TZrNativeString, SZrIo *) = ZR_NULL;
    if (caseState == ZR_NULL) return out;
    out.stateCreated = ZR_TRUE;
    previousLoader = caseState->global->sourceLoader;
    caseState->global->sourceLoader = member_semantic_loader;
    ZrParser_ToGlobalState_Register(caseState);
    sourceName = ZrCore_String_CreateFromNative(caseState, "member_semantics_consumer.zr");
    if (sourceName == ZR_NULL) goto cleanup;
    ast = ZrParser_Parse(caseState, source, strlen(source), sourceName);
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
            ast->data.script.statements == ZR_NULL || ast->data.script.statements->count < 2U)
        goto cleanup;
    out.parsed = ZR_TRUE;
    decl = ast->data.script.statements->nodes[1];
    if (decl == ZR_NULL || decl->type != ZR_AST_VARIABLE_DECLARATION) goto cleanup;
    if (shadow) {
        SZrAstNode *block;
        if (ast->data.script.statements->count < 3U) goto cleanup;
        block = ast->data.script.statements->nodes[2];
        if (block != ZR_NULL && block->type == ZR_AST_EXPRESSION_STATEMENT)
            block = block->data.expressionStatement.expr;
        if (block == ZR_NULL || block->type != ZR_AST_BLOCK ||
                block->data.block.body == ZR_NULL || block->data.block.body->count == 0U)
            goto cleanup;
        inner = block->data.block.body->nodes[0];
        if (inner == ZR_NULL || inner->type != ZR_AST_VARIABLE_DECLARATION) goto cleanup;
    }
    out.declarationLocated = ZR_TRUE;
    annotation = decl->data.variableDeclaration.typeInfo;
    ZrParser_CompilerState_Init(&cs, caseState);
    initialized = ZR_TRUE;
    cs.currentAst = ast;
    cs.suppressErrorOutput = ZR_TRUE;
    cs.currentFunction = ZrCore_Function_New(caseState);
    root = cs.currentFunction;
    if (root == ZR_NULL || !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            caseState->global, caseState, ZR_CAST_RAW_OBJECT_AS_SUPER(root), &ignored))
        goto cleanup;
    compile_script(&cs, ast);
    out.compiled = !cs.hasError;
    out.validated = ZrParser_Compiler_PreSemanticIrIsValidated(&cs);
    if (cs.hasStructuredError && cs.structuredError.code != ZR_NULL) {
        const TZrChar *code = ZrCore_String_GetNativeString(cs.structuredError.code);
        out.typeMismatch = code != ZR_NULL && strcmp(code, "type_mismatch") == 0;
    }
    if (cs.errorMessage != ZR_NULL) snprintf(out.error, sizeof(out.error), "%s", cs.errorMessage);
    out.annotationPreserved = annotation != ZR_NULL &&
            decl->data.variableDeclaration.typeInfo == annotation;
    member_semantic_copy_local(&cs, decl, &out.local[0]);
    if (inner != ZR_NULL) member_semantic_copy_local(&cs, inner, &out.local[1]);
cleanup:
    caseState->global->sourceLoader = previousLoader;
    if (initialized) {
        SZrFunction *currentRoot = cs.currentFunction, *topRoot = cs.topLevelFunction;
        if (ignored) ZrCore_GarbageCollector_UnignoreObject(
                caseState->global, ZR_CAST_RAW_OBJECT_AS_SUPER(root));
        if (root != ZR_NULL) ZrCore_Function_Free(caseState, root);
        if (currentRoot != ZR_NULL && currentRoot != root) ZrCore_Function_Free(caseState, currentRoot);
        if (topRoot != ZR_NULL && topRoot != root && topRoot != currentRoot)
            ZrCore_Function_Free(caseState, topRoot);
        ZrParser_CompilerState_Free(&cs);
    }
    if (ast != ZR_NULL) ZrParser_Ast_Free(caseState, ast);
    ZrTests_Runtime_State_Destroy(caseState);
    return out;
}

static void assert_member_semantic_local(const SZrMemberSemanticObservation *out,
        const SZrMemberSemanticLocal *local) {
    TEST_ASSERT_TRUE(out->stateCreated);
    TEST_ASSERT_TRUE(out->parsed);
    TEST_ASSERT_TRUE(out->declarationLocated);
    TEST_ASSERT_TRUE_MESSAGE(out->compiled, out->error);
    TEST_ASSERT_TRUE(out->validated);
    TEST_ASSERT_TRUE(local->queried);
    TEST_ASSERT_TRUE(local->declarationMatches);
    TEST_ASSERT_TRUE(local->irPublished);
    TEST_ASSERT_TRUE(local->irMatches);
    TEST_ASSERT_TRUE(local->placeMatches);
    TEST_ASSERT_TRUE(local->instructionMatches);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_PLACE_ID_INVALID, local->irPlaceId);
}

static void test_imported_callable_local_keeps_canonical_identity(void) {
    const SZrMemberSemanticObservation out = observe_member_semantics(
            "var provider = import(\"typed_shape_provider\"); var callback = provider.answer; "
            "return callback;", ZR_FALSE);
    assert_member_semantic_local(&out, &out.local[0]);
    TEST_ASSERT_TRUE(out.local[0].bindingMatches);
    TEST_ASSERT_EQUAL_UINT32(ZR_PLACE_ID_INVALID, out.local[0].lexicalPlaceId);
    TEST_ASSERT_TRUE(out.local[0].memberFound);
    TEST_ASSERT_EQUAL_UINT32(out.local[0].memberTypeId, out.local[0].typeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(out.local[0].memberSymbolId, out.local[0].symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_FUNCTION, out.local[0].kind);
    TEST_ASSERT_EQUAL_UINT32(0U, out.local[0].argumentCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, out.local[0].returnKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, out.local[0].returnPrimitive);
}

static void test_imported_noncallable_local_keeps_primitive_type(void) {
    const SZrMemberSemanticObservation out = observe_member_semantics(
            "var provider = import(\"typed_shape_provider\"); var callback = provider.number; "
            "return callback;", ZR_FALSE);
    assert_member_semantic_local(&out, &out.local[0]);
    TEST_ASSERT_TRUE(out.local[0].bindingMatches);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, out.local[0].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, out.local[0].primitive);
}

static void test_imported_callable_shape_respects_lexical_shadowing(void) {
    const SZrMemberSemanticObservation out = observe_member_semantics(
            "var provider = import(\"typed_shape_provider\"); var callback = provider.answer; "
            "{ var callback = 17; var value = callback; }; return callback;", ZR_TRUE);
    assert_member_semantic_local(&out, &out.local[0]);
    assert_member_semantic_local(&out, &out.local[1]);
    TEST_ASSERT_TRUE(out.local[0].bindingMatches);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_FUNCTION, out.local[0].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, out.local[1].kind);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, out.local[1].primitive);
    TEST_ASSERT_NOT_EQUAL_UINT32(out.local[0].symbolId, out.local[1].symbolId);
    TEST_ASSERT_NOT_EQUAL_UINT32(out.local[0].typeId, out.local[1].typeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(out.local[0].irPlaceId, out.local[1].irPlaceId);
}

static void test_explicit_callable_arity_mismatch_is_not_refined(void) {
    const SZrMemberSemanticObservation out = observe_member_semantics(
            "var provider = import(\"typed_shape_provider\"); "
            "var callback: fn(int)->int = provider.answer;", ZR_FALSE);
    TEST_ASSERT_TRUE(out.stateCreated);
    TEST_ASSERT_TRUE(out.parsed);
    TEST_ASSERT_TRUE(out.declarationLocated);
    TEST_ASSERT_FALSE_MESSAGE(out.compiled, out.error);
    TEST_ASSERT_TRUE_MESSAGE(out.typeMismatch, out.error);
    TEST_ASSERT_TRUE(out.annotationPreserved);
    TEST_ASSERT_FALSE(out.local[0].queried &&
            out.local[0].kind == (TZrUInt32)ZR_CANONICAL_TYPE_FUNCTION &&
            out.local[0].argumentCount == 0U);
    TEST_ASSERT_FALSE(out.local[0].irPublished);
}

static void test_callable_binding_survives_statement_block_shadow(void) {
    const SZrMemberSemanticObservation out = observe_member_semantics(
            "var provider = import(\"typed_shape_provider\"); var callback = provider.answer; "
            "if (true) { var callback = 17; var value = callback; } return callback;", ZR_FALSE);
    assert_member_semantic_local(&out, &out.local[0]);
    TEST_ASSERT_TRUE(out.local[0].bindingMatches);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_FUNCTION, out.local[0].kind);
    TEST_ASSERT_EQUAL_UINT32(0U, out.local[0].argumentCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, out.local[0].returnKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, out.local[0].returnPrimitive);
}

static void test_callable_binding_survives_failed_block_expression_shadow(void) {
    const SZrMemberSemanticObservation out = observe_member_semantics(
            "var provider = import(\"typed_shape_provider\"); var callback = provider.answer; "
            "{ var callback = 17; missing_member_scope_probe(); };", ZR_TRUE);
    TEST_ASSERT_TRUE(out.stateCreated);
    TEST_ASSERT_TRUE(out.parsed);
    TEST_ASSERT_TRUE(out.declarationLocated);
    TEST_ASSERT_FALSE_MESSAGE(out.compiled, out.error);
    TEST_ASSERT_NOT_NULL(strstr(out.error, "missing_member_scope_probe"));
    TEST_ASSERT_TRUE(out.local[1].queried);
    TEST_ASSERT_TRUE(out.local[1].irPublished);
    TEST_ASSERT_TRUE(out.local[0].bindingMatches);
    TEST_ASSERT_TRUE(out.local[0].declarationMatches);
    TEST_ASSERT_TRUE(out.local[0].irMatches);
    TEST_ASSERT_TRUE(out.local[0].placeMatches);
    TEST_ASSERT_TRUE(out.local[0].instructionMatches);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_FUNCTION, out.local[0].kind);
    TEST_ASSERT_EQUAL_UINT32(0U, out.local[0].argumentCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_TYPE_PRIMITIVE, out.local[0].returnKind);
    TEST_ASSERT_EQUAL_UINT32(ZR_VALUE_TYPE_INT64, out.local[0].returnPrimitive);
}
