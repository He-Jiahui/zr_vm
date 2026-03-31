//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

void ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            // 脚本节点：遍历 statements 数组
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            // 处理 moduleName（如果有）
            if (script->moduleName != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, script->moduleName);
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_BLOCK: {
            // 块节点：遍历 body 数组
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            // pattern 可能是 Identifier, DestructuringPattern, 或 DestructuringArrayPattern
            SZrString *name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, varDecl->pattern);
            if (name != ZR_NULL) {
                // 推断类型（集成类型推断系统）
                SZrInferredType *typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
                if (typeInfo != ZR_NULL) {
                    if (varDecl->typeInfo != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 ZrParser_AstTypeToInferredType_Convert
                        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    } else if (varDecl->value != ZR_NULL) {
                        // 从值推断类型
                        // 使用类型推断系统
                        if (analyzer->compilerState != ZR_NULL) {
                            SZrInferredType inferredType;
                            ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
                            if (ZrParser_ExpressionType_Infer(analyzer->compilerState, varDecl->value, &inferredType)) {
                                // 复制推断类型
                                *typeInfo = inferredType;
                            } else {
                                ZrParser_InferredType_Free(state, &inferredType);
                                // TODO: 回退到简化实现
                                if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_INT64);
                                } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                                } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_STRING);
                                } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                                } else {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                                }
                            }
                        } else {
                            // TODO: 简化实现：根据字面量类型推断
                            if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_INT64);
                            } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                            } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_STRING);
                            } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                            } else {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                            }
                        }
                    } else {
                        // 默认类型
                        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_VARIABLE, name,
                                         node->location, typeInfo,
                                         varDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL &&
                    typeInfo != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterVariable(state,
                                                     analyzer->compilerState->typeEnv,
                                                     name,
                                                     typeInfo);
                }
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                          typeInfo,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
            }
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->body != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, usingStmt->body);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrArray paramTypes;
            SZrString *name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
            if (name != ZR_NULL) {
                // 推断返回类型（集成类型推断系统）
                SZrInferredType *returnType = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
                if (returnType != ZR_NULL) {
                    if (funcDecl->returnType != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 ZrParser_AstTypeToInferredType_Convert
                        ZrParser_InferredType_Init(state, returnType, ZR_VALUE_TYPE_OBJECT);
                    } else {
                        // 默认返回类型为 object
                        ZrParser_InferredType_Init(state, returnType, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                // SZrFunctionDeclaration 没有 accessModifier 成员，使用默认值
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_FUNCTION, name,
                                         node->location, returnType,
                                         ZR_ACCESS_PUBLIC, node,
                                         &symbol);
                ZrCore_Array_Construct(&paramTypes);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL &&
                    returnType != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterFunction(state,
                                                     analyzer->compilerState->typeEnv,
                                                     name,
                                                     returnType,
                                                     &paramTypes);
                }
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                          returnType,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
            }
            break;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = classDecl->name != ZR_NULL ? classDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_CLASS, name,
                                         node->location, ZR_NULL,
                                         classDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_REFERENCE);
                ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
                
                // 检查类实现的接口，验证 const 字段匹配
                if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
                    for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
                        SZrAstNode *inheritNode = classDecl->inherits->nodes[i];
                        if (inheritNode != ZR_NULL && inheritNode->type == ZR_AST_TYPE) {
                            // 查找接口定义
                            SZrType *inheritType = &inheritNode->data.type;
                            if (inheritType->name != ZR_NULL && 
                                inheritType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                                SZrString *interfaceName = inheritType->name->data.identifier.name;
                                if (interfaceName != ZR_NULL) {
                                    SZrSymbol *interfaceSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, interfaceName, ZR_NULL);
                                    if (interfaceSymbol != ZR_NULL && 
                                        interfaceSymbol->type == ZR_SYMBOL_INTERFACE &&
                                        interfaceSymbol->astNode != ZR_NULL &&
                                        interfaceSymbol->astNode->type == ZR_AST_INTERFACE_DECLARATION) {
                                        // 检查接口中的 const 字段是否在类中也标记为 const
                                        SZrInterfaceDeclaration *interfaceDecl = &interfaceSymbol->astNode->data.interfaceDeclaration;
                                        if (interfaceDecl->members != ZR_NULL) {
                                            for (TZrSize j = 0; j < interfaceDecl->members->count; j++) {
                                                SZrAstNode *interfaceMember = interfaceDecl->members->nodes[j];
                                                if (interfaceMember != ZR_NULL && 
                                                    interfaceMember->type == ZR_AST_INTERFACE_FIELD_DECLARATION) {
                                                    SZrInterfaceFieldDeclaration *interfaceField = &interfaceMember->data.interfaceFieldDeclaration;
                                                    if (interfaceField->isConst && interfaceField->name != ZR_NULL) {
                                                        SZrString *fieldName = interfaceField->name->name;
                                                        // 在类中查找对应的字段
                                                        if (classDecl->members != ZR_NULL) {
                                                            TZrBool found = ZR_FALSE;
                                                            for (TZrSize k = 0; k < classDecl->members->count; k++) {
                                                                SZrAstNode *classMember = classDecl->members->nodes[k];
                                                                if (classMember != ZR_NULL && 
                                                                    classMember->type == ZR_AST_CLASS_FIELD) {
                                                                    SZrClassField *classField = &classMember->data.classField;
                                                                    if (classField->name != ZR_NULL && 
                                                                        ZrCore_String_Equal(classField->name->name, fieldName)) {
                                                                        found = ZR_TRUE;
                                                                        // 检查类字段是否也是 const
                                                                        if (!classField->isConst) {
                                                                            TZrChar errorMsg[256];
                                                                            TZrNativeString fieldNameStr = ZrCore_String_GetNativeStringShort(fieldName);
                                                                            if (fieldNameStr != ZR_NULL) {
                                                                                snprintf(errorMsg, sizeof(errorMsg), 
                                                                                        "Interface field '%s' is const, but implementation field is not const", 
                                                                                        fieldNameStr);
                                                                            } else {
                                                                                snprintf(errorMsg, sizeof(errorMsg), 
                                                                                        "Interface field is const, but implementation field is not const");
                                                                            }
                                                                            ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                                                            ZR_DIAGNOSTIC_ERROR,
                                                                                                            classMember->location,
                                                                                                            errorMsg,
                                            "const_interface_mismatch");
                                                                        }
                                                                        break;
                                                                    }
                                                                }
                                                            }
                                                            // TODO: 如果字段未找到，也应该报告错误（字段缺失）
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (classDecl->members != ZR_NULL) {
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < classDecl->members->count; memberIndex++) {
                    SZrAstNode *classMember = classDecl->members->nodes[memberIndex];
                    if (classMember == ZR_NULL) {
                        continue;
                    }

                    if (classMember->type == ZR_AST_CLASS_FIELD) {
                        ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(state,
                                                       analyzer,
                                                       classMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD,
                                                       (TZrInt32)memberIndex);
                    } else if (classMember->type == ZR_AST_CLASS_METHOD) {
                        SZrClassMethod *method = &classMember->data.classMethod;
                        SZrString *memberName = method->name != ZR_NULL ? method->name->name : ZR_NULL;
                        SZrSymbol *memberSymbol = ZR_NULL;
                        if (memberName != ZR_NULL) {
                            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                     analyzer->symbolTable,
                                                                     ZR_SYMBOL_METHOD,
                                                                     memberName,
                                                                     classMember->location,
                                                                     ZR_NULL,
                                                                     method->access,
                                                                     classMember,
                                                                     &memberSymbol);
                            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                      memberSymbol,
                                                      ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                                      ZR_NULL,
                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, memberSymbol);
                        }
                    } else if (classMember->type == ZR_AST_CLASS_PROPERTY) {
                        SZrClassProperty *property = &classMember->data.classProperty;
                        SZrString *memberName = ZrLanguageServer_SemanticAnalyzer_GetClassPropertySymbolName(classMember);
                        SZrSymbol *memberSymbol = ZR_NULL;
                        if (memberName != ZR_NULL) {
                            ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                                                     analyzer->symbolTable,
                                                                     ZR_SYMBOL_PROPERTY,
                                                                     memberName,
                                                                     classMember->location,
                                                                     ZR_NULL,
                                                                     property->access,
                                                                     classMember,
                                                                     &memberSymbol);
                            ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                                      memberSymbol,
                                                      ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                                      ZR_NULL,
                                                      ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
                            ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, memberSymbol);
                        }
                    }
                }
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = structDecl->name != ZR_NULL ? structDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_STRUCT, name,
                                         node->location, ZR_NULL,
                                         structDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
                ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_VALUE);
            }

            if (structDecl->members != ZR_NULL) {
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < structDecl->members->count; memberIndex++) {
                    SZrAstNode *structMember = structDecl->members->nodes[memberIndex];
                    if (structMember != ZR_NULL && structMember->type == ZR_AST_STRUCT_FIELD) {
                        ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(state,
                                                       analyzer,
                                                       structMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD,
                                                       (TZrInt32)memberIndex);
                    }
                }
            }
            break;
        }
        
        default:
            // 对于其他节点类型，继续递归处理可能的子节点
            break;
    }
    
    // 递归处理子节点（根据不同节点类型访问不同的子节点数组）
    // 对于已处理的节点类型（如 SCRIPT, BLOCK），已经在 switch 中处理并返回
    // 对于其他节点类型，需要根据具体情况递归处理子节点
    // 例如：函数声明可能有 body（Block），类声明可能有 members 数组等
    // TODO: 由于这些不是顶层声明节点，暂时跳过深度递归，仅处理直接的符号定义
}

// 辅助函数：遍历 AST 收集引用
