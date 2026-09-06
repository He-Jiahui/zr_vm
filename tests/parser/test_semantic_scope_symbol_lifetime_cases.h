#ifndef ZR_TEST_SEMANTIC_SCOPE_SYMBOL_LIFETIME_CASES_H
#define ZR_TEST_SEMANTIC_SCOPE_SYMBOL_LIFETIME_CASES_H

static struct {
    FZrAllocator original;
    TZrPtr target;
    TZrPtr retired;
    TZrSize retiredSize;
    TZrUInt32 relocations;
} g_scope_symbol_relocation;

static TZrPtr scope_symbol_relocating_allocator(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    if (pointer != ZR_NULL && pointer == g_scope_symbol_relocation.target &&
        newSize > originalSize) {
        TZrPtr replacement = g_scope_symbol_relocation.original(
                userData, ZR_NULL, 0U, newSize, flag);
        if (replacement == ZR_NULL) {
            return ZR_NULL;
        }
        memcpy(replacement, pointer, originalSize);
        /* Quarantine and clear the old records to expose stale IDs on every allocator. */
        memset(pointer, 0, originalSize);
        g_scope_symbol_relocation.retired = pointer;
        g_scope_symbol_relocation.retiredSize = originalSize;
        g_scope_symbol_relocation.target = ZR_NULL;
        g_scope_symbol_relocation.relocations++;
        return replacement;
    }
    return g_scope_symbol_relocation.original(
            userData, pointer, originalSize, newSize, flag);
}

static void scope_symbol_check_owner_after_growth(const TZrChar *source) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "scope_symbol_growth.zr");
    SZrString *ownerName = ZrCore_String_CreateFromNative(g_state, "Container");
    SZrString *fieldName = ZrCore_String_CreateFromNative(g_state, "field");
    SZrString *methodName = ZrCore_String_CreateFromNative(g_state, "echo");
    SZrString *parameterName = ZrCore_String_CreateFromNative(g_state, "value");
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrAstNode *owner;
    SZrAstNode *field;
    SZrAstNode *method;
    SZrAstNode *parameter;
    SZrAstNode *body = ZR_NULL;
    SZrAstNodeArray *members = ZR_NULL;
    SZrAstNodeArray *params = ZR_NULL;
    TZrTypeId ownerTypeId;
    TZrTypeId intTypeId;
    TZrSymbolId ownerId;
    TZrSymbolId fieldId;
    TZrSymbolId methodId;
    TZrSymbolId parameterId;
    TZrSymbolId fieldOwner = ZR_SEMANTIC_ID_INVALID;
    TZrSymbolId parameterOwner = ZR_SEMANTIC_ID_INVALID;
    TZrSymbolId bodyOwner = ZR_SEMANTIC_ID_INVALID;
    TZrBool hasBody;
    TZrBool built;

    TEST_ASSERT_NOT_NULL(context);
    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    owner = ast->data.script.statements->nodes[0];
    switch (owner->type) {
        case ZR_AST_CLASS_DECLARATION:
            members = owner->data.classDeclaration.members;
            break;
        case ZR_AST_STRUCT_DECLARATION:
            members = owner->data.structDeclaration.members;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            members = owner->data.interfaceDeclaration.members;
            break;
        default:
            TEST_FAIL_MESSAGE("Expected a type declaration in the growth fixture");
    }
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)members->count);
    field = members->nodes[0];
    method = members->nodes[1];
    switch (method->type) {
        case ZR_AST_CLASS_METHOD:
            params = method->data.classMethod.params;
            body = method->data.classMethod.body;
            break;
        case ZR_AST_STRUCT_METHOD:
            params = method->data.structMethod.params;
            body = method->data.structMethod.body;
            break;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            params = method->data.interfaceMethodSignature.params;
            break;
        default:
            TEST_FAIL_MESSAGE("Expected a method declaration in the growth fixture");
    }
    TEST_ASSERT_NOT_NULL(params);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)params->count);
    parameter = params->nodes[0];
    hasBody = body != ZR_NULL;
    ownerTypeId = ZrParser_CanonicalType_InternNominal(context, sourceName, ownerName, 0U);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, ownerTypeId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, intTypeId);
    ownerId = ZrParser_Semantic_RegisterSymbol(
            context, ownerName, ZR_SEMANTIC_SYMBOL_KIND_TYPE, ownerTypeId,
            ZR_SEMANTIC_ID_INVALID, owner, owner->location);
    fieldId = ZrParser_Semantic_RegisterSymbol(
            context, fieldName, ZR_SEMANTIC_SYMBOL_KIND_FIELD, intTypeId,
            ZR_SEMANTIC_ID_INVALID, field, field->location);
    methodId = ZrParser_Semantic_RegisterSymbol(
            context, methodName, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION, ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID, method, method->location);
    parameterId = ZrParser_Semantic_RegisterSymbol(
            context, parameterName, ZR_SEMANTIC_SYMBOL_KIND_PARAMETER, intTypeId,
            ZR_SEMANTIC_ID_INVALID, parameter, parameter->location);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, ownerId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, methodId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, fieldId);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, parameterId);
    symbol_append_reference(context, parameter, ZR_SEMANTIC_REFERENCE_DECLARATION,
                            parameterId, intTypeId, parameter->location, parameter->location,
                            ZR_TRUE, ZR_TRUE, parameterName, ZR_NULL);
    while (context->symbols.length < context->symbols.capacity) {
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID,
                ZrParser_Semantic_RegisterSymbol(
                        context, fieldName, ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, intTypeId,
                        ZR_SEMANTIC_ID_INVALID, ZR_NULL, owner->location));
    }

    memset(&g_scope_symbol_relocation, 0, sizeof(g_scope_symbol_relocation));
    g_scope_symbol_relocation.original = g_state->global->allocator;
    g_scope_symbol_relocation.target = context->symbols.head;
    g_state->global->allocator = scope_symbol_relocating_allocator;
    built = ZrParser_Semantic_BuildSourceScopeFacts(context, ast);
    g_state->global->allocator = g_scope_symbol_relocation.original;
    if (g_scope_symbol_relocation.retired != ZR_NULL) {
        ZrCore_Memory_RawFree(g_state->global,
                             g_scope_symbol_relocation.retired,
                             g_scope_symbol_relocation.retiredSize);
    }
    for (TZrSize index = 0U; index < context->visibleSymbolFacts.length; index++) {
        const SZrSemanticVisibleSymbolFact *fact =
                (const SZrSemanticVisibleSymbolFact *)ZrCore_Array_Get(
                        &context->visibleSymbolFacts, index);
        if (fact->symbolId == fieldId) {
            fieldOwner = fact->ownerSymbolId;
        } else if (fact->symbolId == parameterId) {
            parameterOwner = fact->ownerSymbolId;
        }
    }
    for (TZrSize index = 0U; hasBody && index < context->scopeFacts.length; index++) {
        const SZrSemanticScopeFact *scope = (const SZrSemanticScopeFact *)ZrCore_Array_Get(
                &context->scopeFacts, index);
        if (scope->kind == ZR_SEMANTIC_SCOPE_KIND_BLOCK &&
            scope->range.start.offset == body->location.start.offset) {
            bodyOwner = scope->ownerSymbolId;
        }
    }
    ZrParser_SemanticContext_Free(context);
    ZrParser_Ast_Free(g_state, ast);

    TEST_ASSERT_TRUE(built);
    TEST_ASSERT_EQUAL_UINT32(1U, g_scope_symbol_relocation.relocations);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ownerId, fieldOwner, "Field lost its type owner during symbol growth");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(methodId, parameterOwner, "Parameter lost its method owner during symbol growth");
    if (hasBody) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(methodId, bodyOwner, "Body scope lost its method owner during symbol growth");
    }
}

static void test_scope_facts_preserve_generic_class_owner_after_symbol_growth(void) {
    scope_symbol_check_owner_after_growth(
            "class Container<T> { var field: int; fn echo(value: int): int { return value; } }");
}

static void test_scope_facts_preserve_generic_struct_owner_after_symbol_growth(void) {
    scope_symbol_check_owner_after_growth(
            "struct Container<T> { var field: int; fn echo(value: int): int { return value; } }");
}

static void test_scope_facts_preserve_generic_interface_owner_after_symbol_growth(void) {
    scope_symbol_check_owner_after_growth(
            "interface Container<T> { var field: int; fn echo(value: int): int; }");
}

static void test_scope_facts_preserve_generic_class_method_owner_after_symbol_growth(void) {
    scope_symbol_check_owner_after_growth(
            "class Container { var field: int; fn echo<T>(value: int): int { return value; } }");
}

static void test_scope_facts_preserve_generic_struct_method_owner_after_symbol_growth(void) {
    scope_symbol_check_owner_after_growth(
            "struct Container { var field: int; fn echo<T>(value: int): int { return value; } }");
}

static void test_scope_facts_preserve_generic_interface_method_owner_after_symbol_growth(void) {
    scope_symbol_check_owner_after_growth(
            "interface Container { var field: int; fn echo<T>(value: int): int; }");
}

#endif
