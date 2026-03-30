//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "compile_statement_internal.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"

#include <stdio.h>
#include <string.h>

ZR_PARSER_API void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);
static void compile_using_statement(SZrCompilerState *cs, SZrAstNode *node);
static SZrAstNode *try_statement_first_catch_clause(SZrTryCatchFinallyStatement *stmt) {
    if (stmt == ZR_NULL || stmt->catchClauses == ZR_NULL || stmt->catchClauses->count == 0) {
        return ZR_NULL;
    }
    return stmt->catchClauses->nodes[0];
}

static void emit_constant_to_slot_local(SZrCompilerState *cs, TZrUInt32 slot, const SZrTypeValue *value,
                                        SZrFileRange location) {
    if (cs == ZR_NULL || value == ZR_NULL || cs->hasError) {
        return;
    }

    if (!ZrParser_Compiler_ValidateRuntimeProjectionValue(cs, value, location)) {
        return;
    }

    SZrTypeValue constantValue = *value;
    TZrUInt32 constantIndex = add_constant(cs, &constantValue);
    TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                               (TZrUInt16)slot,
                                               (TZrInt32)constantIndex);
    emit_instruction(cs, inst);
}

static TZrBool emit_cstring_constant_to_slot_local(SZrCompilerState *cs, TZrUInt32 slot, const TZrChar *literal) {
    SZrString *stringValue;
    SZrTypeValue constantValue;

    if (cs == ZR_NULL || literal == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    stringValue = ZrCore_String_Create(cs->state, literal, strlen(literal));
    if (stringValue == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to allocate string constant", cs->errorLocation);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    constantValue.type = ZR_VALUE_TYPE_STRING;
    emit_constant_to_slot_local(cs, slot, &constantValue, cs->errorLocation);
    return !cs->hasError;
}

static TZrBool resolve_fixed_array_size(const SZrInferredType *type, TZrSize *fixedSize) {
    if (type == ZR_NULL || fixedSize == ZR_NULL ||
        type->baseType != ZR_VALUE_TYPE_ARRAY || !type->hasArraySizeConstraint) {
        return ZR_FALSE;
    }

    if (type->arrayFixedSize > 0) {
        *fixedSize = type->arrayFixedSize;
        return ZR_TRUE;
    }

    if (type->arrayMinSize > 0 && type->arrayMinSize == type->arrayMaxSize) {
        *fixedSize = type->arrayMinSize;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void compile_default_fixed_array_initialization(SZrCompilerState *cs,
                                                       TZrUInt32 arraySlot,
                                                       TZrSize fixedSize,
                                                       SZrFileRange location) {
    SZrTypeValue nullValue;
    TZrUInt32 nullConstantIndex;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    emit_instruction(cs,
                     create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_ARRAY),
                                          (TZrUInt16)arraySlot));

    ZrCore_Value_ResetAsNull(&nullValue);
    nullConstantIndex = add_constant(cs, &nullValue);

    for (TZrSize i = 0; i < fixedSize; i++) {
        SZrTypeValue indexValue;
        TZrUInt32 valueSlot = allocate_stack_slot(cs);
        TZrUInt32 indexSlot;

        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                              (TZrUInt16)valueSlot,
                                              (TZrInt32)nullConstantIndex));

        ZrCore_Value_InitAsInt(cs->state, &indexValue, (TZrInt64)i);
        indexSlot = allocate_stack_slot(cs);
        emit_constant_to_slot_local(cs, indexSlot, &indexValue, location);

        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE),
                                              (TZrUInt16)valueSlot,
                                              (TZrUInt16)arraySlot,
                                              (TZrUInt16)indexSlot));
        ZrParser_Compiler_TrimStackBy(cs, 2);
    }

    {
        SZrTypeValue lengthValue;
        TZrUInt32 keySlot = allocate_stack_slot(cs);
        TZrUInt32 valueSlot = allocate_stack_slot(cs);

        if (!emit_cstring_constant_to_slot_local(cs, keySlot, "length")) {
            return;
        }

        ZrCore_Value_InitAsInt(cs->state, &lengthValue, (TZrInt64)fixedSize);
        emit_constant_to_slot_local(cs, valueSlot, &lengthValue, location);

        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE),
                                              (TZrUInt16)valueSlot,
                                              (TZrUInt16)arraySlot,
                                              (TZrUInt16)keySlot));
        ZrParser_Compiler_TrimStackBy(cs, 2);
    }

    ZR_UNUSED_PARAMETER(location);
}

static TZrTypeId resolve_using_resource_type_id(SZrCompilerState *cs, SZrAstNode *resource) {
    SZrInferredType inferredType;
    TZrBool hasInferredType = ZR_FALSE;
    TZrTypeId typeId = 0;
    EZrSemanticTypeKind semanticKind = ZR_SEMANTIC_TYPE_KIND_REFERENCE;

    if (cs == ZR_NULL || resource == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return 0;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);

    if (resource->type == ZR_AST_IDENTIFIER_LITERAL && cs->typeEnv != ZR_NULL &&
        resource->data.identifier.name != ZR_NULL) {
        hasInferredType = ZrParser_TypeEnvironment_LookupVariable(cs->state,
                                                          cs->typeEnv,
                                                          resource->data.identifier.name,
                                                          &inferredType);
    }

    if (!hasInferredType) {
        hasInferredType = ZrParser_ExpressionType_Infer(cs, resource, &inferredType);
    }

    if (hasInferredType) {
        if (inferredType.baseType != ZR_VALUE_TYPE_OBJECT &&
            inferredType.baseType != ZR_VALUE_TYPE_ARRAY &&
            inferredType.baseType != ZR_VALUE_TYPE_STRING) {
            semanticKind = ZR_SEMANTIC_TYPE_KIND_VALUE;
        }

        typeId = ZrParser_Semantic_RegisterInferredType(cs->semanticContext,
                                                &inferredType,
                                                semanticKind,
                                                inferredType.typeName,
                                                resource);
    }

    ZrParser_InferredType_Free(cs->state, &inferredType);
    return typeId;
}

static TZrSymbolId register_using_resource_symbol(SZrCompilerState *cs, SZrAstNode *resource) {
    TZrTypeId typeId;

    if (cs == ZR_NULL || resource == ZR_NULL || cs->semanticContext == ZR_NULL ||
        resource->type != ZR_AST_IDENTIFIER_LITERAL ||
        resource->data.identifier.name == ZR_NULL) {
        return 0;
    }

    typeId = resolve_using_resource_type_id(cs, resource);
    return ZrParser_Semantic_RegisterSymbol(cs->semanticContext,
                                    resource->data.identifier.name,
                                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                    typeId,
                                    0,
                                    resource,
                                    resource->location);
}

static SZrScope *get_current_scope(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->scopeStack.length == 0) {
        return ZR_NULL;
    }

    return (SZrScope *)ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1);
}

static SZrString *create_hidden_using_local_name(SZrCompilerState *cs) {
    TZrChar buffer[64];
    int length;

    if (cs == ZR_NULL || cs->state == ZR_NULL) {
        return ZR_NULL;
    }

    length = snprintf(buffer,
                      sizeof(buffer),
                      "__zr_using_%u_%u",
                      (unsigned)cs->scopeStack.length,
                      (unsigned)cs->localVars.length);
    if (length < 0) {
        return ZR_NULL;
    }

    if ((size_t)length >= sizeof(buffer)) {
        length = (int)sizeof(buffer) - 1;
        buffer[length] = '\0';
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)length);
}

static TZrBool compile_using_resource_slot(SZrCompilerState *cs, SZrAstNode *resource, TZrUInt32 *slot) {
    TZrUInt32 targetSlot;
    TZrUInt32 valueSlot;
    SZrString *hiddenName;
    TZrUInt32 existingLocalSlot;

    if (slot == ZR_NULL || cs == ZR_NULL || resource == ZR_NULL || cs->hasError) {
        return ZR_FALSE;
    }

    if (resource->type == ZR_AST_IDENTIFIER_LITERAL && resource->data.identifier.name != ZR_NULL) {
        existingLocalSlot = find_local_var(cs, resource->data.identifier.name);
        if (existingLocalSlot != (TZrUInt32)-1) {
            *slot = existingLocalSlot;
            return ZR_TRUE;
        }
    }

    hiddenName = create_hidden_using_local_name(cs);
    if (hiddenName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Failed to create using resource slot", resource->location);
        return ZR_FALSE;
    }

    targetSlot = allocate_local_var(cs, hiddenName);
    ZrParser_Expression_Compile(cs, resource);
    if (cs->hasError || cs->stackSlotCount == 0) {
        return ZR_FALSE;
    }

    valueSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    if (valueSlot != targetSlot) {
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                              (TZrUInt16)targetSlot,
                                              (TZrInt32)valueSlot));
    }

    ZrParser_Compiler_TrimStackToSlot(cs, targetSlot);
    *slot = targetSlot;
    return ZR_TRUE;
}

static void register_using_cleanup_slot(SZrCompilerState *cs, TZrUInt32 slot) {
    SZrScope *scope;

    if (cs == ZR_NULL || cs->hasError) {
        return;
    }

    scope = get_current_scope(cs);
    if (scope == ZR_NULL) {
        return;
    }

    emit_instruction(cs,
                     create_instruction_0(ZR_INSTRUCTION_ENUM(MARK_TO_BE_CLOSED),
                                          (TZrUInt16)slot));
    scope->cleanupRegistrationCount++;
}

// 编译变量声明
static void compile_variable_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_VARIABLE_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected variable declaration", node->location);
        return;
    }
    
    SZrVariableDeclaration *decl = &node->data.variableDeclaration;
    
    // 检查 pattern 是否存在
    if (decl->pattern == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Variable declaration pattern is null", node->location);
        return;
    }
    
    // 处理单个变量声明（标识符 pattern）
    if (decl->pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *varName = decl->pattern->data.identifier.name;
        SZrInferredType resolvedType;
        TZrBool resolvedTypeInitialized = ZR_FALSE;
        TZrBool hasResolvedType = ZR_FALSE;
        if (varName == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Variable name is null", node->location);
            return;
        }

        if (decl->typeInfo != ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            resolvedTypeInitialized = ZR_TRUE;
            hasResolvedType = ZrParser_AstTypeToInferredType_Convert(cs, decl->typeInfo, &resolvedType);
        } else if (decl->value != ZR_NULL) {
            ZrParser_InferredType_Init(cs->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            resolvedTypeInitialized = ZR_TRUE;
            hasResolvedType = ZrParser_ExpressionType_Infer(cs, decl->value, &resolvedType);
        }

        if (cs->hasError) {
            if (resolvedTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &resolvedType);
            }
            return;
        }

        TZrUInt32 varIndex = 0;

        // 如果有初始值，编译初始值表达式
        if (decl->value != ZR_NULL) {
            ZrParser_Expression_Compile(cs, decl->value);
            if (cs->hasError || cs->stackSlotCount == 0) {
                if (resolvedTypeInitialized) {
                    ZrParser_InferredType_Free(cs->state, &resolvedType);
                }
                return;
            }

            varIndex = bind_existing_stack_slot_as_local_var(cs,
                                                             varName,
                                                             (TZrUInt32)(cs->stackSlotCount - 1));
        } else {
            TZrSize fixedArraySize;
            varIndex = allocate_local_var(cs, varName);

            if (hasResolvedType && resolve_fixed_array_size(&resolvedType, &fixedArraySize)) {
                compile_default_fixed_array_initialization(cs, varIndex, fixedArraySize, node->location);
            } else {
                // 没有初始值，设置为 null
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                emit_constant_to_slot_local(cs, varIndex, &nullValue, node->location);

                TZrInstruction setInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)varIndex, (TZrInt32)varIndex);
                emit_instruction(cs, setInst);
            }

            ZrParser_Compiler_TrimStackToSlot(cs, varIndex);
        }

        if (cs->typeEnv != ZR_NULL) {
            if (hasResolvedType) {
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, varName, &resolvedType);
            } else {
                SZrInferredType defaultType;
                ZrParser_InferredType_Init(cs->state, &defaultType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_TypeEnvironment_RegisterVariable(cs->state, cs->typeEnv, varName, &defaultType);
                ZrParser_InferredType_Free(cs->state, &defaultType);
            }
        }

        // 如果是 const 变量，记录到 constLocalVars 数组
        if (decl->isConst) {
            ZrCore_Array_Push(cs->state, &cs->constLocalVars, &varName);
        }

        // 如果是脚本级变量且可见性为 pub 或 pro，记录到导出列表
        if (cs->isScriptLevel && decl->accessModifier != ZR_ACCESS_PRIVATE) {
            SZrExportedVariable exportedVar;
            exportedVar.name = varName;
            exportedVar.stackSlot = varIndex;
            exportedVar.accessModifier = decl->accessModifier;

            if (decl->accessModifier == ZR_ACCESS_PUBLIC) {
                ZrCore_Array_Push(cs->state, &cs->pubVariables, &exportedVar);
                ZrCore_Array_Push(cs->state, &cs->proVariables, &exportedVar);
            } else if (decl->accessModifier == ZR_ACCESS_PROTECTED) {
                ZrCore_Array_Push(cs->state, &cs->proVariables, &exportedVar);
            }
        }

        if (resolvedTypeInitialized) {
            ZrParser_InferredType_Free(cs->state, &resolvedType);
        }
    } else if (decl->pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        // 处理解构对象赋值：var {key1, key2, ...} = value;
        compile_destructuring_object(cs, decl->pattern, decl->value);
    } else if (decl->pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        // 处理解构数组赋值：var [elem1, elem2, ...] = value;
        compile_destructuring_array(cs, decl->pattern, decl->value);
    } else {
        // 未知的 pattern 类型
        ZrParser_Compiler_Error(cs, "Unknown variable declaration pattern type", node->location);
    }
}

// 编译表达式语句
static void compile_expression_statement(SZrCompilerState *cs, SZrAstNode *node) {
    TZrSize previousStackCount;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_EXPRESSION_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected expression statement", node->location);
        return;
    }
    
    SZrExpressionStatement *stmt = &node->data.expressionStatement;
    previousStackCount = cs->stackSlotCount;
    if (stmt->expr != ZR_NULL) {
        // 编译表达式
        ZrParser_Expression_Compile(cs, stmt->expr);
        ZrParser_Compiler_TrimStackToCount(cs, previousStackCount);
    }
}

// 编译返回语句
static void compile_return_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrCompilerTryContext finallyContext;
    TZrBool hasFinallyContext = ZR_FALSE;
    SZrReturnStatement *stmt;
    TZrUInt32 resultSlot = 0;
    TZrUInt32 resultCount = 0;
    TZrBool oldTailCallContext;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_RETURN_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected return statement", node->location);
        return;
    }

    stmt = &node->data.returnStatement;
    hasFinallyContext = try_context_find_innermost_finally(cs, &finallyContext);

    oldTailCallContext = cs->isInTailCallContext;
    cs->isInTailCallContext = hasFinallyContext ? ZR_FALSE : ZR_TRUE;

    if (stmt->expr != ZR_NULL) {
        TZrSize exprInstBefore = cs->instructions.length;
        TZrSize exprInstAfter;

        ZrParser_Expression_Compile(cs, stmt->expr);
        exprInstAfter = cs->instructions.length;
        if (exprInstAfter > exprInstBefore) {
            TZrInstruction *lastInst = (TZrInstruction *)ZrCore_Array_Get(&cs->instructions, exprInstAfter - 1);
            if (lastInst != ZR_NULL &&
                lastInst->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL)) {
                resultSlot = lastInst->instruction.operandExtra;
            } else {
                resultSlot = cs->stackSlotCount - 1;
            }
        } else {
            resultSlot = cs->stackSlotCount - 1;
        }
        resultCount = 1;
    } else {
        SZrTypeValue nullValue;
        TZrUInt32 constantIndex;

        resultSlot = allocate_stack_slot(cs);
        ZrCore_Value_ResetAsNull(&nullValue);
        constantIndex = add_constant(cs, &nullValue);
        emit_instruction(cs,
                         create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                              (TZrUInt16)resultSlot,
                                              (TZrInt32)constantIndex));
        resultCount = 1;
    }

    cs->isInTailCallContext = oldTailCallContext;

    if (hasFinallyContext) {
        TZrSize resumeLabelId = create_label(cs);
        TZrInstruction pendingInst =
                create_instruction_1(ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN), (TZrUInt16)resultSlot, 0);
        TZrSize pendingIndex = cs->instructionCount;

        emit_instruction(cs, pendingInst);
        add_pending_absolute_patch(cs, pendingIndex, resumeLabelId);
        emit_jump_to_label(cs, finallyContext.finallyLabelId);
        resolve_label(cs, resumeLabelId);
    }

    emit_instruction(cs,
                     create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN),
                                          (TZrUInt16)resultCount,
                                          (TZrUInt16)resultSlot,
                                          0));
}

// 编译块语句
static void compile_block_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BLOCK) {
        ZrParser_Compiler_Error(cs, "Expected block statement", node->location);
        return;
    }
    
    SZrBlock *block = &node->data.block;
    
    // 进入新作用域
    enter_scope(cs);

    ZrParser_Compiler_PredeclareFunctionBindings(cs, block->body);
    if (cs->hasError) {
        return;
    }
    
    // 编译块内所有语句
    if (block->body != ZR_NULL) {
        for (TZrSize i = 0; i < block->body->count; i++) {
            SZrAstNode *stmt = block->body->nodes[i];
            if (stmt != ZR_NULL) {
                ZrParser_Statement_Compile(cs, stmt);
                if (cs->hasError) {
                    break;
                }
            }
        }
    }
    
    // 退出作用域
    exit_scope(cs);
}

static void compile_using_statement(SZrCompilerState *cs, SZrAstNode *node) {
    SZrUsingStatement *stmt;
    TZrLifetimeRegionId regionId = 0;
    TZrSymbolId symbolId = 0;
    TZrUInt32 resourceSlot = 0;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_USING_STATEMENT) {
        ZrParser_Compiler_Error(cs, "Expected using statement", node->location);
        return;
    }

    stmt = &node->data.usingStatement;

    if (cs->semanticContext != ZR_NULL) {
        SZrDeterministicCleanupStep cleanupStep;

        regionId = ZrParser_Semantic_ReserveLifetimeRegionId(cs->semanticContext);
        symbolId = register_using_resource_symbol(cs, stmt->resource);

        cleanupStep.kind = ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE;
        cleanupStep.regionId = regionId;
        cleanupStep.ownerRegionId = regionId;
        cleanupStep.symbolId = symbolId;
        cleanupStep.declarationOrder = (TZrInt32)cs->semanticContext->cleanupPlan.length;
        cleanupStep.callsClose = ZR_TRUE;
        cleanupStep.callsDestructor = ZR_TRUE;
        ZrParser_Semantic_AppendCleanupStep(cs->semanticContext, &cleanupStep);
    }

    if (stmt->body != ZR_NULL) {
        enter_scope(cs);
    }

    if (stmt->resource != ZR_NULL) {
        if (!compile_using_resource_slot(cs, stmt->resource, &resourceSlot)) {
            return;
        }
        register_using_cleanup_slot(cs, resourceSlot);
    }

    if (stmt->body != ZR_NULL) {
        ZrParser_Statement_Compile(cs, stmt->body);
        if (cs->hasError) {
            return;
        }
        exit_scope(cs);
    }
}

// 编译 if 语句
static void compile_if_statement(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    // if 语句和 if 表达式使用相同的 AST 节点类型
    if (node->type != ZR_AST_IF_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected if statement", node->location);
        return;
    }
    
    SZrIfExpression *ifExpr = &node->data.ifExpression;
    
    // 编译条件表达式
    ZrParser_Expression_Compile(cs, ifExpr->condition);
    TZrUInt32 condSlot = cs->stackSlotCount - 1;
    
    // 创建 else 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)condSlot, 0);  // 偏移将在后面填充
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    if (ifExpr->thenExpr != ZR_NULL) {
        ZrParser_Statement_Compile(cs, ifExpr->thenExpr);
    }
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);  // 偏移将在后面填充
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    if (ifExpr->elseExpr != ZR_NULL) {
        ZrParser_Statement_Compile(cs, ifExpr->elseExpr);
    }
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 编译 while 语句
ZR_PARSER_API void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node) {
    SZrAstNode *oldCurrentAst;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    oldCurrentAst = cs->currentAst;
    cs->currentAst = node;
    
    switch (node->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            compile_variable_declaration(cs, node);
            break;
        
        case ZR_AST_EXPRESSION_STATEMENT:
            compile_expression_statement(cs, node);
            break;

        case ZR_AST_USING_STATEMENT:
            compile_using_statement(cs, node);
            break;
        
        case ZR_AST_RETURN_STATEMENT:
            compile_return_statement(cs, node);
            break;
        
        case ZR_AST_BLOCK:
            compile_block_statement(cs, node);
            break;
        
        case ZR_AST_IF_EXPRESSION:
            // 检查是否是语句（isStatement 标志）
            if (node->data.ifExpression.isStatement) {
                compile_if_statement(cs, node);
            } else {
                // 作为表达式处理
                ZrParser_Expression_Compile(cs, node);
            }
            break;
        
        case ZR_AST_WHILE_LOOP:
            // 检查是否是语句
            if (node->data.whileLoop.isStatement) {
                compile_while_statement(cs, node);
            } else {
                // 作为表达式处理（暂不支持）
                ZrParser_Compiler_Error(cs, "While loop as expression is not supported", node->location);
            }
            break;
        
        case ZR_AST_FOR_LOOP:
            compile_for_statement(cs, node);
            break;
        
        case ZR_AST_FOREACH_LOOP:
            compile_foreach_statement(cs, node);
            break;
        
        case ZR_AST_SWITCH_EXPRESSION:
            // 检查是否是语句
            if (node->data.switchExpression.isStatement) {
                compile_switch_statement(cs, node);
            } else {
                // 作为表达式处理
                ZrParser_Expression_Compile(cs, node);
            }
            break;
        
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            compile_break_continue_statement(cs, node);
            break;
        
        case ZR_AST_THROW_STATEMENT:
            compile_throw_statement(cs, node);
            break;
        
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            compile_try_catch_finally_statement(cs, node);
            break;
        
        case ZR_AST_OUT_STATEMENT:
            compile_out_statement(cs, node);
            break;
        
        case ZR_AST_FUNCTION_DECLARATION:
            // 函数声明可以作为语句编译（嵌套函数）
            compile_function_declaration(cs, node);
            break;

        case ZR_AST_EXTERN_BLOCK:
            ZrParser_Compiler_CompileExternBlock(cs, node);
            break;
        
        default:
            // 检查是否是声明类型（不应该作为语句编译）
            if (node->type == ZR_AST_INTERFACE_METHOD_SIGNATURE ||
                node->type == ZR_AST_INTERFACE_FIELD_DECLARATION ||
                node->type == ZR_AST_INTERFACE_PROPERTY_SIGNATURE ||
                node->type == ZR_AST_INTERFACE_META_SIGNATURE ||
                node->type == ZR_AST_STRUCT_FIELD ||
                node->type == ZR_AST_STRUCT_METHOD ||
                node->type == ZR_AST_STRUCT_META_FUNCTION ||
                node->type == ZR_AST_CLASS_FIELD ||
                node->type == ZR_AST_CLASS_METHOD ||
                node->type == ZR_AST_CLASS_PROPERTY ||
                node->type == ZR_AST_CLASS_META_FUNCTION ||
                node->type == ZR_AST_STRUCT_DECLARATION ||
                node->type == ZR_AST_CLASS_DECLARATION ||
                node->type == ZR_AST_INTERFACE_DECLARATION ||
                node->type == ZR_AST_ENUM_DECLARATION ||
                node->type == ZR_AST_EXTERN_FUNCTION_DECLARATION ||
                node->type == ZR_AST_EXTERN_DELEGATE_DECLARATION ||
                node->type == ZR_AST_ENUM_MEMBER ||
                node->type == ZR_AST_MODULE_DECLARATION ||
                node->type == ZR_AST_SCRIPT) {
                // 这些是声明类型，不应该作为语句编译
                static TZrChar errorMsg[256];
                const TZrChar *typeName = "UNKNOWN";
                switch (node->type) {
                    case ZR_AST_INTERFACE_METHOD_SIGNATURE: typeName = "INTERFACE_METHOD_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_FIELD_DECLARATION: typeName = "INTERFACE_FIELD_DECLARATION"; break;
                    case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: typeName = "INTERFACE_PROPERTY_SIGNATURE"; break;
                    case ZR_AST_INTERFACE_META_SIGNATURE: typeName = "INTERFACE_META_SIGNATURE"; break;
                    case ZR_AST_STRUCT_FIELD: typeName = "STRUCT_FIELD"; break;
                    case ZR_AST_STRUCT_METHOD: typeName = "STRUCT_METHOD"; break;
                    case ZR_AST_STRUCT_META_FUNCTION: typeName = "STRUCT_META_FUNCTION"; break;
                    case ZR_AST_CLASS_FIELD: typeName = "CLASS_FIELD"; break;
                    case ZR_AST_CLASS_METHOD: typeName = "CLASS_METHOD"; break;
                    case ZR_AST_CLASS_PROPERTY: typeName = "CLASS_PROPERTY"; break;
                    case ZR_AST_CLASS_META_FUNCTION: typeName = "CLASS_META_FUNCTION"; break;
                    case ZR_AST_STRUCT_DECLARATION: typeName = "STRUCT_DECLARATION"; break;
                    case ZR_AST_CLASS_DECLARATION: typeName = "CLASS_DECLARATION"; break;
                    case ZR_AST_INTERFACE_DECLARATION: typeName = "INTERFACE_DECLARATION"; break;
                    case ZR_AST_ENUM_DECLARATION: typeName = "ENUM_DECLARATION"; break;
                    case ZR_AST_EXTERN_FUNCTION_DECLARATION: typeName = "EXTERN_FUNCTION_DECLARATION"; break;
                    case ZR_AST_EXTERN_DELEGATE_DECLARATION: typeName = "EXTERN_DELEGATE_DECLARATION"; break;
                    case ZR_AST_ENUM_MEMBER: typeName = "ENUM_MEMBER"; break;
                    case ZR_AST_MODULE_DECLARATION: typeName = "MODULE_DECLARATION"; break;
                    case ZR_AST_SCRIPT: typeName = "SCRIPT"; break;
                    default: break;
                }
                snprintf(errorMsg, sizeof(errorMsg), 
                        "Declaration type '%s' (type %d) cannot be used as a statement at line %d:%d", 
                        typeName, node->type, 
                        node->location.start.line, node->location.start.column);
                ZrParser_Compiler_Error(cs, errorMsg, node->location);
                return;
            }
            
            // 其他类型尝试作为表达式编译
            ZrParser_Expression_Compile(cs, node);
            break;
    }

    cs->currentAst = oldCurrentAst;
}

// 编译解构对象赋值：var {key1, key2, ...} = value;
