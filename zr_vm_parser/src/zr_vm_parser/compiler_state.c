//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state) {
    ZR_ASSERT(cs != ZR_NULL);
    ZR_ASSERT(state != ZR_NULL);

    cs->state = state;
    cs->currentFunction = ZR_NULL;
    cs->currentAst = ZR_NULL;
    cs->semanticContext = ZrParser_SemanticContext_New(state);
    cs->hirModule = ZR_NULL;

    // 初始化常量池
    ZrCore_Array_Init(state, &cs->constants, sizeof(SZrTypeValue), 16);
    cs->constantCount = 0;
    cs->cachedNullConstantIndex = 0;
    cs->hasCachedNullConstantIndex = ZR_FALSE;

    // 初始化局部变量数组
    ZrCore_Array_Init(state, &cs->localVars, sizeof(SZrFunctionLocalVariable), 16);
    cs->localVarCount = 0;
    cs->stackSlotCount = 0;
    cs->maxStackSlotCount = 0;

    // 初始化闭包变量数组
    ZrCore_Array_Init(state, &cs->closureVars, sizeof(SZrFunctionClosureVariable), 8);
    cs->closureVarCount = 0;

    // 初始化指令数组
    ZrCore_Array_Init(state, &cs->instructions, sizeof(TZrInstruction), 64);
    cs->instructionCount = 0;

    // 初始化作用域栈
    ZrCore_Array_Init(state, &cs->scopeStack, sizeof(SZrScope), 8);

    // 初始化标签数组
    ZrCore_Array_Init(state, &cs->labels, sizeof(SZrLabel), 8);

    // 初始化待解析跳转数组
    ZrCore_Array_Init(state, &cs->pendingJumps, sizeof(SZrPendingJump), 8);
    ZrCore_Array_Init(state, &cs->pendingAbsolutePatches, sizeof(SZrPendingAbsolutePatch), 8);

    // 初始化循环标签栈
    ZrCore_Array_Init(state, &cs->loopLabelStack, sizeof(SZrLoopLabel), 4);
    ZrCore_Array_Init(state, &cs->tryContextStack, sizeof(SZrCompilerTryContext), 4);

    // 初始化调试与异常元数据
    ZrCore_Array_Init(state, &cs->executionLocations, sizeof(SZrFunctionExecutionLocationInfo), 32);
    ZrCore_Array_Init(state, &cs->catchClauseInfos, sizeof(SZrCompilerCatchClauseInfo), 8);
    ZrCore_Array_Init(state, &cs->exceptionHandlerInfos, sizeof(SZrCompilerExceptionHandlerInfo), 4);

    // 初始化子函数数组
    ZrCore_Array_Init(state, &cs->childFunctions, sizeof(SZrFunction *), 8);
    
    // 初始化函数名到子函数索引的映射数组（仅用于编译时查找）
    ZrCore_Array_Init(state, &cs->childFunctionNameMap, sizeof(SZrChildFunctionNameMap), 8);

    // 初始化顶层函数声明
    cs->topLevelFunction = ZR_NULL;

    // 初始化错误状态
    cs->hasError = ZR_FALSE;
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
    ZrCore_Array_Init(state, &cs->testFunctions, sizeof(SZrFunction *), 8);
    
    // 初始化尾调用上下文
    cs->isInTailCallContext = ZR_FALSE;
    
    // 初始化外部变量引用数组
    ZrCore_Array_Init(state, &cs->referencedExternalVars, sizeof(SZrString *), 8);
    
    // 初始化类型环境
    cs->typeEnv = ZrParser_TypeEnvironment_New(state);
    if (cs->typeEnv != ZR_NULL) {
        cs->typeEnv->semanticContext = cs->semanticContext;
    }
    ZrCore_Array_Init(state, &cs->typeEnvStack, sizeof(SZrTypeEnvironment *), 8);
    
    // 初始化模块导出跟踪数组
    ZrCore_Array_Init(state, &cs->pubVariables, sizeof(SZrExportedVariable), 8);
    ZrCore_Array_Init(state, &cs->proVariables, sizeof(SZrExportedVariable), 8);
    ZrCore_Array_Init(state, &cs->exportedTypes, sizeof(SZrString *), 4);  // TODO: 暂时存储类型名
    
    // 初始化脚本 AST 引用
    cs->scriptAst = ZR_NULL;
    
    // 初始化脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
    
    // 初始化类型 Prototype 信息数组
    ZrCore_Array_Init(state, &cs->typePrototypes, sizeof(SZrTypePrototypeInfo), 8);
    cs->currentTypePrototypeInfo = ZR_NULL;
    cs->externBindingsPredeclared = ZR_FALSE;
    
    // 初始化编译期环境
    cs->compileTimeTypeEnv = ZrParser_TypeEnvironment_New(state);
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        cs->compileTimeTypeEnv->semanticContext = ZR_NULL;
    }
    ZrCore_Array_Init(state, &cs->compileTimeVariables, sizeof(SZrCompileTimeVariable*), 8);
    ZrCore_Array_Init(state, &cs->compileTimeFunctions, sizeof(SZrCompileTimeFunction*), 8);
    cs->isInCompileTimeContext = ZR_FALSE;
    
    // 初始化构造函数上下文
    cs->isInConstructor = ZR_FALSE;
    cs->currentFunctionNode = ZR_NULL;
    cs->currentTypeName = ZR_NULL;
    
    // 初始化 const 变量跟踪数组
    ZrCore_Array_Init(state, &cs->constLocalVars, sizeof(SZrString *), 8);
    ZrCore_Array_Init(state, &cs->constParameters, sizeof(SZrString *), 8);
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
                // 释放函数结构体本身（字符串和AST节点由GC管理）
                ZrCore_Memory_RawFreeWithType(state->global, func, sizeof(SZrCompileTimeFunction), ZR_MEMORY_NATIVE_TYPE_ARRAY);
            }
        }
        ZrCore_Array_Free(state, &cs->compileTimeFunctions);
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
