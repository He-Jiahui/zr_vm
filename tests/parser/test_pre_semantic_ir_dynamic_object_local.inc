/* An unknown dynamic source value keeps its source binding but has no
 * canonical local producer. A declared scalar is the control case. */
static SZrAstNode *compile_dynamic_object_local_prefix_fixture(
        SZrCompilerState *compiler,
        TZrNativeString path,
        const char *source,
        TZrSize statementCount) {
    SZrString *name = ZrCore_String_CreateFromNative(g_state, path);
    SZrAstNode *ast;
    TZrSize index;

    if (name == ZR_NULL) return ZR_NULL;
    ast = ZrParser_Parse(g_state, source, strlen(source), name);
    if (ast == ZR_NULL) return ZR_NULL;
    ZrParser_CompilerState_Init(compiler, g_state);
    compiler->currentAst = ast;
    compiler->scriptAst = ast;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    if (compiler->currentFunction == ZR_NULL) return ast;
    for (index = 0U; index < ast->data.script.statements->count &&
                    index < statementCount; index++) {
        ZrParser_Statement_Compile(
                compiler, ast->data.script.statements->nodes[index]);
        if (compiler->hasError) break;
    }
    return ast;
}

static SZrAstNode *compile_dynamic_object_local_fixture(
        SZrCompilerState *compiler,
        TZrNativeString path,
        const char *source) {
    return compile_dynamic_object_local_prefix_fixture(
            compiler, path, source, ZR_MAX_SIZE);
}

static void free_dynamic_object_local_fixture(SZrCompilerState *compiler,
                                              SZrAstNode *ast) {
    if (compiler->currentFunction != ZR_NULL) {
        ZrCore_Function_Free(g_state, compiler->currentFunction);
        compiler->currentFunction = ZR_NULL;
    }
    ZrParser_CompilerState_Free(compiler);
    ZrParser_Ast_Free(g_state, ast);
}

static void test_explicit_scalar_local_keeps_source_cfg_load(void) {
    static const char source[] =
            "var marker: int = 2;\n"
            "return marker;\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_dynamic_object_local_fixture(
            &compiler, "typed_marker_source_cfg.zr", source);
    const SZrTypeBinding *binding;
    const SZrSemanticIrFunction *function;
    TZrUInt32 localSlot;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_EQUAL_UINT32(2U, ast->data.script.statements->count);
    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv,
            ast->data.script.statements->nodes[0]
                    ->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    localSlot = find_local_var(&compiler, binding->name);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, localSlot);
    TEST_ASSERT_NOT_NULL(compiler_semantic_ir_find_slot(&compiler, localSlot));
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_LOAD));
    TEST_ASSERT_TRUE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_RETURN));

    free_dynamic_object_local_fixture(&compiler, ast);
}

static void test_unannotated_dynamic_object_local_read_keeps_legacy_cfg(void) {
    static const char source[] =
            "var marker = zr.__forcePoolFfiGc();\n"
            "return 42 + marker;\n";
    SZrCompilerState compiler;
    SZrAstNode *ast = compile_dynamic_object_local_fixture(
            &compiler, "dynamic_marker_source_cfg.zr", source);
    SZrAstNode *declaration;
    SZrAstNode *markerRead;
    const SZrTypeBinding *binding;
    const SZrSemanticReferenceFact *reference;
    const SZrSemanticIrFunction *function;
    TZrUInt32 localSlot;
    TZrBool hasLegacyLoad = ZR_FALSE;
    TZrSize index;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_NOT_NULL(compiler.currentFunction);
    TEST_ASSERT_EQUAL_UINT32(2U, ast->data.script.statements->count);
    declaration = ast->data.script.statements->nodes[0];
    markerRead = ast->data.script.statements->nodes[1]
                         ->data.returnStatement.expr->data.binaryExpression.right;
    TEST_ASSERT_EQUAL_INT(ZR_AST_IDENTIFIER_LITERAL, markerRead->type);
    binding = ZrParser_TypeEnvironment_FindVariableBinding(
            compiler.typeEnv,
            declaration->data.variableDeclaration.pattern->data.identifier.name);
    TEST_ASSERT_NOT_NULL(binding);
    TEST_ASSERT_TRUE(binding->hasDeclarationRange);
    TEST_ASSERT_EQUAL_INT(
            ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION,
            binding->originKind);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, binding->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, binding->typeId);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, binding->type.baseType);
    TEST_ASSERT_NULL(binding->type.typeName);
    TEST_ASSERT_EQUAL_UINT32(0U, binding->type.elementTypes.length);
    localSlot = find_local_var(&compiler, binding->name);
    TEST_ASSERT_NOT_EQUAL(ZR_PARSER_SLOT_NONE, localSlot);

    TEST_ASSERT_FALSE_MESSAGE(compiler.hasError, compiler.errorMessage);
    TEST_ASSERT_TRUE(compiler.preSemanticIrCfgStartupBlocked);
    TEST_ASSERT_FALSE(compiler.preSemanticIrCfgActive);
    reference = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            compiler.semanticContext, markerRead, ZR_SEMANTIC_REFERENCE_READ);
    TEST_ASSERT_NOT_NULL(reference);
    TEST_ASSERT_EQUAL_UINT32(binding->symbolId, reference->symbolId);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, reference->typeId);
    for (index = 0U; index < compiler.instructions.length; index++) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(
                        &compiler.instructions, index);
        TEST_ASSERT_NOT_NULL(instruction);
        if (instruction->instruction.operationCode ==
                    ZR_INSTRUCTION_ENUM(GET_STACK) &&
            instruction->instruction.operand.operand2[0] ==
                    (TZrInt32)localSlot) {
            hasLegacyLoad = ZR_TRUE;
        }
    }
    TEST_ASSERT_TRUE(hasLegacyLoad);
    TEST_ASSERT_TRUE(ZrParser_Compiler_ValidatePreSemanticIr(&compiler));
    function = ZrParser_Compiler_PreSemanticIr(&compiler);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_FALSE(semantic_ir_has_opcode(function, ZR_SEMANTIC_IR_RETURN));
    TEST_ASSERT_EQUAL_UINT32(2U, function->cfg.blocks.length);
    for (index = 0U; index < function->instructions.length; index++) {
        const SZrSemanticIrInstruction *instruction =
                ZrParser_SemanticIr_InstructionAt(function, index);
        TEST_ASSERT_NOT_NULL(instruction);
        TEST_ASSERT_FALSE(instruction->opcode == ZR_SEMANTIC_IR_LOAD &&
                          instruction->sourceRange.start.offset ==
                                  markerRead->location.start.offset);
    }

    free_dynamic_object_local_fixture(&compiler, ast);
}

typedef struct SDynamicLocalStoreResult {
    TZrBool parsedSource;
    TZrBool prefixCompiled;
    TZrBool assignmentFromSource;
    TZrBool bindingFromSource;
    TZrBool symbolFromSource;
    TZrBool indexedIdentityMatches;
    TZrBool objectSentinel;
    TZrBool canonicalInt64Binding;
    TZrTypeId bindingTypeId;
    TZrBool isolated;
    TZrBool destinationSlotMissing;
    TZrBool initialized;
    TZrBool cfgActiveBefore;
    TZrBool terminatedBefore;
    TZrBool rhsDefinedInt64Constant;
    TZrBool sourceBoolBranch;
    TZrBool lowered;
    TZrSize legacyInstructionDelta;
    TZrBool legacyStoreMatches;
    TZrBool semanticStateUnchanged;
    TZrBool destinationStillMissing;
    TZrBool typedLocalStore;
    TZrBool bindingUnchanged;
    TZrBool cfgUnchanged;
    TZrBool validated;
} SDynamicLocalStoreResult;

static TZrBool dynamic_local_store_range_matches(
        SZrFileRange left, SZrFileRange right) {
    return (TZrBool)(left.source == right.source &&
            left.start.offset == right.start.offset &&
            left.end.offset == right.end.offset &&
            left.start.line == right.start.line &&
            left.start.column == right.start.column &&
            left.end.line == right.end.line &&
            left.end.column == right.end.column);
}

static const SZrSemanticIrInstruction *dynamic_local_store_constant(
        SZrCompilerState *compiler, TZrUInt32 stackSlot,
        const SZrAstNode *node, EZrValueType primitive) {
    const SZrCompilerSemanticIrSlot *slot =
            compiler_semantic_ir_find_slot(compiler, stackSlot);
    const SZrSemanticIrValue *value;
    const SZrCanonicalTypeNode *type;
    const SZrSemanticIrInstruction *definition;

    if (slot == ZR_NULL || node == ZR_NULL) return ZR_NULL;
    value = ZrParser_SemanticIr_Value(&compiler->preSemanticIr, slot->valueId);
    type = ZrParser_CanonicalType_Find(compiler->semanticContext, slot->typeId);
    if (value == ZR_NULL || type == ZR_NULL ||
        type->kind != ZR_CANONICAL_TYPE_PRIMITIVE ||
        type->data.primitive.valueType != primitive ||
        value->typeId != slot->typeId ||
        value->definitionInstructionId == ZR_SEMANTIC_INSTRUCTION_ID_INVALID ||
        value->definitionInstructionId > compiler->preSemanticIr.instructions.length)
        return ZR_NULL;
    definition = ZrParser_SemanticIr_InstructionAt(
            &compiler->preSemanticIr, value->definitionInstructionId - 1U);
    if (definition == ZR_NULL || definition->opcode != ZR_SEMANTIC_IR_CONSTANT ||
        definition->id != value->definitionInstructionId ||
        definition->resultValueId != value->id ||
        definition->typeId != slot->typeId ||
        !definition->hasConstantPoolIndex ||
        definition->constantPoolIndex >= compiler->constants.length ||
        !dynamic_local_store_range_matches(definition->sourceRange, node->location))
        return ZR_NULL;
    return definition;
}

static SDynamicLocalStoreResult observe_dynamic_local_store(
        TZrNativeString path, const char *source, TZrBool activateCfg) {
    SDynamicLocalStoreResult result;
    SZrCompilerState compiler;
    SZrCompilerSemanticIrIsolation isolation;
    SZrAstNode *ast;
    SZrAstNode *declaration;
    SZrAstNode *pattern;
    SZrAstNode *statement;
    SZrAstNode *branch = ZR_NULL;
    SZrAstNode *assignment;
    const SZrTypeBinding *binding;
    const SZrSemanticSymbolRecord *symbol;
    const SZrCanonicalTypeNode *type;
    const SZrSemanticIrInstruction *constant;
    const SZrCompilerSemanticIrSlot *slot;
    TZrUInt32 localSlot;
    TZrUInt32 valueSlot;
    TZrValueId rhsValueId;
    TZrTypeId rhsTypeId;
    TZrSymbolId symbolId;
    TZrSize index;
    TZrSize legacyBefore;
    TZrSize instructionsBefore;
    TZrSize valuesBefore;
    TZrSize placesBefore;
    TZrSize slotsBefore;
    TZrSize blocksBefore;
    TZrUInt32 blockBefore;
    TZrUInt32 startBefore;
    TZrBool suppressedBefore;
    TZrBool blockedBefore;

    memset(&result, 0, sizeof(result));
    memset(&compiler, 0, sizeof(compiler));
    memset(&isolation, 0, sizeof(isolation));
    ast = compile_dynamic_object_local_prefix_fixture(
            &compiler, path, source, 1U);
    result.parsedSource = (TZrBool)(ast != ZR_NULL &&
            ast->type == ZR_AST_SCRIPT && ast->data.script.statements != ZR_NULL &&
            ast->data.script.statements->count == 2U);
    result.prefixCompiled = (TZrBool)(compiler.currentFunction != ZR_NULL &&
            !compiler.hasError);
    if (!result.parsedSource || !result.prefixCompiled) goto cleanup;

    declaration = ast->data.script.statements->nodes[0];
    if (declaration == ZR_NULL || declaration->type != ZR_AST_VARIABLE_DECLARATION)
        goto cleanup;
    pattern = declaration->data.variableDeclaration.pattern;
    if (pattern == ZR_NULL || pattern->type != ZR_AST_IDENTIFIER_LITERAL)
        goto cleanup;
    statement = ast->data.script.statements->nodes[1];
    if (activateCfg) {
        branch = statement;
        if (branch == ZR_NULL || branch->type != ZR_AST_IF_EXPRESSION ||
            !branch->data.ifExpression.isStatement ||
            branch->data.ifExpression.condition == ZR_NULL ||
            branch->data.ifExpression.condition->type != ZR_AST_BOOLEAN_LITERAL ||
            branch->data.ifExpression.thenExpr == ZR_NULL ||
            branch->data.ifExpression.thenExpr->type != ZR_AST_BLOCK ||
            branch->data.ifExpression.thenExpr->data.block.body == ZR_NULL ||
            branch->data.ifExpression.thenExpr->data.block.body->count != 1U)
            goto cleanup;
        statement = branch->data.ifExpression.thenExpr->data.block.body->nodes[0];
    }
    if (statement == ZR_NULL || statement->type != ZR_AST_EXPRESSION_STATEMENT)
        goto cleanup;
    assignment = statement->data.expressionStatement.expr;
    result.assignmentFromSource = (TZrBool)(assignment != ZR_NULL &&
            assignment->type == ZR_AST_ASSIGNMENT_EXPRESSION &&
            assignment->data.assignmentExpression.op.op != ZR_NULL &&
            strcmp(assignment->data.assignmentExpression.op.op, "=") == 0 &&
            assignment->data.assignmentExpression.left != ZR_NULL &&
            assignment->data.assignmentExpression.left->type == ZR_AST_IDENTIFIER_LITERAL &&
            assignment->data.assignmentExpression.right != ZR_NULL &&
            assignment->data.assignmentExpression.right->type == ZR_AST_INTEGER_LITERAL &&
            assignment->data.assignmentExpression.right->data.integerLiteral.value == 42);
    if (!result.assignmentFromSource) goto cleanup;

    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            compiler.typeEnv, pattern->data.identifier.name,
            pattern->data.identifier.syntaxContext);
    if (binding == ZR_NULL) goto cleanup;
    result.bindingTypeId = binding->typeId;
    symbolId = binding->symbolId;
    result.bindingFromSource = (TZrBool)(binding->hasDeclarationRange &&
            binding->originKind == ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION &&
            dynamic_local_store_range_matches(binding->declarationRange, pattern->location) &&
            ZrCore_String_Equal(binding->name,
                    assignment->data.assignmentExpression.left->data.identifier.name));
    symbol = ZrParser_Semantic_FindSymbolById(compiler.semanticContext, symbolId);
    result.symbolFromSource = (TZrBool)(symbol != ZR_NULL &&
            symbolId != ZR_SEMANTIC_ID_INVALID &&
            symbol->kind == ZR_SEMANTIC_SYMBOL_KIND_VARIABLE &&
            symbol->astNode == declaration && symbol->typeId == binding->typeId &&
            dynamic_local_store_range_matches(symbol->location, pattern->location));
    result.objectSentinel = (TZrBool)(binding->type.baseType == ZR_VALUE_TYPE_OBJECT &&
            binding->type.typeName == ZR_NULL && binding->type.elementTypes.length == 0U);
    type = ZrParser_CanonicalType_Find(compiler.semanticContext, binding->typeId);
    result.canonicalInt64Binding = (TZrBool)(type != ZR_NULL &&
            type->kind == ZR_CANONICAL_TYPE_PRIMITIVE &&
            type->data.primitive.valueType == ZR_VALUE_TYPE_INT64);
    localSlot = find_local_var_with_context(&compiler, pattern->data.identifier.name,
            pattern->data.identifier.syntaxContext);
    for (index = 0U; index < compiler.localVars.length; index++) {
        const SZrFunctionLocalVariable *local =
                (const SZrFunctionLocalVariable *)ZrCore_Array_Get(
                        &compiler.localVars, index);
        const SZrCompilerBindingIdentity *identity;
        if (local == ZR_NULL || local->stackSlot != localSlot ||
            local->name == ZR_NULL ||
            !ZrCore_String_Equal(local->name, pattern->data.identifier.name))
            continue;
        if (index >= compiler.localBindingIdentities.length) break;
        identity = (const SZrCompilerBindingIdentity *)ZrCore_Array_Get(
                &compiler.localBindingIdentities, index);
        result.indexedIdentityMatches = (TZrBool)(identity != ZR_NULL &&
                localSlot != ZR_PARSER_SLOT_NONE &&
                ZrParser_SyntaxContext_Equal(
                        ZrParser_SyntaxContext_Normalize(identity->syntaxContext,
                                ZR_SYNTAX_NAMESPACE_LOCAL),
                        ZrParser_SyntaxContext_Normalize(binding->syntaxContext,
                                ZR_SYNTAX_NAMESPACE_LOCAL)) &&
                ZrParser_SyntaxContext_Equal(
                        ZrParser_SyntaxContext_Normalize(identity->syntaxContext,
                                ZR_SYNTAX_NAMESPACE_LOCAL),
                        ZrParser_SyntaxContext_Normalize(
                                assignment->data.assignmentExpression.left->data.identifier.syntaxContext,
                                ZR_SYNTAX_NAMESPACE_LOCAL)));
        break;
    }
    if (!result.bindingFromSource || !result.symbolFromSource ||
        !result.indexedIdentityMatches) goto cleanup;

    result.isolated = compiler_semantic_ir_isolation_begin(&compiler, &isolation);
    if (!result.isolated) goto cleanup;
    result.destinationSlotMissing = (TZrBool)(
            compiler_semantic_ir_find_slot(&compiler, localSlot) == ZR_NULL);
    if (activateCfg) {
        const SZrSemanticIrInstruction *semanticBranch;
        const TZrValueId *condition;
        TZrValueId conditionValueId;
        TZrUInt32 thenBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        TZrUInt32 elseBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        TZrUInt32 joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;

        ZrParser_Expression_Compile(&compiler, branch->data.ifExpression.condition);
        if (compiler.hasError) goto cleanup;
        constant = dynamic_local_store_constant(&compiler, compiler.lastExpressionSlot,
                branch->data.ifExpression.condition, ZR_VALUE_TYPE_BOOL);
        if (constant == ZR_NULL) goto cleanup;
        conditionValueId = constant->resultValueId;
        if (!compiler_semantic_cfg_begin_if(&compiler, compiler.lastExpressionSlot,
                    branch, &thenBlock, &elseBlock, &joinBlock))
            goto cleanup;
        semanticBranch = ZrParser_SemanticIr_InstructionAt(
                &compiler.preSemanticIr, compiler.preSemanticIr.instructions.length - 1U);
        condition = semanticBranch != ZR_NULL && semanticBranch->operandCount == 1U
                ? (const TZrValueId *)ZrCore_Array_Get(
                        &compiler.preSemanticIr.valueOperands, semanticBranch->operandStart)
                : ZR_NULL;
        result.sourceBoolBranch = (TZrBool)(compiler.preSemanticIrCfgActive &&
                semanticBranch != ZR_NULL && semanticBranch->opcode == ZR_SEMANTIC_IR_BRANCH &&
                semanticBranch->targetBlockId == thenBlock &&
                dynamic_local_store_range_matches(semanticBranch->sourceRange, branch->location) &&
                condition != ZR_NULL && *condition == conditionValueId);
        if (!result.sourceBoolBranch) goto cleanup;
    }

    ZrParser_Expression_Compile(&compiler, assignment->data.assignmentExpression.right);
    if (compiler.hasError) goto cleanup;
    valueSlot = compiler.lastExpressionSlot;
    constant = dynamic_local_store_constant(&compiler, valueSlot,
            assignment->data.assignmentExpression.right, ZR_VALUE_TYPE_INT64);
    result.rhsDefinedInt64Constant = (TZrBool)(constant != ZR_NULL);
    if (!result.rhsDefinedInt64Constant) goto cleanup;
    rhsValueId = constant->resultValueId;
    rhsTypeId = constant->typeId;
    result.initialized = compiler.preSemanticIrInitialized;
    result.cfgActiveBefore = compiler.preSemanticIrCfgActive;
    result.terminatedBefore = compiler.preSemanticIrCfgTerminated;
    legacyBefore = compiler.instructions.length;
    instructionsBefore = compiler.preSemanticIr.instructions.length;
    valuesBefore = compiler.preSemanticIr.values.length;
    placesBefore = compiler.preSemanticIr.places.places.length;
    slotsBefore = compiler.preSemanticIrSlots.length;
    blocksBefore = compiler.preSemanticIr.cfg.blocks.length;
    blockBefore = compiler.preSemanticIrCfgBlock;
    startBefore = compiler.preSemanticIrCfgStart;
    suppressedBefore = compiler.preSemanticIrCfgStartupSuppressed;
    blockedBefore = compiler.preSemanticIrCfgStartupBlocked;

    result.lowered = compiler_semantic_ir_lower_store(
            &compiler, localSlot, valueSlot,
            assignment->data.assignmentExpression.left->location);
    result.legacyInstructionDelta = compiler.instructions.length >= legacyBefore
            ? compiler.instructions.length - legacyBefore : ZR_MAX_SIZE;
    if (result.legacyInstructionDelta == 1U) {
        const TZrInstruction *instruction =
                (const TZrInstruction *)ZrCore_Array_Get(&compiler.instructions, legacyBefore);
        result.legacyStoreMatches = (TZrBool)(instruction != ZR_NULL &&
                instruction->instruction.operationCode == ZR_INSTRUCTION_ENUM(SET_STACK) &&
                instruction->instruction.operandExtra == localSlot &&
                instruction->instruction.operand.operand2[0] == (TZrInt32)valueSlot);
    }
    result.semanticStateUnchanged = (TZrBool)(
            compiler.preSemanticIr.instructions.length == instructionsBefore &&
            compiler.preSemanticIr.values.length == valuesBefore &&
            compiler.preSemanticIr.places.places.length == placesBefore &&
            compiler.preSemanticIrSlots.length == slotsBefore);
    slot = compiler_semantic_ir_find_slot(&compiler, localSlot);
    result.destinationStillMissing = (TZrBool)(slot == ZR_NULL);
    if (slot != ZR_NULL && compiler.preSemanticIr.instructions.length > instructionsBefore) {
        const SZrParserPlace *place = ZrParser_PlaceGraph_Get(
                &compiler.preSemanticIr.places, slot->placeId);
        const SZrSemanticIrInstruction *store = ZrParser_SemanticIr_InstructionAt(
                &compiler.preSemanticIr, compiler.preSemanticIr.instructions.length - 1U);
        result.typedLocalStore = (TZrBool)(place != ZR_NULL &&
                place->base.kind == ZR_PARSER_PLACE_BASE_LOCAL &&
                place->base.identity == localSlot &&
                slot->typeId == result.bindingTypeId && slot->symbolId == symbolId &&
                slot->valueId == rhsValueId && slot->typeId == rhsTypeId &&
                store != ZR_NULL && store->opcode == ZR_SEMANTIC_IR_STORE &&
                store->placeId == slot->placeId && store->symbolId == symbolId &&
                store->typeId == rhsTypeId && store->valueId == rhsValueId &&
                dynamic_local_store_range_matches(store->sourceRange,
                        assignment->data.assignmentExpression.left->location));
    }
    binding = ZrParser_TypeEnvironment_FindVariableBindingWithContext(
            compiler.typeEnv, pattern->data.identifier.name,
            pattern->data.identifier.syntaxContext);
    result.bindingUnchanged = (TZrBool)(binding != ZR_NULL &&
            binding->typeId == result.bindingTypeId && binding->symbolId == symbolId);
    result.cfgUnchanged = (TZrBool)(
            compiler.preSemanticIrCfgActive == result.cfgActiveBefore &&
            compiler.preSemanticIrCfgTerminated == result.terminatedBefore &&
            compiler.preSemanticIrCfgStartupSuppressed == suppressedBefore &&
            compiler.preSemanticIrCfgStartupBlocked == blockedBefore &&
            compiler.preSemanticIrCfgBlock == blockBefore &&
            compiler.preSemanticIrCfgStart == startBefore &&
            compiler.preSemanticIr.cfg.blocks.length == blocksBefore);
    if (result.lowered && !activateCfg) {
        result.validated = ZrParser_Compiler_ValidatePreSemanticIr(&compiler);
    }

cleanup:
    if (compiler.state != ZR_NULL) {
        compiler_semantic_ir_isolation_end(&compiler, &isolation);
        free_dynamic_object_local_fixture(&compiler, ast);
    } else if (ast != ZR_NULL) {
        ZrParser_Ast_Free(g_state, ast);
    }
    return result;
}

static void assert_dynamic_local_store_source_preconditions(
        const SDynamicLocalStoreResult *result) {
    TEST_ASSERT_TRUE(result->parsedSource);
    TEST_ASSERT_TRUE(result->prefixCompiled);
    TEST_ASSERT_TRUE(result->assignmentFromSource);
    TEST_ASSERT_TRUE(result->bindingFromSource);
    TEST_ASSERT_TRUE(result->symbolFromSource);
    TEST_ASSERT_TRUE(result->indexedIdentityMatches);
    TEST_ASSERT_TRUE(result->isolated);
    TEST_ASSERT_TRUE(result->destinationSlotMissing);
    TEST_ASSERT_TRUE(result->initialized);
    TEST_ASSERT_FALSE(result->terminatedBefore);
    TEST_ASSERT_TRUE(result->rhsDefinedInt64Constant);
}

static void test_unannotated_dynamic_object_local_store_keeps_legacy_cfg(void) {
    static const char source[] =
            "var marker = zr.__forcePoolFfiGc();\n"
            "marker = 42;\n";
    SDynamicLocalStoreResult result = observe_dynamic_local_store(
            "dynamic_marker_store.zr", source, ZR_FALSE);

    assert_dynamic_local_store_source_preconditions(&result);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, result.bindingTypeId);
    TEST_ASSERT_TRUE(result.objectSentinel);
    TEST_ASSERT_FALSE(result.cfgActiveBefore);
    TEST_ASSERT_TRUE_MESSAGE(result.lowered,
            "an inactive source local without a canonical producer must retain its legacy store");
    TEST_ASSERT_EQUAL_UINT64(1U, result.legacyInstructionDelta);
    TEST_ASSERT_TRUE(result.legacyStoreMatches);
    TEST_ASSERT_TRUE(result.semanticStateUnchanged);
    TEST_ASSERT_TRUE(result.destinationStillMissing);
    TEST_ASSERT_TRUE(result.bindingUnchanged);
    TEST_ASSERT_TRUE(result.cfgUnchanged);
    TEST_ASSERT_TRUE(result.validated);
}

static void test_explicit_scalar_local_store_rematerializes_source_slot(void) {
    static const char source[] =
            "var marker: int = 2;\n"
            "marker = 42;\n";
    SDynamicLocalStoreResult result = observe_dynamic_local_store(
            "typed_marker_store.zr", source, ZR_FALSE);

    assert_dynamic_local_store_source_preconditions(&result);
    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, result.bindingTypeId);
    TEST_ASSERT_TRUE(result.canonicalInt64Binding);
    TEST_ASSERT_FALSE(result.cfgActiveBefore);
    TEST_ASSERT_TRUE(result.lowered);
    TEST_ASSERT_EQUAL_UINT64(1U, result.legacyInstructionDelta);
    TEST_ASSERT_TRUE(result.legacyStoreMatches);
    TEST_ASSERT_FALSE(result.destinationStillMissing);
    TEST_ASSERT_TRUE(result.typedLocalStore);
    TEST_ASSERT_TRUE(result.bindingUnchanged);
    TEST_ASSERT_TRUE(result.cfgUnchanged);
    TEST_ASSERT_TRUE(result.validated);
}

static void test_untyped_local_store_does_not_bypass_active_source_cfg(void) {
    static const char source[] =
            "var marker = zr.__forcePoolFfiGc();\n"
            "if (true) { marker = 42; }\n";
    SDynamicLocalStoreResult result = observe_dynamic_local_store(
            "dynamic_marker_active_store.zr", source, ZR_TRUE);

    assert_dynamic_local_store_source_preconditions(&result);
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, result.bindingTypeId);
    TEST_ASSERT_TRUE(result.objectSentinel);
    TEST_ASSERT_TRUE(result.sourceBoolBranch);
    TEST_ASSERT_TRUE(result.cfgActiveBefore);
    /* This is the lower API boundary; the full source language contract is unchanged. */
    TEST_ASSERT_FALSE_MESSAGE(result.lowered,
            "the untyped local legacy fallback must not admit an active source CFG");
    TEST_ASSERT_EQUAL_UINT64(0U, result.legacyInstructionDelta);
    TEST_ASSERT_TRUE(result.semanticStateUnchanged);
    TEST_ASSERT_TRUE(result.destinationStillMissing);
    TEST_ASSERT_TRUE(result.bindingUnchanged);
    TEST_ASSERT_TRUE(result.cfgUnchanged);
}
