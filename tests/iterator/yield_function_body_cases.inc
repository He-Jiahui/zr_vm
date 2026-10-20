#include "compiler/compiler_function_body.h"
#include "compiler/compiler_typed_call_binding.h"

typedef struct SZrYieldFunctionBodyFacts {
    TZrBool prepared, compiled, scopesRestored, execBcEmpty, rangesPresent;
    TZrSize counts[4];
    TZrUInt32 offsets[3];
} SZrYieldFunctionBodyFacts;

typedef struct SZrYieldOuterGraphFacts {
    TZrBool compiled, outerNamed, innerNamed, outerReturns, innerExecBcEmpty, graphValid;
} SZrYieldOuterGraphFacts;

/* Observe the actual declaration body while its owned, isolated IR is still alive. */
static SZrYieldFunctionBodyFacts observe_yield_function_body(
        const TZrChar *source, TZrBool nested) {
    static const EZrSemanticIrOpcode opcodes[] = {
            ZR_SEMANTIC_IR_YIELD_VALUE, ZR_SEMANTIC_IR_YIELD_SUSPEND,
            ZR_SEMANTIC_IR_YIELD_RESUME, ZR_SEMANTIC_IR_ITERATOR_COMPLETE};
    SZrYieldFunctionBodyFacts facts = {0};
    SZrState *state = ZrContainerTests_CreateState();
    SZrCompilerState *compiler = ZR_NULL;
    SZrAstNode *ast = ZR_NULL, *functionNode = ZR_NULL;
    SZrFunction *rootFunction = ZR_NULL;
    SZrCompilerCallableTypeScope typeScope = {0};
    SZrCompilerSemanticIrIsolation irScope = {0};
    TZrSize hintScope = 0U, scopeDepth = 0U;
    TZrBool hintScopeEntered = ZR_FALSE, functionScopeEntered = ZR_FALSE;

    if (state == ZR_NULL) {
        goto cleanup;
    }
    ast = ZrParser_Parse(state, source, strlen(source),
                        ZrCore_String_CreateFromNative(state, "yield_function_body.zr"));
    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT ||
        ast->data.script.statements == ZR_NULL || ast->data.script.statements->count != 2U) {
        goto cleanup;
    }
    compiler = ZrContainerTests_CreateCompilerState(state);
    if (compiler == ZR_NULL) {
        goto cleanup;
    }
    compiler->scriptAst = ast;
    rootFunction = ZrCore_Function_New(state);
    compiler->currentFunction = rootFunction;
    if (rootFunction == ZR_NULL) {
        goto cleanup;
    }
    ZrContainerTests_CompileTopLevelStatement(compiler, ast->data.script.statements->nodes[0]);
    functionNode = ast->data.script.statements->nodes[1];
    if (compiler->hasError || functionNode == ZR_NULL ||
        functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        goto cleanup;
    }
    if (nested) {
        SZrAstNode *body = functionNode->data.functionDeclaration.body;
        if (body == ZR_NULL || body->type != ZR_AST_BLOCK ||
            body->data.block.body == ZR_NULL || body->data.block.body->count != 1U) {
            goto cleanup;
        }
        functionNode = body->data.block.body->nodes[0];
        if (functionNode == ZR_NULL || functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
            goto cleanup;
        }
    }

    hintScope = compiler_enter_stack_slot_type_hint_scope(compiler);
    hintScopeEntered = ZR_TRUE;
    compiler->currentFunctionNode = functionNode;
    compiler->currentFunction = ZrCore_Function_New(state);
    if (compiler->currentFunction == ZR_NULL) {
        goto cleanup;
    }
    compiler->isInConstructor = ZR_FALSE;
    compiler->initializationPhase = ZR_COMPILER_INITIALIZATION_NONE;
    compiler->instructionCount = compiler->stackSlotCount = compiler->maxStackSlotCount = 0U;
    compiler->localVarCount = compiler->constantCount = compiler->closureVarCount = 0U;
    compiler->lastExpressionSlot = ZR_PARSER_SLOT_NONE;
    compiler->instructions.length = compiler->localVars.length = compiler->localBindingIdentities.length = 0U;
    compiler->constants.length = compiler->closureVars.length = compiler->closureBindingIdentities.length = 0U;
    compiler->childFunctions.length = compiler->childFunctionNameMap.length = compiler->childFunctionBindingIdentities.length = 0U;
    compiler->cachedNullConstantIndex = 0U;
    compiler->hasCachedNullConstantIndex = ZR_FALSE;
    scopeDepth = compiler->scopeStack.length;
    enter_scope(compiler);
    functionScopeEntered = (compiler->scopeStack.length == scopeDepth + 1U);
    if (!functionScopeEntered || !compiler_callable_type_scope_begin(compiler, &typeScope) ||
        !compiler_semantic_ir_isolation_begin(compiler, &irScope)) {
        goto cleanup;
    }
    if (functionNode->data.functionDeclaration.params != ZR_NULL) {
        SZrAstNodeArray *params = functionNode->data.functionDeclaration.params;
        for (TZrSize index = 0U; index < params->count; index++) {
            SZrAstNode *paramNode = params->nodes[index];
            SZrParameter *param;
            SZrInferredType paramType;
            TZrUInt32 slot;
            TZrBool registered;
            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                goto cleanup;
            }
            param = &paramNode->data.parameter;
            if (param->name == ZR_NULL || param->name->name == ZR_NULL || param->typeInfo == ZR_NULL) {
                goto cleanup;
            }
            slot = allocate_local_var_with_context(compiler, param->name->name, param->name->syntaxContext);
            if (slot == ZR_PARSER_SLOT_NONE) {
                goto cleanup;
            }
            compiler_register_typed_owner_cleanup_slot(compiler, slot, param->typeInfo);
            compiler_register_readonly_parameter_name(compiler, param, param->name->name);
            ZrParser_InferredType_Init(state, &paramType, ZR_VALUE_TYPE_OBJECT);
            if (!ZrParser_AstTypeToInferredType_Convert(compiler, param->typeInfo, &paramType)) {
                ZrParser_InferredType_Free(state, &paramType);
                goto cleanup;
            }
            registered = ZrParser_TypeEnvironment_RegisterVariableEx(
                    state, compiler->typeEnv, param->name->name, &paramType, paramNode, paramNode->location);
            ZrParser_InferredType_Free(state, &paramType);
            if (!registered || !compiler_register_typed_callable_annotation(
                        compiler, param->name->name, param->typeInfo, param->name->syntaxContext)) {
                goto cleanup;
            }
        }
    }
    facts.prepared = !compiler->hasError;
    compiler_compile_function_body(compiler, functionNode);
    facts.compiled = !compiler->hasError;
    facts.execBcEmpty = (compiler->instructions.length == 0U);
    facts.rangesPresent = semantic_ir_has_opcode(&compiler->preSemanticIr, ZR_SEMANTIC_IR_YIELD_VALUE);
    for (TZrSize index = 0U; index < 4U; index++) {
        facts.counts[index] = semantic_ir_opcode_count(&compiler->preSemanticIr, opcodes[index]);
        if (index < 3U) {
            const SZrSemanticIrInstruction *instruction =
                    semantic_ir_first_opcode(&compiler->preSemanticIr, opcodes[index]);
            if (instruction == ZR_NULL) {
                facts.rangesPresent = ZR_FALSE;
            } else {
                facts.offsets[index] = instruction->sourceRange.start.offset;
            }
        }
    }

cleanup:
    if (compiler != ZR_NULL) {
        if (functionScopeEntered) {
            exit_scope(compiler);
        }
        compiler_semantic_ir_isolation_end(compiler, &irScope);
        facts.scopesRestored = (typeScope.child != ZR_NULL &&
                                compiler_callable_type_scope_end(compiler, &typeScope));
        if (hintScopeEntered) {
            compiler_restore_stack_slot_type_hint_scope(compiler, hintScope);
        }
        if (rootFunction != ZR_NULL && compiler->currentFunction != rootFunction) {
            ZrCore_Function_Free(state, rootFunction);
        }
    }
    destroy_compilation(state, compiler, ast);
    ZrContainerTests_DestroyState(state);
    return facts;
}

static SZrYieldOuterGraphFacts observe_public_outer_yield_graph(const TZrChar *source) {
    SZrYieldOuterGraphFacts facts = {0};
    SZrState *state = ZrContainerTests_CreateState();
    SZrAstNode *ast = ZR_NULL;
    SZrFunction *function = ZR_NULL;

    if (state != ZR_NULL) {
        ast = ZrParser_Parse(state, source, strlen(source),
                            ZrCore_String_CreateFromNative(state, "yield_outer_graph.zr"));
        if (ast != ZR_NULL) {
            function = ZrParser_Compiler_Compile(state, ast);
        }
    }
    facts.compiled = (function != ZR_NULL);
    if (function != ZR_NULL) {
        facts.graphValid = ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(function);
        if (function->childFunctionLength == 1U && function->childFunctionList != ZR_NULL) {
            const SZrFunction *outer = &function->childFunctionList[0];
            facts.outerNamed = ZrContainerTests_StringEqualsCString(outer->functionName, "outer");
            if (outer->instructionsList != ZR_NULL) {
                for (TZrUInt32 index = 0U; index < outer->instructionsLength; index++) {
                    if (outer->instructionsList[index].instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        facts.outerReturns = ZR_TRUE;
                    }
                }
            }
            if (outer->childFunctionLength == 1U && outer->childFunctionList != ZR_NULL) {
                const SZrFunction *inner = &outer->childFunctionList[0];
                facts.innerNamed = ZrContainerTests_StringEqualsCString(inner->functionName, "inner");
                facts.innerExecBcEmpty = (inner->instructionsLength == 0U && inner->instructionsList == ZR_NULL);
            }
        }
        ZrCore_Function_Free(state, function);
    }
    if (ast != ZR_NULL) {
        ZrParser_Ast_Free(state, ast);
    }
    ZrContainerTests_DestroyState(state);
    return facts;
}
