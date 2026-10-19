#include "compiler/expression/compile_expression_binary.h"
#include "compiler/expression/compile_expression_internal.h"
#include "compiler/semantic/compiler_semantic_compare.h"
#include "type_inference/type_inference_internal.h"

static void compile_binary_expression_error(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const TZrChar *message) {
    ZrParser_Compiler_Error(cs, message, node->location);
}

/* Keep conversion and emission temporaries off recursive operand compilation. */
static void compile_binary_expression_emit(
        SZrCompilerState *cs,
        SZrAstNode *node,
        const TZrChar *op,
        TZrUInt32 leftSlot,
        TZrUInt32 rightSlot,
        SZrInferredType *leftType,
        SZrInferredType *rightType,
        SZrInferredType *resultType,
        TZrBool hasTypeInfo,
        EZrValueType effectiveLeftType,
        EZrValueType effectiveRightType) {
    // 如果需要类型转换，插入转换指令
    // 注意：对于比较操作，需要保留 leftType 和 rightType 用于选择正确的比较指令
    TZrBool isComparisonOp = (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
                            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                            strcmp(op, "==") == 0 || strcmp(op, "!=") == 0);
    TZrBool isShiftOp = (strcmp(op, "<<") == 0 || strcmp(op, ">>") == 0);
    
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 对于比较操作，不应该根据 resultType（布尔类型）来转换操作数
            // 比较操作的结果才是布尔类型，但操作数应该保持其原始数值类型
            // 实现完整的类型提升逻辑（例如：int 和 float 比较时，将 int 提升为 float）
            // 类型提升规则：
            // 1. 如果一个是float，另一个是int，将int提升为float
            // 2. 如果一个是更大的整数类型，将较小的整数类型提升为较大的类型
            // 3. 其他情况保持原类型
            EZrValueType leftValueType = leftType->baseType;
            EZrValueType rightValueType = rightType->baseType;
            
            // 检查是否需要类型提升
            if (ZR_VALUE_IS_TYPE_FLOAT(leftValueType) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValueType)) {
                // 左操作数是float，右操作数是int，将右操作数提升为float
                TZrUInt32 promotedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, promotedRightSlot, rightSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
                rightSlot = promotedRightSlot;
                effectiveRightType = ZR_VALUE_TYPE_DOUBLE;
            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValueType) && ZR_VALUE_IS_TYPE_FLOAT(rightValueType)) {
                // 左操作数是int，右操作数是float，将左操作数提升为float
                TZrUInt32 promotedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, promotedLeftSlot, leftSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
                leftSlot = promotedLeftSlot;
                effectiveLeftType = ZR_VALUE_TYPE_DOUBLE;
            } else if (ZR_VALUE_IS_TYPE_SIGNED_INT(leftValueType) && ZR_VALUE_IS_TYPE_SIGNED_INT(rightValueType)) {
                // 两个都是整数类型，提升到较大的类型
                // 类型提升顺序：INT8 < INT16 < INT32 < INT64
                // 注意：目前只有 TO_INT 指令（提升到 INT32），对于 INT16 和 INT64 的提升需要特殊处理
                if (leftValueType < rightValueType) {
                    // 左操作数类型较小，提升左操作数
                    // 如果目标是 INT32 或更大，使用 TO_INT 提升到 INT32
                    if (rightValueType == ZR_VALUE_TYPE_INT32 || rightValueType == ZR_VALUE_TYPE_INT64) {
                        if (leftValueType < ZR_VALUE_TYPE_INT32) {
                            TZrUInt32 promotedLeftSlot = allocate_stack_slot(cs);
                            emit_type_conversion(cs, promotedLeftSlot, leftSlot, ZR_INSTRUCTION_ENUM(TO_INT));
                            leftSlot = promotedLeftSlot;
                            effectiveLeftType = ZR_VALUE_TYPE_INT64;
                        }
                    }
                    // TODO: 对于 INT16 的提升，需要添加 TO_INT16 指令支持
                } else if (rightValueType < leftValueType) {
                    // 右操作数类型较小，提升右操作数
                    // 如果目标是 INT32 或更大，使用 TO_INT 提升到 INT32
                    if (leftValueType == ZR_VALUE_TYPE_INT32 || leftValueType == ZR_VALUE_TYPE_INT64) {
                        if (rightValueType < ZR_VALUE_TYPE_INT32) {
                            TZrUInt32 promotedRightSlot = allocate_stack_slot(cs);
                            emit_type_conversion(cs, promotedRightSlot, rightSlot, ZR_INSTRUCTION_ENUM(TO_INT));
                            rightSlot = promotedRightSlot;
                            effectiveRightType = ZR_VALUE_TYPE_INT64;
                        }
                    }
                    // TODO: 对于 INT16 的提升，需要添加 TO_INT16 指令支持
                }
            }
            // 其他情况（如都是float，或类型不兼容）保持原类型
        } else if (!isShiftOp) {
            // 对于非比较操作，根据结果类型进行转换
            // 检查左操作数是否需要转换
            EZrInstructionCode leftConvOp = ZrParser_InferredType_GetConversionOpcode(leftType, resultType);
            if (leftConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TZrUInt32 convertedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedLeftSlot, leftSlot, leftConvOp);
                leftSlot = convertedLeftSlot;
                effectiveLeftType = binary_expression_effective_type_after_conversion(leftType->baseType, leftConvOp);
            }
            
            // 检查右操作数是否需要转换
            EZrInstructionCode rightConvOp = ZrParser_InferredType_GetConversionOpcode(rightType, resultType);
            if (rightConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TZrUInt32 convertedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedRightSlot, rightSlot, rightConvOp);
                rightSlot = convertedRightSlot;
                effectiveRightType = binary_expression_effective_type_after_conversion(rightType->baseType, rightConvOp);
            }
            
            // 对于非比较操作，可以立即清理类型信息
            ZrParser_InferredType_Free(cs->state, leftType);
            ZrParser_InferredType_Free(cs->state, rightType);
        }
        // resultType 在比较操作中需要使用，所以稍后清理
    }
    
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD);  // 默认（通用加法指令）
    if (hasTypeInfo && resultType->typeName != ZR_NULL) {
        (void)note_inline_struct_result_slot(cs, destSlot, resultType->typeName);
    }
    
    // 根据操作符和类型选择指令
    // 临时调试输出：检查操作符字符串的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu), hasTypeInfo=%d\n", 
    //         op, (int)(unsigned char)op[0], strlen(op), hasTypeInfo);
    
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0 ||
        strcmp(op, "%") == 0 || strcmp(op, "**") == 0) {
        opcode = compiler_select_binary_arithmetic_opcode(
                op, hasTypeInfo, resultType->baseType, effectiveLeftType, effectiveRightType);
    } else if (strcmp(op, "<<") == 0) {
        opcode = hasTypeInfo && (ZR_VALUE_IS_TYPE_UNSIGNED_INT(effectiveLeftType) ||
                                 ZR_VALUE_IS_TYPE_UNSIGNED_INT(effectiveRightType))
                ? ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT)
                : ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT);
    } else if (strcmp(op, ">>") == 0) {
        opcode = hasTypeInfo && (ZR_VALUE_IS_TYPE_UNSIGNED_INT(effectiveLeftType) ||
                                 ZR_VALUE_IS_TYPE_UNSIGNED_INT(effectiveRightType))
                ? ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT)
                : ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT);
    } else if (strcmp(op, "==") == 0) {
        opcode = compiler_select_binary_equality_opcode(ZR_FALSE, hasTypeInfo, effectiveLeftType, effectiveRightType);
    } else if (strcmp(op, "!=") == 0) {
        opcode = compiler_select_binary_equality_opcode(ZR_TRUE, hasTypeInfo, effectiveLeftType, effectiveRightType);
    } else if (strcmp(op, "<") == 0) {
        // 根据操作数类型选择比较指令
        // 对于比较操作，使用 leftType 或 rightType 来判断操作数类型（它们应该相同或兼容）
        if (hasTypeInfo) {
            // 优先检查是否有浮点数类型
            if (leftType->baseType == ZR_VALUE_TYPE_FLOAT || leftType->baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType->baseType == ZR_VALUE_TYPE_FLOAT || rightType->baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType->baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType->baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);  // 默认使用有符号整数比较
        }
    } else if (strcmp(op, ">") == 0) {
        if (hasTypeInfo) {
            if (leftType->baseType == ZR_VALUE_TYPE_FLOAT || leftType->baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType->baseType == ZR_VALUE_TYPE_FLOAT || rightType->baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType->baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType->baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
        }
    } else if (strcmp(op, "<=") == 0) {
        if (hasTypeInfo) {
            if (leftType->baseType == ZR_VALUE_TYPE_FLOAT || leftType->baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType->baseType == ZR_VALUE_TYPE_FLOAT || rightType->baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType->baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType->baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
        }
    } else if (strcmp(op, ">=") == 0) {
        if (hasTypeInfo) {
            if (leftType->baseType == ZR_VALUE_TYPE_FLOAT || leftType->baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType->baseType == ZR_VALUE_TYPE_FLOAT || rightType->baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType->baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType->baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED);
        }
    } else if (strcmp(op, "&&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_AND);
    } else if (strcmp(op, "||") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_OR);
    } else if (strcmp(op, "&") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_AND);
    } else if (strcmp(op, "|") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_OR);
    } else if (strcmp(op, "^") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(BITWISE_XOR);
    } else {
        ZrParser_Compiler_Error(cs, "Unknown binary operator", node->location);
        return;
    }
    
    EZrCompilerSemanticCompareResult comparisonResult =
            compiler_semantic_compare_lower(cs, node, opcode,
                    leftSlot, rightSlot, destSlot,
                    hasTypeInfo ? resultType : ZR_NULL);
    if (comparisonResult == ZR_COMPILER_SEMANTIC_COMPARE_NOT_APPLICABLE &&
        !compiler_semantic_ir_lower_binary(
                cs, opcode, leftSlot, rightSlot, destSlot,
                hasTypeInfo ? resultType : ZR_NULL, node->location)) {
        TZrInstruction inst = create_instruction_2(
                opcode,
                ZR_COMPILE_SLOT_U16(destSlot),
                ZR_COMPILE_SLOT_U16(leftSlot),
                ZR_COMPILE_SLOT_U16(rightSlot));
        emit_instruction(cs, inst);
    }
    collapse_stack_to_slot(cs, destSlot);
    
    // 清理类型信息
    if (hasTypeInfo) {
        if (isComparisonOp || isShiftOp) {
            // 比较/位移操作保留操作数原始类型到指令选择阶段后再清理
            ZrParser_InferredType_Free(cs->state, leftType);
            ZrParser_InferredType_Free(cs->state, rightType);
        }
        ZrParser_InferredType_Free(cs->state, resultType);
    }
}

static void compile_binary_expression_from_slots(
        SZrCompilerState *cs, SZrAstNode *node,
        TZrUInt32 leftSlot, TZrUInt32 rightSlot) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BINARY_EXPRESSION) {
        compile_binary_expression_error(cs, node, "Expected binary expression");
        return;
    }
    
    const TZrChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    // 调试：检查操作符字符串
    if (op == ZR_NULL) {
        compile_binary_expression_error(cs, node, "Binary operator string is NULL");
        return;
    }
    
    // 临时调试：输出操作符字符串的实际值
    // 注意：这里使用 fprintf 来调试，确认 op 的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu)\n", 
    //         op, (int)(unsigned char)op[0], strlen(op));
    
    // 推断左右操作数的类型
    SZrInferredType leftType, rightType, resultType;
    EZrValueType effectiveLeftType = ZR_VALUE_TYPE_OBJECT;
    EZrValueType effectiveRightType = ZR_VALUE_TYPE_OBJECT;
    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &rightType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &resultType, ZR_VALUE_TYPE_OBJECT);
    TZrBool hasTypeInfo = ZR_FALSE;
    if (cs->typeEnv != ZR_NULL) {
        if (ZrParser_ExpressionType_Infer(cs, left, &leftType) && ZrParser_ExpressionType_Infer(cs, right, &rightType)) {
            hasTypeInfo = ZR_TRUE;
            effectiveLeftType = leftType.baseType;
            effectiveRightType = rightType.baseType;
            // 推断结果类型
            if (!ZrParser_BinaryExpressionType_Infer(cs, node, &resultType)) {
                // 类型推断失败，使用默认类型
                ZrParser_InferredType_Init(cs->state, &resultType, ZR_VALUE_TYPE_OBJECT);
            }
        } else {
            // 类型推断失败，清理已推断的类型
            if (ZrParser_ExpressionType_Infer(cs, left, &leftType)) {
                ZrParser_InferredType_Free(cs->state, &leftType);
            }
            if (ZrParser_ExpressionType_Infer(cs, right, &rightType)) {
                ZrParser_InferredType_Free(cs->state, &rightType);
            }
        }
    }
    
    // 编译左操作数
    if (leftSlot == ZR_PARSER_SLOT_NONE) {
        compile_expression_non_tail(cs, left);
        leftSlot = cs->lastExpressionSlot;
    }
    if (leftSlot == ZR_PARSER_SLOT_NONE) {
        compile_binary_expression_error(cs, node, "Failed to compile left operand");
        return;
    }
    
    // 编译右操作数
    if (rightSlot == ZR_PARSER_SLOT_NONE) {
        compile_expression_non_tail(cs, right);
        rightSlot = cs->lastExpressionSlot;
    }
    if (rightSlot == ZR_PARSER_SLOT_NONE) {
        compile_binary_expression_error(cs, node, "Failed to compile right operand");
        return;
    }
    
    compile_binary_expression_emit(
            cs, node, op, leftSlot, rightSlot,
            &leftType, &rightType, &resultType,
            hasTypeInfo, effectiveLeftType, effectiveRightType);
}

/* Cached pure operator trees have already been inferred. Compile operands in
 * a small frame before retaining the three large inferred types for emission. */
void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    TZrUInt32 leftSlot;
    TZrUInt32 rightSlot;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) return;
    if (cs->expressionTypeCache == ZR_NULL ||
        cs->expressionTypeCache->environment != cs->typeEnv) {
        compile_binary_expression_from_slots(cs, node, ZR_PARSER_SLOT_NONE, ZR_PARSER_SLOT_NONE);
        return;
    }
    compile_expression_non_tail(cs, node->data.binaryExpression.left);
    leftSlot = cs->lastExpressionSlot;
    if (cs->hasError || leftSlot == ZR_PARSER_SLOT_NONE) return;
    compile_expression_non_tail(cs, node->data.binaryExpression.right);
    rightSlot = cs->lastExpressionSlot;
    if (cs->hasError || rightSlot == ZR_PARSER_SLOT_NONE) return;
    compile_binary_expression_from_slots(cs, node, leftSlot, rightSlot);
}

