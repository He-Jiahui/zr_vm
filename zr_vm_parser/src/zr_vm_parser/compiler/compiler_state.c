//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "compiler_type_prototype_lifecycle.h"

#include "zr_vm_library/native_registry.h"
#include "compiler/tool/compile_tool_project_provider.h"
#include "compiler/compile_time/comptime_runtime_contract.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_core/gc.h"

SZrFunction *compiler_new_function(SZrCompilerState *cs) {
    SZrGcRootLease root = {ZR_NULL};
    SZrFunction *function = ZrCore_Function_New(cs->state);
    if (function == ZR_NULL) return ZR_NULL;
    if (!ZrCore_Gc_RootLeaseAcquire(cs->state, &function->super, &root)) {
        ZrCore_Function_Free(cs->state, function);
        return ZR_NULL;
    }
    ZrCore_Array_Push(cs->state, &cs->functionRoots, &root);
    return function;
}

static void compiler_free_type_environment_chain(SZrState *state, SZrTypeEnvironment *env) {
    while (state != ZR_NULL && env != ZR_NULL) {
        SZrTypeEnvironment *parent = env->parent;
        env->parent = ZR_NULL;
        ZrParser_TypeEnvironment_Free(state, env);
        env = parent;
    }
}

void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state) {
    ZR_ASSERT(cs != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);

    cs->state = state;
    (void)ZrLibrary_NativeRegistry_Attach(state->global);
    cs->currentFunction = ZR_NULL;
    cs->currentAst = ZR_NULL;
    cs->expressionTypeCache = ZR_NULL;
    ZrCore_Array_Init(state, &cs->functionRoots, sizeof(SZrGcRootLease), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->semanticContext = ZrParser_SemanticContext_New(state);
    cs->hirModule = ZR_NULL;
    compiler_semantic_ir_init(cs);

    // 初始化常量池
    ZrCore_Array_Init(state, &cs->constants, sizeof(SZrTypeValue), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    cs->constantCount = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;

    // 初始化局部变量数组
    ZrCore_Array_Init(state, &cs->localVars, sizeof(SZrFunctionLocalVariable), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    ZrCore_Array_Init(state,
                      &cs->localBindingIdentities,
                      sizeof(SZrCompilerBindingIdentity),
                      ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    cs->localVarCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;
    cs->lastExpressionSlot = ZR_PARSER_SLOT_NONE;
    ZrCore_Array_Init(state,
                      &cs->stackSlotTypeHints,
                      sizeof(SZrCompilerStackSlotTypeHint),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->stackSlotTypeHintScopeStart = 0;

    // 初始化闭包变量数组
    ZrCore_Array_Init(state, &cs->closureVars, sizeof(SZrFunctionClosureVariable), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->closureBindingIdentities,
                      sizeof(SZrCompilerBindingIdentity),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->closureVarCount = 0;
    cs->submissionContext = ZR_NULL;
    cs->submissionEntryFunction = ZR_NULL;
    ZrCore_Array_Init(state,
                      &cs->submissionDeclaredCaptureIndices,
                      sizeof(TZrUInt32),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);

    // 初始化指令数组
    ZrCore_Array_Init(state, &cs->instructions, sizeof(TZrInstruction), ZR_PARSER_INSTRUCTION_INITIAL_CAPACITY);
    cs->instructionCount = 0;

    // 初始化作用域栈
    ZrCore_Array_Init(state, &cs->scopeStack, sizeof(SZrScope), ZR_PARSER_INITIAL_CAPACITY_SMALL);

    // 初始化标签数组
    ZrCore_Array_Init(state, &cs->labels, sizeof(SZrLabel), ZR_PARSER_INITIAL_CAPACITY_SMALL);

    // 初始化待解析跳转数组
    ZrCore_Array_Init(state, &cs->pendingJumps, sizeof(SZrPendingJump), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->pendingAbsolutePatches,
                      sizeof(SZrPendingAbsolutePatch),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);

    // 初始化循环标签栈
    ZrCore_Array_Init(state, &cs->loopLabelStack, sizeof(SZrLoopLabel), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &cs->tryContextStack, sizeof(SZrCompilerTryContext), ZR_PARSER_INITIAL_CAPACITY_TINY);

    // 初始化调试与异常元数据
    ZrCore_Array_Init(state,
                      &cs->executionLocations,
                      sizeof(SZrFunctionExecutionLocationInfo),
                      ZR_PARSER_INITIAL_CAPACITY_LARGE);
    ZrCore_Array_Init(state,
                      &cs->catchClauseInfos,
                      sizeof(SZrCompilerCatchClauseInfo),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->exceptionHandlerInfos,
                      sizeof(SZrCompilerExceptionHandlerInfo),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);

    // 初始化子函数数组
    ZrCore_Array_Init(state, &cs->childFunctions, sizeof(SZrFunction *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->emitTestManifest = ZR_FALSE;
    ZrCore_Array_Init(state,
                      &cs->testManifestEntries,
                      sizeof(SZrParserTestEntry),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    
    // 初始化函数名到子函数索引的映射数组（仅用于编译时查找）
    ZrCore_Array_Init(state,
                      &cs->childFunctionNameMap,
                      sizeof(SZrChildFunctionNameMap),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->childFunctionBindingIdentities,
                      sizeof(SZrCompilerBindingIdentity),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);

    // 初始化顶层函数声明
    cs->topLevelFunction = ZR_NULL;

    // 初始化错误状态
    cs->hasError = ZR_FALSE;
    cs->hadRecoverableError = ZR_FALSE;
    cs->hasFatalError = ZR_FALSE;
    cs->hasCompileTimeError = ZR_FALSE;
    cs->suppressErrorOutput = ZR_FALSE;
    cs->errorMessage = ZR_NULL;
    cs->errorMessageStorage = ZR_NULL;
    cs->errorMessageStorageCapacity = 0;
    memset(&cs->errorLocation, 0, sizeof(cs->errorLocation));
    cs->hasStructuredError = ZR_FALSE;
    ZrParser_StructuredDiagnostic_Init(&cs->structuredError);

    // 初始化尾调用上下文
    cs->isInTailCallContext = ZR_FALSE;
    
    // 初始化外部变量引用数组
    ZrCore_Array_Init(state,
                      &cs->referencedExternalVars,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    
    // 初始化类型环境
    cs->typeEnv = ZrParser_TypeEnvironment_New(state);
    if (cs->typeEnv != ZR_NULL) {
        cs->typeEnv->semanticContext = cs->semanticContext;
    }
    ZrCore_Array_Init(state, &cs->typeEnvStack, sizeof(SZrTypeEnvironment *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    
    // 初始化模块导出跟踪数组
    ZrCore_Array_Init(state, &cs->pubVariables, sizeof(SZrExportedVariable), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state, &cs->proVariables, sizeof(SZrExportedVariable), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->exportedTypes,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);  // TODO: 暂时存储类型名
    
    // 初始化脚本 AST 引用
    cs->scriptAst = ZR_NULL;
    cs->currentModuleKey = ZR_NULL;
    
    // 初始化脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
    
    // 初始化类型 Prototype 信息数组
    ZrCore_Array_Init(state, &cs->typePrototypes, sizeof(SZrTypePrototypeInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->signatureCompiledInterfaceNodes,
                      sizeof(SZrAstNode *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    cs->currentTypePrototypeInfo = ZR_NULL;
    cs->externBindingsPredeclared = ZR_FALSE;
    
    // 初始化编译期环境
    cs->compileTimeTypeEnv = ZrParser_TypeEnvironment_New(state);
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        cs->compileTimeTypeEnv->semanticContext = ZR_NULL;
    }
    ZrCore_Array_Init(state,
                      &cs->compileTimeVariables,
                      sizeof(SZrCompileTimeVariable*),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->compileTimeFunctions,
                      sizeof(SZrCompileTimeFunction*),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->importedCompileTimeModules,
                      sizeof(SZrImportedCompileTimeModule *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state,
                      &cs->importedCompileTimeModuleAliases,
                      sizeof(SZrImportedCompileTimeModuleAlias),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    cs->activeImportedCompileTimeModule = ZR_NULL;
    ZrCore_Array_Init(state,
                      &cs->typeValueAliases,
                      sizeof(SZrTypeBinding),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state,
                      &cs->compileToolBindings,
                      sizeof(SZrCompileToolBinding),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state,
                      &cs->attributeSchemas,
                      sizeof(SZrCompilerAttributeSchemaBinding),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    cs->compilePhase = ZR_PARSER_COMPILE_PHASE_BUILD_FACTS;
    ZrParser_ComptimeRuntime_Init(cs);
    cs->isInCompileTimeContext = ZR_FALSE;
    cs->isCompilingCompileTimeRuntimeSupport = ZR_FALSE;
    
    // 初始化构造函数上下文
    cs->isInConstructor = ZR_FALSE;
    cs->initializationPhase = ZR_COMPILER_INITIALIZATION_NONE;
    cs->currentFunctionNode = ZR_NULL;
    cs->currentFunctionReceiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    cs->preservePropertyReferenceResult = ZR_FALSE;
    cs->currentTypeName = ZR_NULL;
    cs->currentTypeNode = ZR_NULL;
    
    // 初始化 const 变量跟踪数组
    ZrCore_Array_Init(state, &cs->constLocalVars, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state, &cs->constParameters, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->constructorInitializedConstFields,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->compileToolProviderParent = ZR_NULL;
    ZrCore_Array_Init(state,
                      &cs->ownedCompileToolProviders,
                      sizeof(SZrCompileToolProjectProvider *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
}

// 清理解译器状态
void ZrParser_CompilerState_Free(SZrCompilerState *cs) {
    if (cs == ZR_NULL) {
        return;
    }

    SZrState *state = cs->state;
    if (state == ZR_NULL) {
        return;
    }

    // 释放常量池（注意：常量值可能包含 GC 对象，但由 SZrFunction 管理）
    if (cs->constants.isValid && cs->constants.head != ZR_NULL && cs->constants.capacity > 0 &&
        cs->constants.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->constants);
    }

    // 释放局部变量数组
    if (cs->localVars.isValid && cs->localVars.head != ZR_NULL && cs->localVars.capacity > 0 &&
        cs->localVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->localVars);
    }
    if (cs->localBindingIdentities.isValid &&
        cs->localBindingIdentities.head != ZR_NULL &&
        cs->localBindingIdentities.capacity > 0 &&
        cs->localBindingIdentities.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->localBindingIdentities);
    }

    if (cs->stackSlotTypeHints.isValid && cs->stackSlotTypeHints.head != ZR_NULL &&
        cs->stackSlotTypeHints.capacity > 0 && cs->stackSlotTypeHints.elementSize > 0) {
        compiler_clear_stack_slot_type_hints_from(cs, 0);
        ZrCore_Array_Free(state, &cs->stackSlotTypeHints);
    }

    // 释放闭包变量数组
    if (cs->closureVars.isValid && cs->closureVars.head != ZR_NULL && cs->closureVars.capacity > 0 &&
        cs->closureVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->closureVars);
    }
    if (cs->closureBindingIdentities.isValid &&
        cs->closureBindingIdentities.head != ZR_NULL &&
        cs->closureBindingIdentities.capacity > 0 &&
        cs->closureBindingIdentities.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->closureBindingIdentities);
    }

    if (cs->submissionDeclaredCaptureIndices.isValid &&
        cs->submissionDeclaredCaptureIndices.head != ZR_NULL &&
        cs->submissionDeclaredCaptureIndices.capacity > 0 &&
        cs->submissionDeclaredCaptureIndices.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->submissionDeclaredCaptureIndices);
    }

    // 释放指令数组
    if (cs->instructions.isValid && cs->instructions.head != ZR_NULL && cs->instructions.capacity > 0 &&
        cs->instructions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->instructions);
    }

    // 释放作用域栈
    if (cs->scopeStack.isValid && cs->scopeStack.head != ZR_NULL && cs->scopeStack.capacity > 0 &&
        cs->scopeStack.elementSize > 0) {
        for (TZrSize index = 0; index < cs->scopeStack.length; index++) {
            SZrScope *scope = (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, index);
            if (scope != ZR_NULL) {
                ZrCore_Array_Free(state, &scope->cleanupRegistrations);
            }
        }
        ZrCore_Array_Free(state, &cs->scopeStack);
    }

    // 释放标签数组
    if (cs->labels.isValid && cs->labels.head != ZR_NULL && cs->labels.capacity > 0 && cs->labels.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->labels);
    }

    // 释放待解析跳转数组
    if (cs->pendingJumps.isValid && cs->pendingJumps.head != ZR_NULL && cs->pendingJumps.capacity > 0 &&
        cs->pendingJumps.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->pendingJumps);
    }

    if (cs->pendingAbsolutePatches.isValid && cs->pendingAbsolutePatches.head != ZR_NULL &&
        cs->pendingAbsolutePatches.capacity > 0 && cs->pendingAbsolutePatches.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->pendingAbsolutePatches);
    }

    // 释放循环标签栈
    if (cs->loopLabelStack.isValid && cs->loopLabelStack.head != ZR_NULL && cs->loopLabelStack.capacity > 0 &&
        cs->loopLabelStack.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->loopLabelStack);
    }

    if (cs->tryContextStack.isValid && cs->tryContextStack.head != ZR_NULL && cs->tryContextStack.capacity > 0 &&
        cs->tryContextStack.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->tryContextStack);
    }

    if (cs->executionLocations.isValid && cs->executionLocations.head != ZR_NULL &&
        cs->executionLocations.capacity > 0 && cs->executionLocations.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->executionLocations);
    }

    if (cs->catchClauseInfos.isValid && cs->catchClauseInfos.head != ZR_NULL &&
        cs->catchClauseInfos.capacity > 0 && cs->catchClauseInfos.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->catchClauseInfos);
    }

    if (cs->exceptionHandlerInfos.isValid && cs->exceptionHandlerInfos.head != ZR_NULL &&
        cs->exceptionHandlerInfos.capacity > 0 && cs->exceptionHandlerInfos.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->exceptionHandlerInfos);
    }

    // 释放子函数数组（函数本身由 GC 管理）
    if (cs->childFunctions.isValid && cs->childFunctions.head != ZR_NULL && cs->childFunctions.capacity > 0 &&
        cs->childFunctions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->childFunctions);
    }
    if (cs->childFunctionNameMap.isValid && cs->childFunctionNameMap.head != ZR_NULL &&
        cs->childFunctionNameMap.capacity > 0 && cs->childFunctionNameMap.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->childFunctionNameMap);
    }
    if (cs->childFunctionBindingIdentities.isValid &&
        cs->childFunctionBindingIdentities.head != ZR_NULL &&
        cs->childFunctionBindingIdentities.capacity > 0 &&
        cs->childFunctionBindingIdentities.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->childFunctionBindingIdentities);
    }
    compiler_test_free_entries(cs);
    
    // 释放外部变量引用数组（字符串本身由 GC 管理）
    if (cs->referencedExternalVars.isValid && cs->referencedExternalVars.head != ZR_NULL && 
        cs->referencedExternalVars.capacity > 0 && cs->referencedExternalVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->referencedExternalVars);
    }
    
    // 释放类型环境栈
    if (cs->typeEnvStack.isValid && cs->typeEnvStack.head != ZR_NULL && 
        cs->typeEnvStack.capacity > 0 && cs->typeEnvStack.elementSize > 0) {
        /* typeEnvStack stores borrowed parent links; cs->typeEnv owns the active chain. */
        ZrCore_Array_Free(state, &cs->typeEnvStack);
    }
    // 释放当前类型环境
    if (cs->typeEnv != ZR_NULL) {
        compiler_free_type_environment_chain(state, cs->typeEnv);
        cs->typeEnv = ZR_NULL;
    }

    if (cs->hirModule != ZR_NULL) {
        ZrParser_HirModule_Free(state, cs->hirModule);
        cs->hirModule = ZR_NULL;
    }
    
    // 释放类型 Prototype 信息数组
    if (cs->typePrototypes.isValid && cs->typePrototypes.head != ZR_NULL &&
        cs->typePrototypes.capacity > 0 && cs->typePrototypes.elementSize > 0) {
        // 释放每个 prototype 信息中的嵌套数组
        for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
            SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
            if (info != ZR_NULL) {
                compiler_type_prototype_free_owned_metadata(state, info);
            }
        }
        ZrCore_Array_Free(state, &cs->typePrototypes);
    }
    if (cs->signatureCompiledInterfaceNodes.isValid &&
        cs->signatureCompiledInterfaceNodes.head != ZR_NULL) {
        ZrCore_Array_Free(state, &cs->signatureCompiledInterfaceNodes);
    }
    
    // 释放模块导出跟踪数组（字符串本身由 GC 管理）
    if (cs->pubVariables.isValid && cs->pubVariables.head != ZR_NULL && 
        cs->pubVariables.capacity > 0 && cs->pubVariables.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->pubVariables);
    }
    if (cs->proVariables.isValid && cs->proVariables.head != ZR_NULL && 
        cs->proVariables.capacity > 0 && cs->proVariables.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->proVariables);
    }
    if (cs->exportedTypes.isValid && cs->exportedTypes.head != ZR_NULL && 
        cs->exportedTypes.capacity > 0 && cs->exportedTypes.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->exportedTypes);
    }
    
    // 释放编译期变量表
    if (cs->compileTimeVariables.isValid && cs->compileTimeVariables.head != ZR_NULL && 
        cs->compileTimeVariables.capacity > 0 && cs->compileTimeVariables.elementSize > 0) {
        // 释放每个编译期变量及其类型
        for (TZrSize i = 0; i < cs->compileTimeVariables.length; i++) {
            SZrCompileTimeVariable **varPtr = (SZrCompileTimeVariable**)ZrCore_Array_Get(&cs->compileTimeVariables, i);
            if (varPtr != ZR_NULL && *varPtr != ZR_NULL) {
                SZrCompileTimeVariable *var = *varPtr;
                // 释放类型（包括嵌套的elementTypes）
                ZrParser_InferredType_Free(state, &var->type);
                if (var->hasEvaluatedValueRoot) {
                    ZrCore_GcRootHandle_Release(
                            state, &var->evaluatedValueRoot);
                    var->hasEvaluatedValueRoot = ZR_FALSE;
                }
                // 释放变量结构体本身（字符串和AST节点由GC管理）
                ZrCore_Memory_RawFreeWithType(state->global, var, sizeof(SZrCompileTimeVariable), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeVariables);
    }

    if (cs->typeValueAliases.isValid && cs->typeValueAliases.head != ZR_NULL &&
        cs->typeValueAliases.capacity > 0 && cs->typeValueAliases.elementSize > 0) {
        for (TZrSize i = 0; i < cs->typeValueAliases.length; i++) {
            SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, i);
            if (binding != ZR_NULL) {
                ZrParser_InferredType_Free(state, &binding->type);
            }
        }
        ZrCore_Array_Free(state, &cs->typeValueAliases);
    }
    // 释放 const 变量跟踪数组
    if (cs->constLocalVars.isValid && cs->constLocalVars.head != ZR_NULL && 
        cs->constLocalVars.capacity > 0 && cs->constLocalVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->constLocalVars);
    }
    if (cs->constParameters.isValid && cs->constParameters.head != ZR_NULL && 
        cs->constParameters.capacity > 0 && cs->constParameters.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->constParameters);
    }
    if (cs->constructorInitializedConstFields.isValid &&
        cs->constructorInitializedConstFields.head != ZR_NULL &&
        cs->constructorInitializedConstFields.capacity > 0 &&
        cs->constructorInitializedConstFields.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->constructorInitializedConstFields);
    }
    
    // 释放编译期函数表
    if (cs->compileTimeFunctions.isValid && cs->compileTimeFunctions.head != ZR_NULL && 
        cs->compileTimeFunctions.capacity > 0 && cs->compileTimeFunctions.elementSize > 0) {
        // 释放每个编译期函数及其类型信息
        for (TZrSize i = 0; i < cs->compileTimeFunctions.length; i++) {
            SZrCompileTimeFunction **funcPtr = (SZrCompileTimeFunction**)ZrCore_Array_Get(&cs->compileTimeFunctions, i);
            if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL) {
                SZrCompileTimeFunction *func = *funcPtr;
                // 释放返回类型（包括嵌套的elementTypes）
                ZrParser_InferredType_Free(state, &func->returnType);
                // 释放参数类型数组中的每个类型
                if (func->paramTypes.isValid && func->paramTypes.head != ZR_NULL && 
                    func->paramTypes.capacity > 0 && func->paramTypes.elementSize > 0) {
                    for (TZrSize j = 0; j < func->paramTypes.length; j++) {
                        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, j);
                        if (paramType != ZR_NULL) {
                            ZrParser_InferredType_Free(state, paramType);
                        }
                    }
                    ZrCore_Array_Free(state, &func->paramTypes);
                }
                if (func->paramNames.isValid &&
                    func->paramNames.head != ZR_NULL &&
                    func->paramNames.capacity > 0 &&
                    func->paramNames.elementSize > 0) {
                    ZrCore_Array_Free(state, &func->paramNames);
                }
                if (func->paramHasDefaultValues.isValid &&
                    func->paramHasDefaultValues.head != ZR_NULL &&
                    func->paramHasDefaultValues.capacity > 0 &&
                    func->paramHasDefaultValues.elementSize > 0) {
                    ZrCore_Array_Free(state, &func->paramHasDefaultValues);
                }
                if (func->paramDefaultValues.isValid &&
                    func->paramDefaultValues.head != ZR_NULL &&
                    func->paramDefaultValues.capacity > 0 &&
                    func->paramDefaultValues.elementSize > 0) {
                    ZrCore_Array_Free(state, &func->paramDefaultValues);
                }
                // 释放函数结构体本身（字符串和AST节点由GC管理）
                ZrCore_Memory_RawFreeWithType(state->global, func, sizeof(SZrCompileTimeFunction), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeFunctions);
    }
    if (cs->importedCompileTimeModuleAliases.isValid &&
        cs->importedCompileTimeModuleAliases.head != ZR_NULL &&
        cs->importedCompileTimeModuleAliases.capacity > 0 &&
        cs->importedCompileTimeModuleAliases.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->importedCompileTimeModuleAliases);
    }

    ZrParser_CompileToolProjectProvider_FreeAll(cs);
    if (cs->compileToolBindings.isValid &&
        cs->compileToolBindings.head != ZR_NULL &&
        cs->compileToolBindings.capacity > 0 &&
        cs->compileToolBindings.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->compileToolBindings);
    }
    if (cs->attributeSchemas.isValid && cs->attributeSchemas.head != ZR_NULL) {
        for (TZrSize index = 0; index < cs->attributeSchemas.length; index++) {
            SZrCompilerAttributeSchemaBinding *schema =
                    (SZrCompilerAttributeSchemaBinding *)ZrCore_Array_Get(
                            &cs->attributeSchemas, index);
            if (schema != ZR_NULL && schema->fields.isValid &&
                schema->fields.head != ZR_NULL) {
                ZrCore_Array_Free(state, &schema->fields);
            }
        }
        ZrCore_Array_Free(state, &cs->attributeSchemas);
    }
    ZrParser_ComptimeRuntime_Free(cs);

    if (cs->importedCompileTimeModules.isValid &&
        cs->importedCompileTimeModules.head != ZR_NULL &&
        cs->importedCompileTimeModules.capacity > 0 &&
        cs->importedCompileTimeModules.elementSize > 0) {
        for (TZrSize i = 0; i < cs->importedCompileTimeModules.length; i++) {
            SZrImportedCompileTimeModule **modulePtr =
                    (SZrImportedCompileTimeModule **)ZrCore_Array_Get(&cs->importedCompileTimeModules, i);
            if (modulePtr == ZR_NULL || *modulePtr == ZR_NULL) {
                continue;
            }

            SZrImportedCompileTimeModule *module = *modulePtr;

            if (module->compileTimeFunctions.isValid &&
                module->compileTimeFunctions.head != ZR_NULL &&
                module->compileTimeFunctions.capacity > 0 &&
                module->compileTimeFunctions.elementSize > 0) {
                for (TZrSize j = 0; j < module->compileTimeFunctions.length; j++) {
                    SZrCompileTimeFunction **funcPtr =
                            (SZrCompileTimeFunction **)ZrCore_Array_Get(&module->compileTimeFunctions, j);
                    if (funcPtr == ZR_NULL || *funcPtr == ZR_NULL) {
                        continue;
                    }

                    SZrCompileTimeFunction *func = *funcPtr;
                    ZrParser_InferredType_Free(state, &func->returnType);
                    if (func->paramTypes.isValid &&
                        func->paramTypes.head != ZR_NULL &&
                        func->paramTypes.capacity > 0 &&
                        func->paramTypes.elementSize > 0) {
                        for (TZrSize k = 0; k < func->paramTypes.length; k++) {
                            SZrInferredType *paramType =
                                    (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, k);
                            if (paramType != ZR_NULL) {
                                ZrParser_InferredType_Free(state, paramType);
                            }
                        }
                        ZrCore_Array_Free(state, &func->paramTypes);
                    }
                    if (func->paramNames.isValid &&
                        func->paramNames.head != ZR_NULL &&
                        func->paramNames.capacity > 0 &&
                        func->paramNames.elementSize > 0) {
                        ZrCore_Array_Free(state, &func->paramNames);
                    }
                    if (func->paramHasDefaultValues.isValid &&
                        func->paramHasDefaultValues.head != ZR_NULL &&
                        func->paramHasDefaultValues.capacity > 0 &&
                        func->paramHasDefaultValues.elementSize > 0) {
                        ZrCore_Array_Free(state, &func->paramHasDefaultValues);
                    }
                    if (func->paramDefaultValues.isValid &&
                        func->paramDefaultValues.head != ZR_NULL &&
                        func->paramDefaultValues.capacity > 0 &&
                        func->paramDefaultValues.elementSize > 0) {
                        ZrCore_Array_Free(state, &func->paramDefaultValues);
                    }
                    ZrCore_Memory_RawFreeWithType(state->global,
                                                  func,
                                                  sizeof(SZrCompileTimeFunction),
                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
                }
                ZrCore_Array_Free(state, &module->compileTimeFunctions);
            }

            if (module->compileTimeVariables.isValid &&
                module->compileTimeVariables.head != ZR_NULL &&
                module->compileTimeVariables.capacity > 0 &&
                module->compileTimeVariables.elementSize > 0) {
                for (TZrSize j = 0; j < module->compileTimeVariables.length; j++) {
                    SZrFunctionCompileTimeVariableInfo **infoPtr =
                            (SZrFunctionCompileTimeVariableInfo **)ZrCore_Array_Get(&module->compileTimeVariables, j);
                    if (infoPtr != ZR_NULL && *infoPtr != ZR_NULL) {
                        if ((*infoPtr)->pathBindings != ZR_NULL && (*infoPtr)->pathBindingCount > 0) {
                            ZrCore_Memory_RawFreeWithType(state->global,
                                                          (*infoPtr)->pathBindings,
                                                          sizeof(SZrFunctionCompileTimePathBinding) *
                                                                  (*infoPtr)->pathBindingCount,
                                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                        }
                        ZrCore_Memory_RawFreeWithType(state->global,
                                                      *infoPtr,
                                                      sizeof(SZrFunctionCompileTimeVariableInfo),
                                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                    }
                }
                ZrCore_Array_Free(state, &module->compileTimeVariables);
            }

            if (module->scriptAst != ZR_NULL) {
                ZrParser_Ast_Free(state, module->scriptAst);
                module->scriptAst = ZR_NULL;
            }

            ZrCore_Memory_RawFreeWithType(state->global,
                                          module,
                                          sizeof(SZrImportedCompileTimeModule),
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
        }
        ZrCore_Array_Free(state, &cs->importedCompileTimeModules);
    }
    
    // 释放编译期类型环境
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, cs->compileTimeTypeEnv);
        cs->compileTimeTypeEnv = ZR_NULL;
    }

    compiler_semantic_ir_free(cs);

    if (cs->semanticContext != ZR_NULL) {
        ZrParser_SemanticContext_Free(cs->semanticContext);
        cs->semanticContext = ZR_NULL;
    }

    if (cs->errorMessageStorage != ZR_NULL && cs->errorMessageStorageCapacity > 0) {
        ZrCore_Memory_RawFreeWithType(state->global,
                                      cs->errorMessageStorage,
                                      cs->errorMessageStorageCapacity,
                                      ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
        cs->errorMessageStorage = ZR_NULL;
        cs->errorMessageStorageCapacity = 0;
        cs->errorMessage = ZR_NULL;
    }

    if (cs->hasStructuredError) {
        ZrParser_StructuredDiagnostic_Free(state, &cs->structuredError);
        cs->hasStructuredError = ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->functionRoots.length; ++index) {
        SZrGcRootLease *root = ZrCore_Array_Get(&cs->functionRoots, index);
        ZrCore_Gc_RootLeaseRelease(state->global, root);
    }
    ZrCore_Array_Free(state, &cs->functionRoots);
}

// 报告编译错误
// 辅助函数：根据错误消息提供解决建议
