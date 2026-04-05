//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"
#include "zr_vm_parser/parser.h"

void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state) {
    ZR_ASSERT(cs != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);

    cs->state = state;
    cs->currentFunction = ZR_NULL;
    cs->currentAst = ZR_NULL;
    cs->semanticContext = ZrParser_SemanticContext_New(state);
    cs->hirModule = ZR_NULL;

    // 初始化常量池
    ZrCore_Array_Init(state, &cs->constants, sizeof(SZrTypeValue), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    cs->constantCount = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;

    // 初始化局部变量数组
    ZrCore_Array_Init(state, &cs->localVars, sizeof(SZrFunctionLocalVariable), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    cs->localVarCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;

    // 初始化闭包变量数组
    ZrCore_Array_Init(state, &cs->closureVars, sizeof(SZrFunctionClosureVariable), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    cs->closureVarCount = 0;

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
    
    // 初始化函数名到子函数索引的映射数组（仅用于编译时查找）
    ZrCore_Array_Init(state,
                      &cs->childFunctionNameMap,
                      sizeof(SZrChildFunctionNameMap),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);

    // 初始化顶层函数声明
    cs->topLevelFunction = ZR_NULL;

    // 初始化错误状态
    cs->hasError = ZR_FALSE;
    cs->hadRecoverableError = ZR_FALSE;
    cs->hasFatalError = ZR_FALSE;
    cs->hasCompileTimeError = ZR_FALSE;
    cs->errorMessage = ZR_NULL;
    cs->errorMessageStorage = ZR_NULL;
    cs->errorMessageStorageCapacity = 0;
    cs->errorLocation.start.line = 0;
    cs->errorLocation.start.column = 0;
    cs->errorLocation.end.line = 0;
    cs->errorLocation.end.column = 0;

    // 初始化测试模式
    cs->isTestMode = ZR_FALSE;
    ZrCore_Array_Init(state, &cs->testFunctions, sizeof(SZrFunction *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    
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
    
    // 初始化脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
    
    // 初始化类型 Prototype 信息数组
    ZrCore_Array_Init(state, &cs->typePrototypes, sizeof(SZrTypePrototypeInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
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
                      &cs->compileTimeDecoratorClasses,
                      sizeof(SZrCompileTimeDecoratorClass *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state,
                      &cs->importedCompileTimeModules,
                      sizeof(SZrImportedCompileTimeModule *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state,
                      &cs->importedCompileTimeModuleAliases,
                      sizeof(SZrImportedCompileTimeModuleAlias),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    cs->isInCompileTimeContext = ZR_FALSE;
    cs->isCompilingCompileTimeRuntimeSupport = ZR_FALSE;
    
    // 初始化构造函数上下文
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = ZR_NULL;
    cs->currentTypeName = ZR_NULL;
    cs->currentTypeNode = ZR_NULL;
    
    // 初始化 const 变量跟踪数组
    ZrCore_Array_Init(state, &cs->constLocalVars, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state, &cs->constParameters, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(state,
                      &cs->constructorInitializedConstFields,
                      sizeof(SZrString *),
                      ZR_PARSER_INITIAL_CAPACITY_SMALL);
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

    // 释放闭包变量数组
    if (cs->closureVars.isValid && cs->closureVars.head != ZR_NULL && cs->closureVars.capacity > 0 &&
        cs->closureVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->closureVars);
    }

    // 释放指令数组
    if (cs->instructions.isValid && cs->instructions.head != ZR_NULL && cs->instructions.capacity > 0 &&
        cs->instructions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->instructions);
    }

    // 释放作用域栈
    if (cs->scopeStack.isValid && cs->scopeStack.head != ZR_NULL && cs->scopeStack.capacity > 0 &&
        cs->scopeStack.elementSize > 0) {
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
    
    // 释放测试函数数组（函数本身由 GC 管理）
    if (cs->testFunctions.isValid && cs->testFunctions.head != ZR_NULL && cs->testFunctions.capacity > 0 &&
        cs->testFunctions.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->testFunctions);
    }
    
    // 释放外部变量引用数组（字符串本身由 GC 管理）
    if (cs->referencedExternalVars.isValid && cs->referencedExternalVars.head != ZR_NULL && 
        cs->referencedExternalVars.capacity > 0 && cs->referencedExternalVars.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->referencedExternalVars);
    }
    
    // 释放类型环境栈
    if (cs->typeEnvStack.isValid && cs->typeEnvStack.head != ZR_NULL && 
        cs->typeEnvStack.capacity > 0 && cs->typeEnvStack.elementSize > 0) {
        // 释放栈中的所有环境（从栈顶到栈底）
        for (TZrSize i = 0; i < cs->typeEnvStack.length; i++) {
            SZrTypeEnvironment **envPtr = (SZrTypeEnvironment **)ZrCore_Array_Get(&cs->typeEnvStack, i);
            if (envPtr != ZR_NULL && *envPtr != ZR_NULL) {
                ZrParser_TypeEnvironment_Free(state, *envPtr);
            }
        }
        ZrCore_Array_Free(state, &cs->typeEnvStack);
    }
    // 释放当前类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, cs->typeEnv);
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
                // 释放 inherits 数组（字符串本身由 GC 管理）
                if (info->inherits.isValid && info->inherits.head != ZR_NULL &&
                    info->inherits.capacity > 0 && info->inherits.elementSize > 0) {
                    ZrCore_Array_Free(state, &info->inherits);
                }
                if (info->implements.isValid && info->implements.head != ZR_NULL &&
                    info->implements.capacity > 0 && info->implements.elementSize > 0) {
                    ZrCore_Array_Free(state, &info->implements);
                }
                if (info->genericParameters.isValid && info->genericParameters.head != ZR_NULL &&
                    info->genericParameters.capacity > 0 && info->genericParameters.elementSize > 0) {
                    for (TZrSize genericIndex = 0; genericIndex < info->genericParameters.length; genericIndex++) {
                        SZrTypeGenericParameterInfo *genericInfo =
                                (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&info->genericParameters, genericIndex);
                        if (genericInfo != ZR_NULL &&
                            genericInfo->constraintTypeNames.isValid &&
                            genericInfo->constraintTypeNames.head != ZR_NULL &&
                            genericInfo->constraintTypeNames.capacity > 0 &&
                            genericInfo->constraintTypeNames.elementSize > 0) {
                            ZrCore_Array_Free(state, &genericInfo->constraintTypeNames);
                        }
                    }
                    ZrCore_Array_Free(state, &info->genericParameters);
                }
                if (info->decorators.isValid && info->decorators.head != ZR_NULL &&
                    info->decorators.capacity > 0 && info->decorators.elementSize > 0) {
                    ZrCore_Array_Free(state, &info->decorators);
                }
                // 释放 members 数组
                if (info->members.isValid && info->members.head != ZR_NULL &&
                    info->members.capacity > 0 && info->members.elementSize > 0) {
                    for (TZrSize memberIndex = 0; memberIndex < info->members.length; memberIndex++) {
                        SZrTypeMemberInfo *memberInfo =
                                (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, memberIndex);
                        if (memberInfo != ZR_NULL &&
                            memberInfo->parameterTypes.isValid &&
                            memberInfo->parameterTypes.head != ZR_NULL &&
                            memberInfo->parameterTypes.capacity > 0 &&
                            memberInfo->parameterTypes.elementSize > 0) {
                            for (TZrSize paramIndex = 0; paramIndex < memberInfo->parameterTypes.length; paramIndex++) {
                                SZrInferredType *paramType =
                                        (SZrInferredType *)ZrCore_Array_Get(&memberInfo->parameterTypes, paramIndex);
                                if (paramType != ZR_NULL) {
                                    ZrParser_InferredType_Free(state, paramType);
                                }
                            }
                            ZrCore_Array_Free(state, &memberInfo->parameterTypes);
                        }
                        if (memberInfo != ZR_NULL &&
                            memberInfo->parameterNames.isValid &&
                            memberInfo->parameterNames.head != ZR_NULL &&
                            memberInfo->parameterNames.capacity > 0 &&
                            memberInfo->parameterNames.elementSize > 0) {
                            ZrCore_Array_Free(state, &memberInfo->parameterNames);
                        }
                        if (memberInfo != ZR_NULL &&
                            memberInfo->parameterHasDefaultValues.isValid &&
                            memberInfo->parameterHasDefaultValues.head != ZR_NULL &&
                            memberInfo->parameterHasDefaultValues.capacity > 0 &&
                            memberInfo->parameterHasDefaultValues.elementSize > 0) {
                            ZrCore_Array_Free(state, &memberInfo->parameterHasDefaultValues);
                        }
                        if (memberInfo != ZR_NULL &&
                            memberInfo->parameterDefaultValues.isValid &&
                            memberInfo->parameterDefaultValues.head != ZR_NULL &&
                            memberInfo->parameterDefaultValues.capacity > 0 &&
                            memberInfo->parameterDefaultValues.elementSize > 0) {
                            ZrCore_Array_Free(state, &memberInfo->parameterDefaultValues);
                        }
                        if (memberInfo != ZR_NULL &&
                            memberInfo->genericParameters.isValid &&
                            memberInfo->genericParameters.head != ZR_NULL &&
                            memberInfo->genericParameters.capacity > 0 &&
                            memberInfo->genericParameters.elementSize > 0) {
                            for (TZrSize genericIndex = 0; genericIndex < memberInfo->genericParameters.length;
                                 genericIndex++) {
                                SZrTypeGenericParameterInfo *genericInfo =
                                        (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(&memberInfo->genericParameters,
                                                                                        genericIndex);
                                if (genericInfo != ZR_NULL &&
                                    genericInfo->constraintTypeNames.isValid &&
                                    genericInfo->constraintTypeNames.head != ZR_NULL &&
                                    genericInfo->constraintTypeNames.capacity > 0 &&
                                    genericInfo->constraintTypeNames.elementSize > 0) {
                                    ZrCore_Array_Free(state, &genericInfo->constraintTypeNames);
                                }
                            }
                            ZrCore_Array_Free(state, &memberInfo->genericParameters);
                        }
                        if (memberInfo != ZR_NULL &&
                            memberInfo->parameterPassingModes.isValid &&
                            memberInfo->parameterPassingModes.head != ZR_NULL &&
                            memberInfo->parameterPassingModes.capacity > 0 &&
                            memberInfo->parameterPassingModes.elementSize > 0) {
                            ZrCore_Array_Free(state, &memberInfo->parameterPassingModes);
                        }
                        if (memberInfo != ZR_NULL &&
                            memberInfo->decorators.isValid &&
                            memberInfo->decorators.head != ZR_NULL &&
                            memberInfo->decorators.capacity > 0 &&
                            memberInfo->decorators.elementSize > 0) {
                            ZrCore_Array_Free(state, &memberInfo->decorators);
                        }
                    }
                    ZrCore_Array_Free(state, &info->members);
                }
            }
        }
        ZrCore_Array_Free(state, &cs->typePrototypes);
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
                // 释放变量结构体本身（字符串和AST节点由GC管理）
                ZrCore_Memory_RawFreeWithType(state->global, var, sizeof(SZrCompileTimeVariable), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeVariables);
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
    if (cs->compileTimeDecoratorClasses.isValid &&
        cs->compileTimeDecoratorClasses.head != ZR_NULL &&
        cs->compileTimeDecoratorClasses.capacity > 0 &&
        cs->compileTimeDecoratorClasses.elementSize > 0) {
        for (TZrSize i = 0; i < cs->compileTimeDecoratorClasses.length; i++) {
            SZrCompileTimeDecoratorClass **classPtr =
                    (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get(&cs->compileTimeDecoratorClasses, i);
            if (classPtr != ZR_NULL && *classPtr != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(state->global,
                                              *classPtr,
                                              sizeof(SZrCompileTimeDecoratorClass),
                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeDecoratorClasses);
    }

    if (cs->importedCompileTimeModuleAliases.isValid &&
        cs->importedCompileTimeModuleAliases.head != ZR_NULL &&
        cs->importedCompileTimeModuleAliases.capacity > 0 &&
        cs->importedCompileTimeModuleAliases.elementSize > 0) {
        ZrCore_Array_Free(state, &cs->importedCompileTimeModuleAliases);
    }

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

            if (module->compileTimeDecoratorClasses.isValid &&
                module->compileTimeDecoratorClasses.head != ZR_NULL &&
                module->compileTimeDecoratorClasses.capacity > 0 &&
                module->compileTimeDecoratorClasses.elementSize > 0) {
                for (TZrSize j = 0; j < module->compileTimeDecoratorClasses.length; j++) {
                    SZrCompileTimeDecoratorClass **classPtr =
                            (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get(&module->compileTimeDecoratorClasses, j);
                    if (classPtr != ZR_NULL && *classPtr != ZR_NULL) {
                        ZrCore_Memory_RawFreeWithType(state->global,
                                                      *classPtr,
                                                      sizeof(SZrCompileTimeDecoratorClass),
                                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
                    }
                }
                ZrCore_Array_Free(state, &module->compileTimeDecoratorClasses);
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
}

// 报告编译错误
// 辅助函数：根据错误消息提供解决建议
