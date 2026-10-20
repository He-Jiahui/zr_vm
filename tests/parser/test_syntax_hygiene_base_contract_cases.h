#ifndef ZR_TEST_SYNTAX_HYGIENE_BASE_CONTRACT_CASES_H
#define ZR_TEST_SYNTAX_HYGIENE_BASE_CONTRACT_CASES_H

/* Included after the shared actual-producer fixture in the test translation unit. */
static SZrNamedTypeBinding parsed_class_binding(TZrSize index) {
    SZrAstNodeArray *nodes = g_ast->data.script.statements;
    SZrAstNode *declaration;
    const SZrNamedTypeBinding *binding;

    TEST_ASSERT_LESS_THAN_UINT(nodes->count, index);
    declaration = nodes->nodes[index];
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, declaration->type);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            declaration->data.classDeclaration.name->name,
            declaration->data.classDeclaration.name->syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    assert_symbol(*binding, declaration);
    return *binding;
}

static void assert_base_relation_after_contracts(TZrSize derivedIndex, TZrSize baseIndex) {
    SZrNamedTypeBinding derivedBinding = parsed_class_binding(derivedIndex);
    SZrNamedTypeBinding baseBinding = parsed_class_binding(baseIndex);
    SZrAstNodeArray *nodes = g_ast->data.script.statements;

    for (TZrSize replay = 0U; replay < 2U; replay++) {
        TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
        TEST_ASSERT_FALSE(g_compiler.hasError);
        assert_published_base(derivedBinding, nodes->nodes[derivedIndex],
                baseBinding, nodes->nodes[baseIndex]);
    }
}

static void test_root_base_relation_survives_contracts(void) {
    test_root_only_base_relation();
    assert_base_relation_after_contracts(1U, 0U);
}

static void test_opaque_only_base_relation_survives_contracts(void) {
    test_opaque_only_base_relation();
    assert_base_relation_after_contracts(1U, 0U);
}

static void test_root_then_opaque_base_relation_survives_contracts(void) {
    test_root_then_opaque_selects_opaque();
    assert_base_relation_after_contracts(2U, 1U);
}

static void test_opaque_then_root_base_relation_survives_contracts(void) {
    test_opaque_then_root_selects_opaque();
    assert_base_relation_after_contracts(2U, 0U);
}

static void test_root_forward_base_materializes_in_contracts(void) {
    test_root_forward_base_preserves_deferred_relation();
    assert_base_relation_after_contracts(0U, 1U);
}

static void test_mixed_missing_alias_stays_unbound_after_contracts(void) {
    SZrNamedTypeBinding derivedBinding;

    test_mixed_site_missing_alias_cannot_capture_root_alias();
    derivedBinding = parsed_class_binding(3U);
    for (TZrSize replay = 0U; replay < 2U; replay++) {
        TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
        TEST_ASSERT_FALSE(g_compiler.hasError);
        (void)ZrParser_SemanticQuery_BaseTypesOf(
                g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
        TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
    }
}

static void test_expired_opaque_alias_keeps_published_base_contract(void) {
    SZrIdentifier *alias;

    test_opaque_alias_can_target_root_nominal_token_zero();
    alias = alias_identifier(g_ast->data.script.statements->nodes[2]);
    TEST_ASSERT_EQUAL_UINT(1U, g_compiler.typeValueAliases.length);
    compiler_truncate_type_value_aliases(&g_compiler, 0U);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeValueAliases.length);
    TEST_ASSERT_NULL(compiler_find_type_value_alias(&g_compiler, alias->name, alias->syntaxContext));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    assert_base_relation_after_contracts(3U, 0U);
}

static void test_closed_source_keeps_open_declaration_base_contract(void) {
    SZrAstNodeArray *nodes = parse_source("class Base<T> {} class Derived<T> : Base<T> {}"
            "let Chosen = Derived<int>;");
    SZrNamedTypeBinding baseBinding;
    SZrNamedTypeBinding derivedBinding;
    SZrIdentifier *alias;
    SZrAstNode *value;
    SZrTypeBinding *aliasBinding;
    SZrTypePrototypeInfo *closedSource;
    TZrBool hasClosedBaseName = ZR_FALSE;

    TEST_ASSERT_EQUAL_UINT(3U, nodes->count);
    baseBinding = publish_class(nodes->nodes[0]);
    derivedBinding = publish_class(nodes->nodes[1]);
    assert_published_base(derivedBinding, nodes->nodes[1], baseBinding, nodes->nodes[0]);
    alias = alias_identifier(nodes->nodes[2]);
    value = nodes->nodes[2]->data.variableDeclaration.value;
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE_LITERAL_EXPRESSION, value->type);
    compiler_register_type_value_alias_literal(&g_compiler, alias->name, value, alias->syntaxContext);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    aliasBinding = compiler_find_type_value_alias(&g_compiler, alias->name, alias->syntaxContext);
    TEST_ASSERT_NOT_NULL(aliasBinding);
    ZrParser_InferredType_Copy(g_state, &g_baseType, &aliasBinding->type);
    TEST_ASSERT_TRUE(ensure_generic_instance_type_prototype_from_inferred(&g_compiler, &g_baseType));
    TEST_ASSERT_FALSE(g_compiler.hasError);
    closedSource = type_inference_find_nominal_prototype(&g_compiler, &g_baseType);
    TEST_ASSERT_NOT_NULL(closedSource);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, closedSource->genericInstanceTypeId);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[1], closedSource->declarationNode);
    TEST_ASSERT_EQUAL_STRING("Derived<int>", ZrCore_String_GetNativeString(closedSource->name));
    for (TZrSize index = 0U; index < closedSource->inherits.length; index++) {
        SZrString *const *name = ZrCore_Array_Get(&closedSource->inherits, index);
        if (name != ZR_NULL && *name != ZR_NULL &&
                strcmp(ZrCore_String_GetNativeString(*name), "Base<int>") == 0) {
            hasClosedBaseName = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(hasClosedBaseName);
    assert_base_relation_after_contracts(1U, 0U);
}

static void test_alias_base_producer_persists_target_before_alias_cleanup(void) {
    SZrAstNodeArray *nodes = parse_source("class Base {} class Evidence : Base {}"
            "let Chosen = Base; class Derived : Chosen {}");
    SZrNamedTypeBinding baseBinding = register_parsed_base_alias(nodes, g_firstDefinition);
    SZrType *annotation = chosen_annotation(nodes->nodes[3]);
    SZrNamedTypeBinding derivedBinding;
    const SZrSemanticReferenceFact *fact;

    annotation->name->data.identifier.syntaxContext = g_firstDefinition;
    TEST_ASSERT_NULL(ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE));
    derivedBinding = publish_class(nodes->nodes[3]);
    assert_published_base(derivedBinding, nodes->nodes[3], baseBinding, nodes->nodes[0]);
    fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_TRUE(fact->isResolved);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId, fact->typeId);
    TEST_ASSERT_EQUAL_UINT32(baseBinding.symbolId, fact->symbolId);
    compiler_truncate_type_value_aliases(&g_compiler, 0U);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeValueAliases.length);
    for (TZrSize replay = 0U; replay < 3U; replay++) {
        if (replay > 0U) {
            TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
            TEST_ASSERT_FALSE(g_compiler.hasError);
        }
        fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE);
        TEST_ASSERT_NOT_NULL(fact);
        TEST_ASSERT_TRUE(fact->isResolved);
        TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId, fact->typeId);
        TEST_ASSERT_EQUAL_UINT32(baseBinding.symbolId, fact->symbolId);
        assert_published_base(derivedBinding, nodes->nodes[3], baseBinding, nodes->nodes[0]);
    }
}

static void test_unnamed_source_keeps_alias_base_after_alias_cleanup(void) {
    SZrAstNodeArray *nodes = parse_source_with_name("class Base {} class Evidence : Base {}"
            "let Chosen = Base; class Derived : Chosen {}", ZR_NULL);
    SZrNamedTypeBinding baseBinding = register_parsed_base_alias(nodes, g_firstDefinition);
    SZrType *annotation = chosen_annotation(nodes->nodes[3]);
    SZrNamedTypeBinding derivedBinding;

    annotation->name->data.identifier.syntaxContext = g_firstDefinition;
    TEST_ASSERT_NULL(annotation->name->location.source);
    TEST_ASSERT_TRUE(annotation->name->location.end.offset > annotation->name->location.start.offset);
    TEST_ASSERT_TRUE(annotation->name->location.start.line > 0);
    TEST_ASSERT_NULL(ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE));
    TEST_ASSERT_FALSE(ZrParser_SemanticTypeUse_Publish(
            g_compiler.semanticContext, annotation, baseBinding.typeId, ZR_TRUE));
    TEST_ASSERT_NULL(ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE));
    derivedBinding = publish_class(nodes->nodes[3]);
    assert_published_base(derivedBinding, nodes->nodes[3], baseBinding, nodes->nodes[0]);
    compiler_truncate_type_value_aliases(&g_compiler, 0U);
    TEST_ASSERT_EQUAL_UINT(0U, g_compiler.typeValueAliases.length);
    for (TZrSize replay = 0U; replay < 3U; replay++) {
        const SZrSemanticReferenceFact *fact;
        TZrSize matchingFacts = 0U;

        if (replay > 0U) {
            TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
            TEST_ASSERT_FALSE(g_compiler.hasError);
        }
        fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
                g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE);
        TEST_ASSERT_NOT_NULL(fact);
        TEST_ASSERT_TRUE(fact->isResolved);
        TEST_ASSERT_EQUAL_UINT32(baseBinding.typeId, fact->typeId);
        TEST_ASSERT_EQUAL_UINT32(baseBinding.symbolId, fact->symbolId);
        TEST_ASSERT_TRUE(ZrParser_SyntaxContext_Equal(g_firstDefinition, fact->syntaxContext));
        TEST_ASSERT_NULL(fact->range.source);
        TEST_ASSERT_EQUAL_UINT(0U, fact->range.end.offset - fact->range.start.offset);
        TEST_ASSERT_FALSE(fact->hasDefinitionRange);
        TEST_ASSERT_NULL(fact->declarationRange.source);
        TEST_ASSERT_NULL(fact->definitionRange.source);
        for (TZrSize index = 0U; index < g_compiler.semanticContext->referenceFacts.length; index++) {
            const SZrSemanticReferenceFact *candidate = ZrCore_Array_Get(
                    &g_compiler.semanticContext->referenceFacts, index);

            if (candidate != ZR_NULL && candidate->node == annotation->name &&
                candidate->kind == ZR_SEMANTIC_REFERENCE_TYPE) {
                matchingFacts++;
            }
        }
        TEST_ASSERT_EQUAL_UINT(1U, matchingFacts);
        assert_published_base(derivedBinding, nodes->nodes[3], baseBinding, nodes->nodes[0]);
    }
}

static void test_existing_interface_does_not_block_forward_base_contract(void) {
    SZrAstNodeArray *nodes = parse_source(
            "interface Known {} class Derived : Known, Forward {} class Forward {}");
    const SZrNamedTypeBinding *binding;
    SZrNamedTypeBinding knownBinding;
    SZrNamedTypeBinding derivedBinding;
    SZrNamedTypeBinding forwardBinding;
    SZrTypePrototypeInfo *knownPrototype;

    TEST_ASSERT_EQUAL_UINT(3U, nodes->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, nodes->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[1]->type);
    TEST_ASSERT_NOT_NULL(nodes->nodes[1]->data.classDeclaration.inherits);
    TEST_ASSERT_EQUAL_UINT(2U, nodes->nodes[1]->data.classDeclaration.inherits->count);
    compile_interface_declaration(&g_compiler, nodes->nodes[0]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            nodes->nodes[0]->data.interfaceDeclaration.name->name,
            nodes->nodes[0]->data.interfaceDeclaration.name->syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    knownBinding = *binding;
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], knownBinding.declarationNode);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, knownBinding.symbolId);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, knownBinding.typeId);
    assert_symbol(knownBinding, nodes->nodes[0]);
    knownPrototype = find_compiler_type_prototype(&g_compiler,
            nodes->nodes[0]->data.interfaceDeclaration.name->name);
    TEST_ASSERT_NOT_NULL(knownPrototype);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, knownPrototype->type);
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[0], knownPrototype->declarationNode);
    derivedBinding = publish_class(nodes->nodes[1]);
    assert_published_base(derivedBinding, nodes->nodes[1], knownBinding, nodes->nodes[0]);
    forwardBinding = publish_class(nodes->nodes[2]);
    for (TZrSize replay = 0U; replay < 2U; replay++) {
        TZrSize knownCount = 0U;
        TZrSize forwardCount = 0U;

        TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
        TEST_ASSERT_FALSE(g_compiler.hasError);
        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_BaseTypesOf(
                g_compiler.semanticContext, derivedBinding.typeId, &g_relations));
        TEST_ASSERT_EQUAL_UINT(2U, g_relations.length);
        for (TZrSize index = 0U; index < g_relations.length; index++) {
            const SZrParserSemanticRelationQuery *relation = ZrCore_Array_Get(&g_relations, index);

            TEST_ASSERT_NOT_NULL(relation);
            TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_BASE_TYPE, relation->kind);
            TEST_ASSERT_EQUAL_UINT32(derivedBinding.symbolId, relation->sourceSymbolId);
            TEST_ASSERT_EQUAL_UINT32(derivedBinding.typeId, relation->sourceTypeId);
            if (relation->targetSymbolId == knownBinding.symbolId) {
                TEST_ASSERT_EQUAL_UINT32(knownBinding.typeId, relation->targetTypeId);
                knownCount++;
            } else {
                TEST_ASSERT_EQUAL_UINT32(forwardBinding.symbolId, relation->targetSymbolId);
                TEST_ASSERT_EQUAL_UINT32(forwardBinding.typeId, relation->targetTypeId);
                forwardCount++;
            }
        }
        TEST_ASSERT_EQUAL_UINT(1U, knownCount);
        TEST_ASSERT_EQUAL_UINT(1U, forwardCount);
    }
}

static void test_imported_first_generic_base_stays_unpublished_after_contracts(void) {
    SZrAstNodeArray *nodes;
    SZrNamedTypeBinding derivedBinding;

    test_root_generic_selector_preserves_first_registered_import();
    nodes = g_ast->data.script.statements;
    derivedBinding = publish_class(nodes->nodes[1]);
    for (TZrSize replay = 0U; replay < 3U; replay++) {
        if (replay > 0U) {
            TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
            TEST_ASSERT_FALSE(g_compiler.hasError);
        }
        (void)ZrParser_SemanticQuery_BaseTypesOf(
                g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
        TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
    }
}

static void test_forward_generic_interface_materializes_exact_typed_contract(void) {
    SZrAstNodeArray *nodes = parse_source(
            "class Derived : Readable<int> { pub fn read(): int { return 1; } }"
            "interface Readable<T> { fn read(): T; }");
    SZrNamedTypeBinding derivedBinding;
    SZrNamedTypeBinding interfaceBinding;
    const SZrNamedTypeBinding *binding;
    SZrType *annotation;
    TZrTypeId expectedInstanceId;
    const SZrSemanticReferenceFact *fact;
    const SZrCanonicalTypeNode *canonical;

    TEST_ASSERT_EQUAL_UINT(2U, nodes->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_CLASS_DECLARATION, nodes->nodes[0]->type);
    TEST_ASSERT_EQUAL_INT(ZR_AST_INTERFACE_DECLARATION, nodes->nodes[1]->type);
    TEST_ASSERT_NOT_NULL(nodes->nodes[0]->data.classDeclaration.inherits);
    TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[0]->data.classDeclaration.inherits->count);
    TEST_ASSERT_NOT_NULL(nodes->nodes[0]->data.classDeclaration.members);
    TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[0]->data.classDeclaration.members->count);
    TEST_ASSERT_NOT_NULL(nodes->nodes[1]->data.interfaceDeclaration.members);
    TEST_ASSERT_EQUAL_UINT(1U, nodes->nodes[1]->data.interfaceDeclaration.members->count);
    TEST_ASSERT_EQUAL_INT(ZR_AST_TYPE,
            nodes->nodes[0]->data.classDeclaration.inherits->nodes[0]->type);
    annotation = &nodes->nodes[0]->data.classDeclaration.inherits->nodes[0]->data.type;
    TEST_ASSERT_NOT_NULL(annotation->name);
    TEST_ASSERT_EQUAL_INT(ZR_AST_GENERIC_TYPE, annotation->name->type);
    derivedBinding = publish_class(nodes->nodes[0]);
    (void)ZrParser_SemanticQuery_BaseTypesOf(
            g_compiler.semanticContext, derivedBinding.typeId, &g_relations);
    TEST_ASSERT_EQUAL_UINT(0U, g_relations.length);
    fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            g_compiler.semanticContext, annotation->name, ZR_SEMANTIC_REFERENCE_TYPE);
    TEST_ASSERT_NOT_NULL(fact);
    TEST_ASSERT_TRUE(fact->isResolved);
    expectedInstanceId = fact->typeId;
    canonical = ZrParser_CanonicalType_Find(g_compiler.semanticContext, expectedInstanceId);
    TEST_ASSERT_NOT_NULL(canonical);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, canonical->kind);
    compile_interface_declaration(&g_compiler, nodes->nodes[1]);
    TEST_ASSERT_FALSE(g_compiler.hasError);
    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(g_compiler.typeEnv,
            nodes->nodes[1]->data.interfaceDeclaration.name->name,
            nodes->nodes[1]->data.interfaceDeclaration.name->syntaxContext);
    TEST_ASSERT_NOT_NULL(binding);
    interfaceBinding = *binding;
    TEST_ASSERT_EQUAL_PTR(nodes->nodes[1], interfaceBinding.declarationNode);
    assert_symbol(interfaceBinding, nodes->nodes[1]);
    for (TZrSize replay = 0U; replay < 2U; replay++) {
        TZrSize targetCount = 0U;
        const SZrParserSemanticRelationQuery *implementation;

        TEST_ASSERT_TRUE(ZrParser_SemanticRelations_PublishCompilerContracts(&g_compiler));
        TEST_ASSERT_FALSE(g_compiler.hasError);
        assert_published_base(derivedBinding, nodes->nodes[0], interfaceBinding, nodes->nodes[1]);
        TEST_ASSERT_TRUE(ZrParser_SemanticQuery_ImplementationsOf(
                g_compiler.semanticContext, interfaceBinding.symbolId, ZR_NULL, &g_relations));
        TEST_ASSERT_EQUAL_UINT(1U, g_relations.length);
        implementation = ZrCore_Array_Get(&g_relations, 0U);
        TEST_ASSERT_NOT_NULL(implementation);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_RELATION_IMPLEMENTATION, implementation->kind);
        TEST_ASSERT_EQUAL_UINT32(derivedBinding.symbolId, implementation->sourceSymbolId);
        TEST_ASSERT_EQUAL_UINT32(derivedBinding.typeId, implementation->sourceTypeId);
        TEST_ASSERT_EQUAL_UINT32(interfaceBinding.symbolId, implementation->targetSymbolId);
        TEST_ASSERT_EQUAL_UINT32(interfaceBinding.typeId, implementation->targetTypeId);
        for (TZrSize index = 0U; index < g_compiler.typePrototypes.length; index++) {
            const SZrTypePrototypeInfo *prototype = ZrCore_Array_Get(&g_compiler.typePrototypes, index);
            const SZrTypeMemberInfo *member;

            if (prototype == ZR_NULL || prototype->genericInstanceTypeId != expectedInstanceId) {
                continue;
            }
            TEST_ASSERT_EQUAL_PTR(nodes->nodes[1], prototype->declarationNode);
            TEST_ASSERT_EQUAL_INT(ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, prototype->type);
            TEST_ASSERT_EQUAL_UINT(1U, prototype->members.length);
            member = ZrCore_Array_Get((SZrArray *)&prototype->members, 0U);
            TEST_ASSERT_NOT_NULL(member);
            TEST_ASSERT_EQUAL_PTR(nodes->nodes[1]->data.interfaceDeclaration.members->nodes[0],
                    member->declarationNode);
            TEST_ASSERT_EQUAL_STRING("int", ZrCore_String_GetNativeString(member->returnTypeName));
            TEST_ASSERT_TRUE(member->hasStructuredReturnType);
            TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, member->structuredReturnType.baseType);
            targetCount++;
        }
        TEST_ASSERT_EQUAL_UINT(1U, targetCount);
    }
}

#endif

