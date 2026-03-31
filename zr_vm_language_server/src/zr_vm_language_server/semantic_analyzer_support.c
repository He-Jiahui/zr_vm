//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

SZrString *ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(SZrState *state, SZrAstNode *node) {
    if (node == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrIdentifier *identifier = &node->data.identifier;
        if (identifier != ZR_NULL && identifier->name != ZR_NULL) {
            return identifier->name;
        }
    }
    
    return ZR_NULL;
}

static TZrBool string_equals_literal(SZrString *value, const TZrChar *literal) {
    TZrNativeString text;
    TZrSize length;
    TZrSize literalLength;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        text = ZrCore_String_GetNativeStringShort(value);
        length = value->shortStringLength;
    } else {
        text = ZrCore_String_GetNativeString(value);
        length = value->longStringLength;
    }

    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    literalLength = strlen(literal);
    return length == literalLength && memcmp(text, literal, literalLength) == 0;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_IsImplicitRuntimeIdentifier(SZrString *name) {
    return string_equals_literal(name, "this") || string_equals_literal(name, "super");
}

SZrString *ZrLanguageServer_SemanticAnalyzer_GetClassPropertySymbolName(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_CLASS_PROPERTY ||
        node->data.classProperty.modifier == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
        node->data.classProperty.modifier->data.propertyGet.name != ZR_NULL) {
        return node->data.classProperty.modifier->data.propertyGet.name->name;
    }

    if (node->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
        node->data.classProperty.modifier->data.propertySet.name != ZR_NULL) {
        return node->data.classProperty.modifier->data.propertySet.name->name;
    }

    return ZR_NULL;
}

void ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(SZrState *state,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrSymbol *symbol) {
    SZrFileRange range;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->referenceTracker == ZR_NULL ||
        symbol == ZR_NULL) {
        return;
    }

    range = symbol->selectionRange;
    if (range.start.offset == 0 && range.end.offset == 0) {
        range = symbol->location;
    }

    ZrLanguageServer_ReferenceTracker_AddReference(state,
                                                   analyzer->referenceTracker,
                                                   symbol,
                                                   range,
                                                   ZR_REFERENCE_DEFINITION);
}

// 辅助函数：递归计算 AST 节点的哈希值
static TZrUInt64 compute_node_hash_recursive(SZrAstNode *node, TZrSize depth) {
    if (node == ZR_NULL || depth > 32) { // 限制递归深度避免栈溢出
        return 0;
    }
    
    TZrUInt64 hash = (TZrUInt64)node->type;
    hash = hash * 31 + (TZrUInt64)node->location.start.offset;
    hash = hash * 31 + (TZrUInt64)node->location.end.offset;
    
    // 根据节点类型访问不同的子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(script->statements->nodes[i], depth + 1);
                }
            }
            if (script->moduleName != ZR_NULL) {
                hash = hash * 31 + compute_node_hash_recursive(script->moduleName, depth + 1);
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(block->body->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(binExpr->left, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(binExpr->right, depth + 1);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(unaryExpr->argument, depth + 1);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            hash = hash * 31 + compute_node_hash_recursive(assignExpr->left, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(assignExpr->right, depth + 1);
            break;
        }
        
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
            hash = hash * 31 + compute_node_hash_recursive(condExpr->test, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(condExpr->consequent, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(condExpr->alternate, depth + 1);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(funcCall->args->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(primaryExpr->property, depth + 1);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(primaryExpr->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            hash = hash * 31 + compute_node_hash_recursive(varDecl->pattern, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(varDecl->value, depth + 1);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(funcDecl->params->nodes[i], depth + 1);
                }
            }
            hash = hash * 31 + compute_node_hash_recursive(funcDecl->body, depth + 1);
            break;
        }
        
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arrayLit = &node->data.arrayLiteral;
            if (arrayLit->elements != ZR_NULL && arrayLit->elements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < arrayLit->elements->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(arrayLit->elements->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *objLit = &node->data.objectLiteral;
            if (objLit->properties != ZR_NULL && objLit->properties->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < objLit->properties->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(objLit->properties->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->condition, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->thenExpr, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->elseExpr, depth + 1);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            hash = hash * 31 + compute_node_hash_recursive(whileLoop->cond, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(whileLoop->block, depth + 1);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            hash = hash * 31 + compute_node_hash_recursive(forLoop->init, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->cond, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->step, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->block, depth + 1);
            break;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(classDecl->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(structDecl->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        default:
            // 对于其他节点类型，只使用基础哈希值
            break;
    }
    
    return hash;
}

// 辅助函数：计算 AST 哈希（递归实现）
TZrSize ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(SZrAstNode *ast) {
    if (ast == ZR_NULL) {
        return 0;
    }
    
    return (TZrSize)compute_node_hash_recursive(ast, 0);
}

static TZrBool reset_symbol_tracking(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
        analyzer->referenceTracker = ZR_NULL;
    }
    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        analyzer->symbolTable = ZR_NULL;
    }

    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    if (analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        analyzer->symbolTable = ZR_NULL;
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_PrepareState(SZrState *state,
                                    SZrSemanticAnalyzer *analyzer,
                                    SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reset_symbol_tracking(state, analyzer)) {
        return ZR_FALSE;
    }

    if (analyzer->compilerState == ZR_NULL) {
        analyzer->compilerState =
            (SZrCompilerState *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompilerState));
        if (analyzer->compilerState == ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        ZrParser_CompilerState_Free(analyzer->compilerState);
    }

    ZrParser_CompilerState_Init(analyzer->compilerState, state);
    analyzer->compilerState->scriptAst = ast;
    if (analyzer->compilerState->typeEnv != ZR_NULL) {
        analyzer->compilerState->typeEnv->semanticContext =
            analyzer->compilerState->semanticContext;
    }
    if (analyzer->compilerState->compileTimeTypeEnv != ZR_NULL) {
        analyzer->compilerState->compileTimeTypeEnv->semanticContext =
            analyzer->compilerState->semanticContext;
    }

    if (analyzer->compilerState->hirModule != ZR_NULL) {
        ZrParser_HirModule_Free(state, analyzer->compilerState->hirModule);
        analyzer->compilerState->hirModule = ZR_NULL;
    }
    analyzer->compilerState->hirModule =
        ZrParser_HirModule_New(state, analyzer->compilerState->semanticContext, ast);
    analyzer->semanticContext = analyzer->compilerState->semanticContext;
    analyzer->hirModule = analyzer->compilerState->hirModule;

    return analyzer->semanticContext != ZR_NULL;
}

TZrBool ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(SZrSemanticAnalyzer *analyzer,
                                       SZrSymbol *symbol,
                                       EZrSemanticSymbolKind semanticKind,
                                       const SZrInferredType *typeInfo,
                                       EZrSemanticTypeKind typeKind) {
    TZrTypeId typeId = 0;
    TZrOverloadSetId overloadSetId = 0;
    TZrSymbolId symbolId;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_TYPE && typeInfo == ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterNamedType(analyzer->semanticContext,
                                             symbol->name,
                                             typeKind,
                                             symbol->astNode);
    } else if (typeInfo != ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterInferredType(analyzer->semanticContext,
                                                typeInfo,
                                                typeKind,
                                                typeInfo->typeName,
                                                symbol->astNode);
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        overloadSetId = ZrParser_Semantic_GetOrCreateOverloadSet(analyzer->semanticContext, symbol->name);
    }

    symbolId = ZrParser_Semantic_RegisterSymbol(analyzer->semanticContext,
                                        symbol->name,
                                        semanticKind,
                                        typeId,
                                        overloadSetId,
                                        symbol->astNode,
                                        symbol->location);
    if (overloadSetId != 0) {
        ZrParser_Semantic_AddOverloadMember(analyzer->semanticContext, overloadSetId, symbolId);
    }

    symbol->semanticId = symbolId;
    symbol->semanticTypeId = typeId;
    symbol->overloadSetId = overloadSetId;
    return symbolId != 0;
}

static TZrSymbolId resolve_semantic_symbol_id_for_node(SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node) {
    SZrString *name;
    SZrSymbol *symbol;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || node == ZR_NULL) {
        return 0;
    }

    name = ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(analyzer->state, node);
    if (name == ZR_NULL) {
        return 0;
    }

    symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, name, ZR_NULL);
    if (symbol == ZR_NULL) {
        return 0;
    }

    return symbol->semanticId;
}

void ZrLanguageServer_SemanticAnalyzer_RecordTemplateStringSegments(SZrSemanticAnalyzer *analyzer,
                                            SZrAstNode *node) {
    SZrTemplateStringLiteral *templateLiteral;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        return;
    }

    templateLiteral = &node->data.templateStringLiteral;
    if (templateLiteral->segments == ZR_NULL || templateLiteral->segments->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
        SZrAstNode *segmentNode = templateLiteral->segments->nodes[i];
        SZrTemplateSegment segment;

        if (segmentNode == ZR_NULL) {
            continue;
        }

        segment.isInterpolation = ZR_FALSE;
        segment.staticText = ZR_NULL;
        segment.expression = ZR_NULL;

        if (segmentNode->type == ZR_AST_STRING_LITERAL) {
            segment.staticText = segmentNode->data.stringLiteral.value;
        } else if (segmentNode->type == ZR_AST_INTERPOLATED_SEGMENT) {
            segment.isInterpolation = ZR_TRUE;
            segment.expression = segmentNode->data.interpolatedSegment.expression;
        } else {
            continue;
        }

        ZrParser_Semantic_AppendTemplateSegment(analyzer->semanticContext, &segment);
    }
}

void ZrLanguageServer_SemanticAnalyzer_RecordUsingCleanupStep(SZrSemanticAnalyzer *analyzer,
                                      SZrAstNode *resource) {
    SZrDeterministicCleanupStep step;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || resource == ZR_NULL) {
        return;
    }

    step.kind = ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext);
    step.ownerRegionId = step.regionId;
    step.symbolId = resolve_semantic_symbol_id_for_node(analyzer, resource);
    step.declarationOrder = (TZrInt32)analyzer->semanticContext->cleanupPlan.length;
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    ZrParser_Semantic_AppendCleanupStep(analyzer->semanticContext, &step);
}

static SZrInferredType *create_field_symbol_type(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 const SZrType *fieldType) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    if (fieldType == ZR_NULL) {
        return typeInfo;
    }

    if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL &&
        ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState, fieldType, typeInfo)) {
        return typeInfo;
    }

    ZrParser_InferredType_Free(state, typeInfo);
    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    typeInfo->ownershipQualifier = fieldType->ownershipQualifier;
    return typeInfo;
}

static void record_field_cleanup_step(SZrSemanticAnalyzer *analyzer,
                                      SZrSymbol *symbol,
                                      EZrDeterministicCleanupKind kind,
                                      TZrLifetimeRegionId ownerRegionId,
                                      TZrInt32 declarationOrder) {
    SZrDeterministicCleanupStep step;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL ||
        symbol->semanticId == 0) {
        return;
    }

    step.kind = kind;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext);
    step.ownerRegionId = ownerRegionId;
    step.symbolId = symbol->semanticId;
    step.declarationOrder = declarationOrder;
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    ZrParser_Semantic_AppendCleanupStep(analyzer->semanticContext, &step);
}

void ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *fieldNode,
                                           TZrLifetimeRegionId ownerRegionId,
                                           EZrDeterministicCleanupKind cleanupKind,
                                           TZrInt32 declarationOrder) {
    SZrSymbol *symbol = ZR_NULL;
    SZrString *name = ZR_NULL;
    SZrInferredType *typeInfo = ZR_NULL;
    EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;
    TZrBool isUsingManaged = ZR_FALSE;
    TZrBool isStatic = ZR_FALSE;
    const SZrType *fieldType = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || fieldNode == ZR_NULL) {
        return;
    }

    if (fieldNode->type == ZR_AST_STRUCT_FIELD) {
        SZrStructField *field = &fieldNode->data.structField;
        name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        accessModifier = field->access;
        isUsingManaged = field->isUsingManaged;
        isStatic = field->isStatic;
        fieldType = field->typeInfo;
    } else if (fieldNode->type == ZR_AST_CLASS_FIELD) {
        SZrClassField *field = &fieldNode->data.classField;
        name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        accessModifier = field->access;
        isUsingManaged = field->isUsingManaged;
        isStatic = field->isStatic;
        fieldType = field->typeInfo;
    } else {
        return;
    }

    if (name == ZR_NULL) {
        return;
    }

    typeInfo = create_field_symbol_type(state, analyzer, fieldType);
    ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                             analyzer->symbolTable,
                             ZR_SYMBOL_FIELD,
                             name,
                             fieldNode->location,
                             typeInfo,
                             accessModifier,
                             fieldNode,
                             &symbol);

    if (symbol != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(analyzer,
                                  symbol,
                                  ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                  typeInfo,
                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
        ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(state, analyzer, symbol);
    }

    if (!isUsingManaged) {
        return;
    }

    if (isStatic) {
        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                        analyzer,
                                        ZR_DIAGNOSTIC_ERROR,
                                        fieldNode->location,
                                        "Field-scoped `using` only supports instance fields",
                                        "static_using_field");
        return;
    }

    record_field_cleanup_step(analyzer,
                              symbol,
                              cleanupKind,
                              ownerRegionId,
                              declarationOrder);
}

// 辅助函数：遍历 AST 收集符号定义
