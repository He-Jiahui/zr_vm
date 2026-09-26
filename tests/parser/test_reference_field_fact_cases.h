#ifndef ZR_TEST_REFERENCE_FIELD_FACT_CASES_H
#define ZR_TEST_REFERENCE_FIELD_FACT_CASES_H

static void field_fact_add_prototype(SZrCompilerState *cs, SZrAstNode *declaration,
                                      SZrString *baseName) {
    SZrTypePrototypeInfo prototype;
    SZrAstNodeArray *members = declaration->data.classDeclaration.members;
    memset(&prototype, 0, sizeof(prototype));
    prototype.name = declaration->data.classDeclaration.name->name;
    prototype.declarationNode = declaration;
    prototype.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    ZrCore_Array_Init(g_state, &prototype.members, sizeof(SZrTypeMemberInfo), 4);
    ZrCore_Array_Init(g_state, &prototype.inherits, sizeof(SZrString *), 1);
    if (baseName != ZR_NULL) {
        ZrCore_Array_Push(g_state, &prototype.inherits, &baseName);
        prototype.extendsTypeName = baseName;
    }
    for (TZrSize index = 0; members != ZR_NULL && index < members->count; index++) {
        SZrAstNode *fieldNode = members->nodes[index];
        SZrTypeMemberInfo field;
        TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_FIELD, fieldNode->type);
        memset(&field, 0, sizeof(field));
        field.memberType = fieldNode->type;
        field.name = fieldNode->data.classField.name->name;
        field.declarationNode = fieldNode;
        field.ownerTypeName = prototype.name;
        field.accessModifier = fieldNode->data.classField.access;
        field.isStatic = fieldNode->data.classField.isStatic;
        field.fieldTypeName = ZrCore_String_CreateFromNative(g_state, "int");
        ZrCore_Array_Push(g_state, &prototype.members, &field);
    }
    ZrCore_Array_Push(g_state, &cs->typePrototypes, &prototype);
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterTypeDeclaration(
            g_state, cs->typeEnv, prototype.name, declaration));
}

static SZrAstNode *field_fact_property(SZrAstNode *expression) {
    SZrAstNodeArray *members;
    if (expression->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
        expression = expression->data.assignmentExpression.left;
    }
    TEST_ASSERT_EQUAL_INT(ZR_AST_PRIMARY_EXPRESSION, expression->type);
    members = expression->data.primaryExpression.members;
    TEST_ASSERT_NOT_NULL(members);
    TEST_ASSERT_TRUE(members->count > 0);
    return members->nodes[members->count - 1]->data.memberExpression.property;
}

static void test_field_reads_and_writes_share_inherited_declaration_identity(void) {
    static const char source[] =
            "class Base { pub var value: int; pub static var total: int; }\n"
            "class Derived: Base { }\n"
            "var item: Derived;\n"
            "item.value; item.value = 2; item.value += 3;\n"
            "Derived.total; Derived.total = 4;";
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source),
            ZrCore_String_CreateFromNative(g_state, "field_identity.zr"));
    SZrAstNode *base = script_statement_at(ast, 0);
    SZrAstNode *derived = script_statement_at(ast, 1);
    SZrAstNode *variable = script_statement_at(ast, 2);
    SZrInferredType receiverType;
    SZrInferredType intType;
    TZrSymbolId fieldIds[2];
    TZrTypeId intTypeId;
    TZrSize referenceCount;

    TEST_ASSERT_NOT_NULL(ast);
    field_fact_add_prototype(cs, base, ZR_NULL);
    field_fact_add_prototype(cs, derived, base->data.classDeclaration.name->name);
    ZrParser_InferredType_Init(g_state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    receiverType.typeName = derived->data.classDeclaration.name->name;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, cs->typeEnv, variable->data.variableDeclaration.pattern->data.identifier.name,
            &receiverType, variable, variable->location));
    ZrParser_InferredType_Init(g_state, &intType, ZR_VALUE_TYPE_INT64);
    intTypeId = ZrParser_Semantic_RegisterInferredType(cs->semanticContext, &intType,
            ZR_SEMANTIC_TYPE_KIND_VALUE, ZR_NULL, ZR_NULL);
    for (TZrSize index = 0; index < 2; index++) {
        SZrAstNode *field = base->data.classDeclaration.members->nodes[index];
        fieldIds[index] = ZrParser_Semantic_RegisterSymbol(cs->semanticContext,
                field->data.classField.name->name, ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                intTypeId, ZR_SEMANTIC_ID_INVALID, field, field->data.classField.nameLocation);
    }
    for (TZrSize repeat = 0; repeat < 2; repeat++) {
        for (TZrSize index = 3; index < 8; index++) {
            SZrAstNode *expression = expression_statement_expression_at(ast, index);
            SZrAstNode *property = field_fact_property(expression);
            SZrInferredType result;
            const SZrSemanticReferenceFact *read;
            TZrSymbolId expected = fieldIds[index < 6 ? 0 : 1];
            ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
            TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expression, &result));
            TEST_ASSERT_FALSE_MESSAGE(cs->hasError, cs->errorMessage);
            read = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                    cs->semanticContext, property, ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
            TEST_ASSERT_NOT_NULL(read);
            TEST_ASSERT_TRUE(read->isResolved);
            TEST_ASSERT_EQUAL_UINT32(expected, read->symbolId);
            TEST_ASSERT_EQUAL_UINT32(intTypeId, read->typeId);
            if (expression->type == ZR_AST_ASSIGNMENT_EXPRESSION) {
                const SZrSemanticReferenceFact *write =
                        ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                                cs->semanticContext, property, ZR_SEMANTIC_REFERENCE_MEMBER_WRITE);
                TEST_ASSERT_NOT_NULL(write);
                TEST_ASSERT_TRUE(write->isResolved);
                TEST_ASSERT_EQUAL_UINT32(expected, write->symbolId);
                TEST_ASSERT_EQUAL_UINT32(read->typeId, write->typeId);
                TEST_ASSERT_EQUAL_UINT64(read->definitionRange.start.offset,
                                         write->definitionRange.start.offset);
            }
            ZrParser_InferredType_Free(g_state, &result);
        }
        if (repeat == 0) {
            referenceCount = 0;
            for (TZrSize index = 0; index < cs->semanticContext->referenceFacts.length; index++) {
                const SZrSemanticReferenceFact *fact = (const SZrSemanticReferenceFact *)
                        ZrCore_Array_Get(&cs->semanticContext->referenceFacts, index);
                if (fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
                    fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE) {
                    referenceCount++;
                    TEST_ASSERT_TRUE(fact->isResolved);
                }
            }
            TEST_ASSERT_EQUAL_UINT64(8, referenceCount);
        }
    }
    referenceCount = 0;
    for (TZrSize index = 0; index < cs->semanticContext->referenceFacts.length; index++) {
        const SZrSemanticReferenceFact *fact = (const SZrSemanticReferenceFact *)
                ZrCore_Array_Get(&cs->semanticContext->referenceFacts, index);
        if (fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS ||
            fact->kind == ZR_SEMANTIC_REFERENCE_MEMBER_WRITE) {
            referenceCount++;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(8, referenceCount);
    ZrParser_InferredType_Free(g_state, &intType);
    ZrParser_InferredType_Free(g_state, &receiverType);
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_field_identity_preserves_unavailable_exact_type(void) {
    static const char source[] =
            "class Box { pub var value; } var item: Box; item.value;";
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source),
            ZrCore_String_CreateFromNative(g_state, "unknown_field_type.zr"));
    SZrAstNode *owner = script_statement_at(ast, 0);
    SZrAstNode *field = owner->data.classDeclaration.members->nodes[0];
    SZrAstNode *variable = script_statement_at(ast, 1);
    SZrAstNode *expression = expression_statement_expression_at(ast, 2);
    SZrTypePrototypeInfo *prototype;
    SZrInferredType receiverType;
    SZrInferredType result;
    TZrSymbolId symbolId;
    const SZrSemanticReferenceFact *reference;
    TEST_ASSERT_NOT_NULL(ast);
    field_fact_add_prototype(cs, owner, ZR_NULL);
    prototype = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, 0);
    ((SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, 0))->fieldTypeName = ZR_NULL;
    symbolId = ZrParser_Semantic_RegisterSymbol(cs->semanticContext,
            field->data.classField.name->name, ZR_SEMANTIC_SYMBOL_KIND_FIELD,
            ZR_SEMANTIC_ID_INVALID, ZR_SEMANTIC_ID_INVALID, field, field->data.classField.nameLocation);
    ZrParser_InferredType_Init(g_state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    receiverType.typeName = owner->data.classDeclaration.name->name;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, cs->typeEnv, variable->data.variableDeclaration.pattern->data.identifier.name,
            &receiverType, variable, variable->location));
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expression, &result));
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext, field_fact_property(expression), ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_TRUE(reference->isResolved);
    TEST_ASSERT_EQUAL_UINT32(symbolId, reference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reference->typeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID,
            ZrParser_Semantic_FindSymbolById(cs->semanticContext, symbolId)->typeId);
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &receiverType);
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_inaccessible_field_assignment_preserves_inference_failure(void) {
    static const char source[] =
            "class Box { pri var hidden: int; } var item: Box; item.hidden = 1;";
    SZrCompilerState *cs = create_compiler_state();
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source),
            ZrCore_String_CreateFromNative(g_state, "private_field_assignment.zr"));
    SZrAstNode *owner = script_statement_at(ast, 0);
    SZrAstNode *variable = script_statement_at(ast, 1);
    SZrAstNode *assignment = expression_statement_expression_at(ast, 2);
    SZrInferredType receiverType;
    SZrInferredType result;
    TEST_ASSERT_NOT_NULL(ast);
    field_fact_add_prototype(cs, owner, ZR_NULL);
    ZrParser_InferredType_Init(g_state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    receiverType.typeName = owner->data.classDeclaration.name->name;
    TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariableEx(
            g_state, cs->typeEnv, variable->data.variableDeclaration.pattern->data.identifier.name,
            &receiverType, variable, variable->location));
    ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
    cs->suppressErrorOutput = ZR_TRUE;
    TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, assignment, &result));
    TEST_ASSERT_TRUE(cs->hasError);
    TEST_ASSERT_NULL(ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            cs->semanticContext, field_fact_property(assignment), ZR_SEMANTIC_REFERENCE_MEMBER_WRITE));
    ZrParser_InferredType_Free(g_state, &result);
    ZrParser_InferredType_Free(g_state, &receiverType);
    destroy_compiler_state(cs);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_protected_field_access_checks_the_immediate_receiver(void) {
    static const char source[] =
            "class Base { pro var value: int; pub var other: Base; }\n"
            "class Child: Base { pub var child: Child; }\n"
            "super.value; super.value = 1;\n"
            "this.child.value; this.child.value = 2;\n"
            "super.other.value; super.other.value = 3;";
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source),
            ZrCore_String_CreateFromNative(g_state, "protected_receiver.zr"));
    SZrAstNode *base;
    SZrAstNode *child;
    TEST_ASSERT_NOT_NULL(ast);
    base = script_statement_at(ast, 0);
    child = script_statement_at(ast, 1);
    for (TZrSize index = 2; index < 8; index++) {
        SZrCompilerState *cs = create_compiler_state();
        SZrAstNode *expression = expression_statement_expression_at(ast, index);
        SZrTypePrototypeInfo *prototype;
        SZrInferredType receiverType;
        SZrInferredType result;
        const SZrSemanticReferenceFact *read;
        const SZrSemanticReferenceFact *write;
        field_fact_add_prototype(cs, base, ZR_NULL);
        field_fact_add_prototype(cs, child, base->data.classDeclaration.name->name);
        prototype = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, 0);
        ((SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, 1))->fieldTypeName =
                base->data.classDeclaration.name->name;
        prototype = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, 1);
        ((SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, 0))->fieldTypeName =
                child->data.classDeclaration.name->name;
        cs->currentTypeName = child->data.classDeclaration.name->name;
        cs->currentTypeNode = child;
        cs->currentTypePrototypeInfo = prototype;
        ZrParser_InferredType_Init(g_state, &receiverType, ZR_VALUE_TYPE_OBJECT);
        receiverType.typeName = base->data.classDeclaration.name->name;
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
                g_state, cs->typeEnv, ZrCore_String_CreateFromNative(g_state, "super"), &receiverType));
        receiverType.typeName = child->data.classDeclaration.name->name;
        TEST_ASSERT_TRUE(ZrParser_TypeEnvironment_RegisterVariable(
                g_state, cs->typeEnv, ZrCore_String_CreateFromNative(g_state, "this"), &receiverType));
        ZrParser_InferredType_Init(g_state, &result, ZR_VALUE_TYPE_OBJECT);
        cs->suppressErrorOutput = ZR_TRUE;
        if (index < 6) {
            TEST_ASSERT_TRUE(ZrParser_ExpressionType_Infer(cs, expression, &result));
            TEST_ASSERT_FALSE_MESSAGE(cs->hasError, cs->errorMessage);
        } else {
            TEST_ASSERT_FALSE(ZrParser_ExpressionType_Infer(cs, expression, &result));
            TEST_ASSERT_TRUE(cs->hasError);
        }
        read = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                cs->semanticContext, field_fact_property(expression), ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS);
        write = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                cs->semanticContext, field_fact_property(expression), ZR_SEMANTIC_REFERENCE_MEMBER_WRITE);
        if (index < 6) {
            TEST_ASSERT_NOT_NULL(read);
            TEST_ASSERT_TRUE(read->isResolved);
            TEST_ASSERT_EQUAL_PTR(base->data.classDeclaration.members->nodes[0],
                    ZrParser_Semantic_FindSymbolById(cs->semanticContext, read->symbolId)->astNode);
            if (index % 2 == 1) {
                TEST_ASSERT_NOT_NULL(write);
                TEST_ASSERT_TRUE(write->isResolved);
                TEST_ASSERT_EQUAL_UINT32(read->symbolId, write->symbolId);
            }
        } else {
            TEST_ASSERT_TRUE(read == ZR_NULL || !read->isResolved);
            TEST_ASSERT_TRUE(write == ZR_NULL || !write->isResolved);
        }
        ZrParser_InferredType_Free(g_state, &result);
        ZrParser_InferredType_Free(g_state, &receiverType);
        destroy_compiler_state(cs);
    }
    ZrParser_Ast_Free(g_state, ast);
}

#endif
