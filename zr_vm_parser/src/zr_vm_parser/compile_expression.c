//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"

static void compile_unary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_UNARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected unary expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.unaryExpression.op.op;
    SZrAstNode *arg = node->data.unaryExpression.argument;
    
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    if (strcmp(op, "$") == 0 || strcmp(op, "new") == 0) {
        ZrParser_Compiler_Error(cs,
                                "Legacy unary constructor syntax is no longer supported; use $target(...) or new target(...)",
                                node->location);
        return;
    } else {
        // 其他一元操作符：先编译操作数
        compile_expression_non_tail(cs, arg);
        TZrUInt32 argSlot = cs->stackSlotCount - 1;
        
        if (strcmp(op, "!") == 0) {
            // 逻辑非
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(LOGICAL_NOT), destSlot, (TZrUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "~") == 0) {
            // 位非
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(BITWISE_NOT), destSlot, (TZrUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "-") == 0) {
            // 取负
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(NEG), destSlot, (TZrUInt16)argSlot, 0);
            emit_instruction(cs, inst);
        } else if (strcmp(op, "+") == 0) {
            // 正号：直接使用操作数（不需要额外指令）
            // 将结果复制到目标槽位
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), destSlot, (TZrInt32)argSlot);
            emit_instruction(cs, inst);
        } else {
            ZrParser_Compiler_Error(cs, "Unknown unary operator", node->location);
        }

        if (!cs->hasError) {
            collapse_stack_to_slot(cs, destSlot);
        }
    }
}

// 编译类型转换表达式
static void compile_type_cast_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_TYPE_CAST_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected type cast expression", node->location);
        return;
    }
    
    SZrType *targetType = node->data.typeCastExpression.targetType;
    SZrAstNode *expression = node->data.typeCastExpression.expression;
    
    if (targetType == ZR_NULL || expression == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Invalid type cast expression", node->location);
        return;
    }
    
    // 先编译要转换的表达式
    compile_expression_non_tail(cs, expression);
    
    TZrUInt32 srcSlot = cs->stackSlotCount - 1;
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    // 根据目标类型生成相应的转换指令
    // 首先检查类型名称
    if (targetType->name != ZR_NULL && targetType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = targetType->name->data.identifier.name;
        TZrNativeString typeNameStr;
        TZrSize nameLen;
        
        if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
            typeNameStr = ZrCore_String_GetNativeStringShort(typeName);
            nameLen = typeName->shortStringLength;
        } else {
            typeNameStr = ZrCore_String_GetNativeString(typeName);
            nameLen = typeName->longStringLength;
        }
        
        // 检查基本类型
        if (nameLen == 3 && strncmp(typeNameStr, "int", 3) == 0) {
            // 转换为 int
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_INT));
            return;
        } else if (nameLen == 5 && strncmp(typeNameStr, "float", 5) == 0) {
            // 转换为 float
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_FLOAT));
            return;
        } else if (nameLen == 6 && strncmp(typeNameStr, "string", 6) == 0) {
            // 转换为 string
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
            return;
        } else if (nameLen == 4 && strncmp(typeNameStr, "bool", 4) == 0) {
            // 转换为 bool
            emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_BOOL));
            return;
        }
        
        // 对于 struct 和 class 类型，查找类型定义
        SZrAstNode *typeDecl = find_type_declaration(cs, typeName);
        if (typeDecl != ZR_NULL) {
            // 将类型名称作为常量存储（运行时通过类型名称查找或创建原型）
            SZrTypeValue typeNameValue;
            ZrCore_Value_ResetAsNull(&typeNameValue);
            ZrCore_Value_InitAsRawObject(cs->state, &typeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(typeName));
            typeNameValue.type = ZR_VALUE_TYPE_STRING;
            TZrUInt32 typeNameConstantIndex = add_constant(cs, &typeNameValue);
            
            // 根据类型声明类型生成相应的转换指令
            if (typeDecl->type == ZR_AST_STRUCT_DECLARATION) {
                // 生成 TO_STRUCT 指令，将类型名称常量索引作为操作数
                emit_type_conversion_with_prototype(cs, destSlot, srcSlot, 
                                                    ZR_INSTRUCTION_ENUM(TO_STRUCT), 
                                                    typeNameConstantIndex);
                return;
            } else if (typeDecl->type == ZR_AST_CLASS_DECLARATION) {
                // 生成 TO_OBJECT 指令，将类型名称常量索引作为操作数
                emit_type_conversion_with_prototype(cs, destSlot, srcSlot, 
                                                    ZR_INSTRUCTION_ENUM(TO_OBJECT), 
                                                    typeNameConstantIndex);
                return;
            }
        }
        
        // 如果未找到类型定义，使用 TO_STRING 作为默认转换
        // 可能需要支持从其他模块导入的类型
        // 注意：从其他模块导入的类型在运行时通过模块系统查找
        // 编译器无法在编译期确定所有类型，因此使用运行时类型查找
        // 如果类型未在当前模块找到，运行时会在全局模块注册表中查找
        emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    } else {
        // TODO: 对于复杂类型（泛型、元组等），暂时使用 TO_STRING
        // 实现完整的类型转换逻辑
        // 注意：复杂类型的转换需要根据具体类型实现
        // 泛型类型转换需要实例化类型参数
        // 元组类型转换需要逐个元素转换
        // TODO: 这里暂时使用TO_STRING作为默认转换，未来可以扩展支持更多类型
        emit_type_conversion(cs, destSlot, srcSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    }
}

// 编译二元表达式
static void compile_binary_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_BINARY_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected binary expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.binaryExpression.op.op;
    SZrAstNode *left = node->data.binaryExpression.left;
    SZrAstNode *right = node->data.binaryExpression.right;
    // 调试：检查操作符字符串
    if (op == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Binary operator string is NULL", node->location);
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
    if (cs->typeEnv != ZR_NULL &&
        !expression_uses_dynamic_object_access(left) &&
        !expression_uses_dynamic_object_access(right)) {
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
    compile_expression_non_tail(cs, left);
    TZrUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 编译右操作数
    compile_expression_non_tail(cs, right);
    TZrUInt32 rightSlot = cs->stackSlotCount - 1;
    
    // 如果需要类型转换，插入转换指令
    // 注意：对于比较操作，需要保留 leftType 和 rightType 用于选择正确的比较指令
    TZrBool isComparisonOp = (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 || 
                            strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                            strcmp(op, "==") == 0 || strcmp(op, "!=") == 0);
    
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 对于比较操作，不应该根据 resultType（布尔类型）来转换操作数
            // 比较操作的结果才是布尔类型，但操作数应该保持其原始数值类型
            // 实现完整的类型提升逻辑（例如：int 和 float 比较时，将 int 提升为 float）
            // 类型提升规则：
            // 1. 如果一个是float，另一个是int，将int提升为float
            // 2. 如果一个是更大的整数类型，将较小的整数类型提升为较大的类型
            // 3. 其他情况保持原类型
            EZrValueType leftValueType = leftType.baseType;
            EZrValueType rightValueType = rightType.baseType;
            
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
        } else {
            // 对于非比较操作，根据结果类型进行转换
            // 检查左操作数是否需要转换
            EZrInstructionCode leftConvOp = ZrParser_InferredType_GetConversionOpcode(&leftType, &resultType);
            if (leftConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TZrUInt32 convertedLeftSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedLeftSlot, leftSlot, leftConvOp);
                leftSlot = convertedLeftSlot;
                effectiveLeftType = binary_expression_effective_type_after_conversion(leftType.baseType, leftConvOp);
            }
            
            // 检查右操作数是否需要转换
            EZrInstructionCode rightConvOp = ZrParser_InferredType_GetConversionOpcode(&rightType, &resultType);
            if (rightConvOp != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
                TZrUInt32 convertedRightSlot = allocate_stack_slot(cs);
                emit_type_conversion(cs, convertedRightSlot, rightSlot, rightConvOp);
                rightSlot = convertedRightSlot;
                effectiveRightType = binary_expression_effective_type_after_conversion(rightType.baseType, rightConvOp);
            }
            
            // 对于非比较操作，可以立即清理类型信息
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
        }
        // resultType 在比较操作中需要使用，所以稍后清理
    }
    
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD);  // 默认（通用加法指令）
    
    // 根据操作符和类型选择指令
    // 临时调试输出：检查操作符字符串的实际值
    // fprintf(stderr, "DEBUG: Binary operator op='%s' (first char=%d, len=%zu), hasTypeInfo=%d\n", 
    //         op, (int)(unsigned char)op[0], strlen(op), hasTypeInfo);
    
    if (strcmp(op, "+") == 0) {
        // 根据结果类型选择 ADD/ADD_INT/ADD_FLOAT/ADD_STRING
        if (!hasTypeInfo) {
            // 类型不明确，使用通用 ADD 指令
            opcode = ZR_INSTRUCTION_ENUM(ADD);
        } else if (resultType.baseType == ZR_VALUE_TYPE_STRING ||
                   effectiveLeftType == ZR_VALUE_TYPE_STRING ||
                   effectiveRightType == ZR_VALUE_TYPE_STRING) {
            opcode = ZR_INSTRUCTION_ENUM(ADD_STRING);
        } else if (binary_expression_type_is_float_like(resultType.baseType) ||
                   binary_expression_type_is_float_like(effectiveLeftType) ||
                   binary_expression_type_is_float_like(effectiveRightType)) {
            opcode = ZR_INSTRUCTION_ENUM(ADD_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
        }
    } else if (strcmp(op, "-") == 0) {
        if (!hasTypeInfo) {
            // 类型不明确，使用通用 SUB 指令
            opcode = ZR_INSTRUCTION_ENUM(SUB);
        } else if (binary_expression_type_is_float_like(resultType.baseType) ||
                   binary_expression_type_is_float_like(effectiveLeftType) ||
                   binary_expression_type_is_float_like(effectiveRightType)) {
            opcode = ZR_INSTRUCTION_ENUM(SUB_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
        }
    } else if (strcmp(op, "*") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(MUL_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
        }
    } else if (strcmp(op, "/") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(DIV_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
        }
    } else if (strcmp(op, "%") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(MOD_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
        }
    } else if (strcmp(op, "**") == 0) {
        if (hasTypeInfo &&
            (binary_expression_type_is_float_like(resultType.baseType) ||
             binary_expression_type_is_float_like(effectiveLeftType) ||
             binary_expression_type_is_float_like(effectiveRightType))) {
            opcode = ZR_INSTRUCTION_ENUM(POW_FLOAT);
        } else {
            opcode = ZR_INSTRUCTION_ENUM(POW_SIGNED);
        }
    } else if (strcmp(op, "<<") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT);
    } else if (strcmp(op, ">>") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT);
    } else if (strcmp(op, "==") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL);
    } else if (strcmp(op, "!=") == 0) {
        opcode = ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL);
    } else if (strcmp(op, "<") == 0) {
        // 根据操作数类型选择比较指令
        // 对于比较操作，使用 leftType 或 rightType 来判断操作数类型（它们应该相同或兼容）
        if (hasTypeInfo) {
            // 优先检查是否有浮点数类型
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED);  // 默认使用有符号整数比较
        }
    } else if (strcmp(op, ">") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED);
        }
    } else if (strcmp(op, "<=") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED);
            } else {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
            }
        } else {
            opcode = ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED);
        }
    } else if (strcmp(op, ">=") == 0) {
        if (hasTypeInfo) {
            if (leftType.baseType == ZR_VALUE_TYPE_FLOAT || leftType.baseType == ZR_VALUE_TYPE_DOUBLE ||
                rightType.baseType == ZR_VALUE_TYPE_FLOAT || rightType.baseType == ZR_VALUE_TYPE_DOUBLE) {
                opcode = ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT);
            } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(leftType.baseType) || ZR_VALUE_IS_TYPE_UNSIGNED_INT(rightType.baseType)) {
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
    
    TZrInstruction inst = create_instruction_2(opcode, destSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
    emit_instruction(cs, inst);
    
    // 清理类型信息
    if (hasTypeInfo) {
        if (isComparisonOp) {
            // 比较操作：清理 leftType, rightType 和 resultType
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
        }
        ZrParser_InferredType_Free(cs->state, &resultType);
    }
}

// 辅助函数：检查变量名是否在 const 数组中
static TZrBool is_const_variable(SZrCompilerState *cs, SZrString *name, SZrArray *constArray) {
    if (cs == ZR_NULL || name == ZR_NULL || constArray == ZR_NULL) {
        return ZR_FALSE;
    }
    
    for (TZrSize i = 0; i < constArray->length; i++) {
        SZrString **varNamePtr = (SZrString **)ZrCore_Array_Get(constArray, i);
        if (varNamePtr != ZR_NULL && *varNamePtr != ZR_NULL) {
            if (ZrCore_String_Equal(*varNamePtr, name)) {
                return ZR_TRUE;
            }
        }
    }
    
    return ZR_FALSE;
}

// 编译赋值表达式
static void compile_assignment_expression(SZrCompilerState *cs, SZrAstNode *node) {
    EZrInstructionCode assignmentConversionOpcode = ZR_INSTRUCTION_ENUM(ENUM_MAX);
    SZrInferredType leftType;
    SZrInferredType rightType;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ASSIGNMENT_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected assignment expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.assignmentExpression.op.op;
    SZrAstNode *left = node->data.assignmentExpression.left;
    SZrAstNode *right = node->data.assignmentExpression.right;

    ZrParser_InferredType_Init(cs->state, &leftType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(cs->state, &rightType, ZR_VALUE_TYPE_OBJECT);
    if (strcmp(op, "=") == 0 &&
        ZrParser_ExpressionType_Infer(cs, left, &leftType) &&
        ZrParser_ExpressionType_Infer(cs, right, &rightType)) {
        if (!ZrParser_AssignmentCompatibility_Check(cs, &leftType, &rightType, node->location)) {
            ZrParser_InferredType_Free(cs->state, &leftType);
            ZrParser_InferredType_Free(cs->state, &rightType);
            return;
        }
        assignmentConversionOpcode = ZrParser_InferredType_GetConversionOpcode(&rightType, &leftType);
    }
    ZrParser_InferredType_Free(cs->state, &leftType);
    ZrParser_InferredType_Free(cs->state, &rightType);
    
    // 编译右值
    ZrParser_Expression_Compile(cs, right);
    TZrUInt32 rightSlot = cs->stackSlotCount - 1;
    if (!cs->hasError && assignmentConversionOpcode != ZR_INSTRUCTION_ENUM(ENUM_MAX)) {
        emit_type_conversion(cs, rightSlot, rightSlot, assignmentConversionOpcode);
    }
    
    // 处理左值（标识符）
    if (left->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = left->data.identifier.name;
        
        // 检查是否是 const 函数参数
        if (is_const_variable(cs, name, &cs->constParameters)) {
            TZrChar errorMsg[256];
            TZrNativeString nameStr = ZrCore_String_GetNativeStringShort(name);
            if (nameStr != ZR_NULL) {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const parameter '%s'", nameStr);
            } else {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const parameter");
            }
            ZrParser_Compiler_Error(cs, errorMsg, node->location);
            return;
        }
        
        // 检查是否是 const 局部变量
        if (is_const_variable(cs, name, &cs->constLocalVars)) {
            TZrChar errorMsg[256];
            TZrNativeString nameStr = ZrCore_String_GetNativeStringShort(name);
            if (nameStr != ZR_NULL) {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const variable '%s' after declaration", nameStr);
            } else {
                snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const variable after declaration");
            }
            ZrParser_Compiler_Error(cs, errorMsg, node->location);
            return;
        }
        
        TZrUInt32 localVarIndex = find_local_var(cs, name);
        
        if (localVarIndex != (TZrUInt32)-1) {
            // 简单赋值
            if (strcmp(op, "=") == 0) {
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)localVarIndex, (TZrInt32)rightSlot);
                emit_instruction(cs, inst);
                update_identifier_assignment_type_environment(cs, name, right);
            } else {
                // 复合赋值：先读取左值，执行运算，再赋值
                TZrUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_STACK), leftSlot, (TZrInt32)localVarIndex);
                emit_instruction(cs, getInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 赋值
                TZrInstruction setInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)localVarIndex, (TZrInt32)resultSlot);
                emit_instruction(cs, setInst);
            }
        } else {
            // 查找闭包变量
            TZrUInt32 closureVarIndex = find_closure_var(cs, name);
            if (closureVarIndex != (TZrUInt32)-1) {
                // 获取闭包变量信息以检查 inStack 标志
                SZrFunctionClosureVariable *closureVar = (SZrFunctionClosureVariable *)ZrCore_Array_Get(&cs->closureVars, closureVarIndex);
                TZrBool useUpval = (closureVar != ZR_NULL && closureVar->inStack);
                
                // 闭包变量：根据 inStack 标志选择使用 SET_CLOSURE 还是 SETUPVAL
                // 简单赋值
                if (strcmp(op, "=") == 0) {
                    if (useUpval) {
                        // SETUPVAL 格式: operandExtra = sourceSlot, operand1[0] = closureVarIndex, operand1[1] = 0
                        TZrInstruction setUpvalInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), (TZrUInt16)rightSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setUpvalInst);
                    } else {
                        // SET_CLOSURE 格式: operandExtra = sourceSlot, operand1[0] = closureVarIndex, operand1[1] = 0
                        TZrInstruction setClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE), (TZrUInt16)rightSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setClosureInst);
                    }
                    update_identifier_assignment_type_environment(cs, name, right);
                } else {
                    // 复合赋值：先读取，执行运算，再写入
                    TZrUInt32 leftSlot = allocate_stack_slot(cs);
                    if (useUpval) {
                        TZrInstruction getUpvalInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), (TZrUInt16)leftSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, getUpvalInst);
                    } else {
                        TZrInstruction getClosureInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_CLOSURE), (TZrUInt16)leftSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, getClosureInst);
                    }
                    
                    // 执行运算
                    EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                    if (strcmp(op, "+=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                    } else if (strcmp(op, "-=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                    } else if (strcmp(op, "*=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                    } else if (strcmp(op, "/=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                    } else if (strcmp(op, "%=") == 0) {
                        opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                    }
                    
                    TZrUInt32 resultSlot = allocate_stack_slot(cs);
                    TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
                    emit_instruction(cs, opInst);
                    
                    // 写入闭包变量
                    if (useUpval) {
                        TZrInstruction setUpvalInst2 = create_instruction_2(ZR_INSTRUCTION_ENUM(SETUPVAL), (TZrUInt16)resultSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setUpvalInst2);
                    } else {
                        TZrInstruction setClosureInst2 = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_CLOSURE), (TZrUInt16)resultSlot, (TZrUInt16)closureVarIndex, 0);
                        emit_instruction(cs, setClosureInst2);
                    }
                    
                    // 释放临时栈槽
                    ZrParser_Compiler_TrimStackBy(cs, 2); // leftSlot 和 resultSlot
                }
                return;
            }
            
            // 尝试作为全局变量访问（使用 SET_TABLE）
            // 1. 获取全局对象（zr 对象）
            TZrUInt32 globalSlot = allocate_stack_slot(cs);
            TZrInstruction getGlobalInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)globalSlot, 0);
            emit_instruction(cs, getGlobalInst);
            
            // 2. 将变量名转换为字符串常量并压入栈
            SZrTypeValue nameValue;
            ZrCore_Value_InitAsRawObject(cs->state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(name));
            nameValue.type = ZR_VALUE_TYPE_STRING;
            TZrUInt32 nameConstantIndex = add_constant(cs, &nameValue);
            TZrUInt32 keySlot = allocate_stack_slot(cs);
            TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)keySlot, (TZrInt32)nameConstantIndex);
            emit_instruction(cs, getKeyInst);
            
            // 对于复合赋值，需要先读取值，执行运算，再写入
            if (strcmp(op, "=") != 0) {
                // 读取全局变量值
                TZrUInt32 leftSlot = allocate_stack_slot(cs);
                TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)leftSlot, (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
                emit_instruction(cs, getTableInst);
                
                // 执行运算
                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                if (strcmp(op, "+=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                } else if (strcmp(op, "-=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                } else if (strcmp(op, "*=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                } else if (strcmp(op, "/=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                } else if (strcmp(op, "%=") == 0) {
                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                }
                
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftSlot, (TZrUInt16)rightSlot);
                emit_instruction(cs, opInst);
                
                // 写入全局变量（使用 SET_TABLE）
                // SET_TABLE 格式: operandExtra = destSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)resultSlot, (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
                emit_instruction(cs, setTableInst);
                
                // 释放临时栈槽
                ZrParser_Compiler_TrimStackBy(cs, 3); // leftSlot, resultSlot, globalSlot, keySlot (但 resultSlot 会被保留)
            } else {
                // 简单赋值：直接使用 SET_TABLE
                // SET_TABLE 格式: operandExtra = destSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)rightSlot, (TZrUInt16)globalSlot, (TZrUInt16)keySlot);
                emit_instruction(cs, setTableInst);
                update_identifier_assignment_type_environment(cs, name, right);
                
                // 释放临时栈槽
                ZrParser_Compiler_TrimStackBy(cs, 2); // globalSlot 和 keySlot
            }
        }
    } else {
        // 处理成员访问等复杂左值
        // 支持 obj.prop = value 和 arr[index] = value
        // 注意：成员访问在 primary expression 中处理，这里需要处理 primary expression 作为左值
        if (left->type == ZR_AST_PRIMARY_EXPRESSION) {
            SZrPrimaryExpression *primary = &left->data.primaryExpression;
            
            // 检查是否是成员访问（members 数组不为空且最后一个成员是 MemberExpression）
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                SZrAstNode *lastMember = primary->members->nodes[primary->members->count - 1];
                if (lastMember != ZR_NULL && lastMember->type == ZR_AST_MEMBER_EXPRESSION) {
                    // 编译整个 primary expression 以获取对象和键
                    // 先编译基础属性（对象）
                    if (primary->property != ZR_NULL) {
                        ZrParser_Expression_Compile(cs, primary->property);
                        TZrUInt32 objSlot = cs->stackSlotCount - 1;
                        
                        // 处理成员访问链，获取最后一个成员访问的键
                        SZrMemberExpression *memberExpr = &lastMember->data.memberExpression;
                        if (memberExpr->property != ZR_NULL) {
                            // 检查成员字段是否是 const
                            // 如果字段是 const 且当前不在构造函数中，报告错误
                            SZrString *rootTypeName = ZR_NULL;
                            TZrBool rootIsTypeReference = ZR_FALSE;
                            resolve_expression_root_type(cs, primary->property, &rootTypeName, &rootIsTypeReference);

                            if (memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                                SZrString *fieldName = memberExpr->property->data.identifier.name;
                                if (fieldName != ZR_NULL && !cs->isInConstructor) {
                                    // 查找类型定义，检查字段是否是 const
                                    // 尝试从 primary->property 推断类型（如果是 this）
                                    if (primary->property != ZR_NULL && 
                                        primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                                        SZrString *objName = primary->property->data.identifier.name;
                                        if (objName != ZR_NULL) {
                                            TZrNativeString objNameStr = ZrCore_String_GetNativeStringShort(objName);
                                            if (objNameStr != ZR_NULL && strcmp(objNameStr, "this") == 0) {
                                                // 这是 this.field 的赋值
                                                // 使用当前类型名称查找字段是否是 const
                                                if (cs->currentTypeName != ZR_NULL) {
                                                    if (find_type_member_is_const(cs, cs->currentTypeName, fieldName)) {
                                                        TZrChar errorMsg[256];
                                                        TZrNativeString fieldNameStr = ZrCore_String_GetNativeStringShort(fieldName);
                                                        if (fieldNameStr != ZR_NULL) {
                                                            snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const field '%s' outside constructor", fieldNameStr);
                                                        } else {
                                                            snprintf(errorMsg, sizeof(errorMsg), "Cannot assign to const field outside constructor");
                                                        }
                                                        ZrParser_Compiler_Error(cs, errorMsg, node->location);
                                                        return;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                SZrTypeMemberInfo *setterAccessor =
                                        find_hidden_property_accessor_member(cs, rootTypeName, fieldName, ZR_TRUE);
                                if (can_use_property_accessor(rootIsTypeReference, setterAccessor)) {
                                    if (strcmp(op, "=") != 0) {
                                        ZrParser_Compiler_Error(cs,
                                                        "Compound assignment to property accessors is not supported yet",
                                                        node->location);
                                        return;
                                    }

                                    if (emit_property_setter_call(cs, objSlot, fieldName, setterAccessor->isStatic,
                                                                  rightSlot, node->location) == (TZrUInt32)-1) {
                                        return;
                                    }
                                    return;
                                }
                            }
                            
                            TZrUInt32 keySlot = compile_member_key_into_slot(cs, memberExpr, objSlot + 1);
                            if (keySlot == (TZrUInt32)-1) {
                                return;
                            }
                            
                            // 使用 SETTABLE 设置对象属性
                            // SETTABLE 格式: operandExtra = valueSlot, operand1[0] = tableSlot, operand1[1] = keySlot
                            if (strcmp(op, "=") == 0) {
                                // 简单赋值
                                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)rightSlot, (TZrUInt16)objSlot, (TZrUInt16)keySlot);
                                emit_instruction(cs, setTableInst);
                            } else {
                                // 复合赋值：先读取，执行运算，再写入
                                TZrUInt32 leftValueSlot = allocate_stack_slot(cs);
                                TZrInstruction getTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETTABLE), (TZrUInt16)leftValueSlot, (TZrUInt16)objSlot, (TZrUInt16)keySlot);
                                emit_instruction(cs, getTableInst);
                                
                                // 执行运算
                                EZrInstructionCode opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                                if (strcmp(op, "+=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(ADD_INT);
                                } else if (strcmp(op, "-=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(SUB_INT);
                                } else if (strcmp(op, "*=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(MUL_SIGNED);
                                } else if (strcmp(op, "/=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(DIV_SIGNED);
                                } else if (strcmp(op, "%=") == 0) {
                                    opcode = ZR_INSTRUCTION_ENUM(MOD_SIGNED);
                                }
                                
                                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                                TZrInstruction opInst = create_instruction_2(opcode, resultSlot, (TZrUInt16)leftValueSlot, (TZrUInt16)rightSlot);
                                emit_instruction(cs, opInst);
                                
                                // 使用 SETTABLE 写入结果
                                TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SETTABLE), (TZrUInt16)resultSlot, (TZrUInt16)objSlot, (TZrUInt16)keySlot);
                                emit_instruction(cs, setTableInst);
                                
                                // 释放临时栈槽
                                ZrParser_Compiler_TrimStackBy(cs, 2); // leftValueSlot 和 resultSlot
                            }
                            
                            // 释放临时栈槽
                            ZrParser_Compiler_TrimStackBy(cs, 2); // objSlot 和 keySlot
                            return;
                        }
                    }
                }
            }
        }
        
        // 其他复杂左值暂不支持
        ZrParser_Compiler_Error(cs, "Complex left-hand side not supported yet", node->location);
    }
}

// 编译逻辑表达式（&& 和 ||，支持短路求值）
static void compile_logical_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_LOGICAL_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected logical expression", node->location);
        return;
    }
    
    const TZrChar *op = node->data.logicalExpression.op;
    SZrAstNode *left = node->data.logicalExpression.left;
    SZrAstNode *right = node->data.logicalExpression.right;
    
    // 编译左操作数
    compile_expression_non_tail(cs, left);
    TZrUInt32 leftSlot = cs->stackSlotCount - 1;
    
    // 分配结果槽位
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    // 创建标签用于短路求值
    TZrSize shortCircuitLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    if (strcmp(op, "&&") == 0) {
        // && 运算符：如果左操作数为false，短路返回false
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为false，跳转到短路标签
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, shortCircuitLabelId);
        
        // 编译右操作数
        compile_expression_non_tail(cs, right);
        TZrUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 跳转到结束标签
        TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
        TZrSize jumpEndIndex = cs->instructionCount;
        emit_instruction(cs, jumpEndInst);
        add_pending_jump(cs, jumpEndIndex, endLabelId);
        
        // 短路标签：左操作数为false，结果就是false（已经在destSlot中）
        resolve_label(cs, shortCircuitLabelId);
        
        // 结束标签
        resolve_label(cs, endLabelId);
    } else if (strcmp(op, "||") == 0) {
        // || 运算符：如果左操作数为true，短路返回true
        // 复制左操作数到结果槽位
        TZrInstruction copyLeftInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)leftSlot);
        emit_instruction(cs, copyLeftInst);
        
        // 如果左操作数为true，跳转到结束标签（短路返回true）
        TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)leftSlot, 0);
        TZrSize jumpIfIndex = cs->instructionCount;
        emit_instruction(cs, jumpIfInst);
        add_pending_jump(cs, jumpIfIndex, endLabelId);
        
        // 左操作数为false，需要计算右操作数
        // 编译右操作数
        compile_expression_non_tail(cs, right);
        TZrUInt32 rightSlot = cs->stackSlotCount - 1;
        
        // 将右操作数复制到结果槽位
        TZrInstruction copyRightInst = create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK), (TZrUInt16)destSlot, (TZrInt32)rightSlot);
        emit_instruction(cs, copyRightInst);
        
        // 结束标签：左操作数为true时跳转到这里（结果已经是true）
        resolve_label(cs, endLabelId);
    } else {
        ZrParser_Compiler_Error(cs, "Unknown logical operator", node->location);
        return;
    }
}

// 编译条件表达式（三元运算符）
static void compile_conditional_expression(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CONDITIONAL_EXPRESSION) {
        ZrParser_Compiler_Error(cs, "Expected conditional expression", node->location);
        return;
    }
    
    SZrAstNode *test = node->data.conditionalExpression.test;
    SZrAstNode *consequent = node->data.conditionalExpression.consequent;
    SZrAstNode *alternate = node->data.conditionalExpression.alternate;
    
    // 编译条件
    compile_expression_non_tail(cs, test);
    TZrUInt32 testSlot = cs->stackSlotCount - 1;
    
    // 创建 else 和 end 标签
    TZrSize elseLabelId = create_label(cs);
    TZrSize endLabelId = create_label(cs);
    
    // JUMP_IF false -> else
    TZrInstruction jumpIfInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP_IF), (TZrUInt16)testSlot, 0);
    TZrSize jumpIfIndex = cs->instructionCount;
    emit_instruction(cs, jumpIfInst);
    add_pending_jump(cs, jumpIfIndex, elseLabelId);
    
    // 编译 then 分支
    compile_expression_non_tail(cs, consequent);
    
    // JUMP -> end
    TZrInstruction jumpEndInst = create_instruction_1(ZR_INSTRUCTION_ENUM(JUMP), 0, 0);
    TZrSize jumpEndIndex = cs->instructionCount;
    emit_instruction(cs, jumpEndInst);
    add_pending_jump(cs, jumpEndIndex, endLabelId);
    
    // 解析 else 标签
    resolve_label(cs, elseLabelId);
    
    // 编译 else 分支
    compile_expression_non_tail(cs, alternate);
    
    // 解析 end 标签
    resolve_label(cs, endLabelId);
}

// 在脚本 AST 中查找函数声明

ZR_PARSER_API void ZrParser_Expression_Compile(SZrCompilerState *cs, SZrAstNode *node) {
    SZrAstNode *oldCurrentAst;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    oldCurrentAst = cs->currentAst;
    cs->currentAst = node;
    
    switch (node->type) {
        // 字面量
        case ZR_AST_BOOLEAN_LITERAL:
        case ZR_AST_INTEGER_LITERAL:
        case ZR_AST_FLOAT_LITERAL:
        case ZR_AST_STRING_LITERAL:
        case ZR_AST_CHAR_LITERAL:
        case ZR_AST_NULL_LITERAL:
            compile_literal(cs, node);
            break;

        case ZR_AST_TEMPLATE_STRING_LITERAL:
            compile_template_string_literal(cs, node);
            break;
        
        case ZR_AST_IDENTIFIER_LITERAL:
            compile_identifier(cs, node);
            break;
        
        // 表达式
        case ZR_AST_UNARY_EXPRESSION:
            compile_unary_expression(cs, node);
            break;
        
        case ZR_AST_TYPE_CAST_EXPRESSION:
            compile_type_cast_expression(cs, node);
            break;
        
        case ZR_AST_BINARY_EXPRESSION:
            compile_binary_expression(cs, node);
            break;
        
        case ZR_AST_LOGICAL_EXPRESSION:
            compile_logical_expression(cs, node);
            break;
        
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            compile_assignment_expression(cs, node);
            break;
        
        case ZR_AST_CONDITIONAL_EXPRESSION:
            compile_conditional_expression(cs, node);
            break;
        
        case ZR_AST_FUNCTION_CALL:
            compile_function_call(cs, node);
            break;

        case ZR_AST_IMPORT_EXPRESSION:
            compile_import_expression(cs, node);
            break;
        
        case ZR_AST_MEMBER_EXPRESSION:
            compile_member_expression(cs, node);
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            compile_primary_expression(cs, node);
            break;

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            compile_prototype_reference_expression(cs, node);
            break;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            compile_construct_expression(cs, node);
            break;

        case ZR_AST_ARRAY_LITERAL:
            compile_array_literal(cs, node);
            break;
        
        case ZR_AST_OBJECT_LITERAL:
            compile_object_literal(cs, node);
            break;
        
        case ZR_AST_LAMBDA_EXPRESSION:
            compile_lambda_expression(cs, node);
            break;
        
        case ZR_AST_BLOCK:
            // 块作为表达式使用时，编译块并提取最后一个表达式的值
            compile_block_as_expression(cs, node);
            break;
        
        // 控制流结构和语句不应该作为表达式编译，应该先转换为语句
        case ZR_AST_IF_EXPRESSION:
        case ZR_AST_SWITCH_EXPRESSION:
        case ZR_AST_WHILE_LOOP:
        case ZR_AST_FOR_LOOP:
        case ZR_AST_FOREACH_LOOP:
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
        case ZR_AST_OUT_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            ZrParser_Compiler_Error(cs, "Loop or statement cannot be used as expression", node->location);
            break;
        
        // 解构对象和数组不是表达式，不应该在这里处理
        case ZR_AST_DESTRUCTURING_OBJECT:
        case ZR_AST_DESTRUCTURING_ARRAY:
            // 这些类型应该在变量声明中处理，不应该作为表达式编译
            ZrParser_Compiler_Error(cs, "Destructuring pattern cannot be used as expression", node->location);
            break;
        
        default:
            // 未处理的表达式类型
            if (node->type == ZR_AST_DESTRUCTURING_OBJECT || node->type == ZR_AST_DESTRUCTURING_ARRAY) {
                // 这不应该作为表达式编译，应该在变量声明中处理
                ZrParser_Compiler_Error(cs, "Destructuring pattern cannot be used as expression", node->location);
            } else {
                // 创建详细的错误消息，包含类型名称和位置信息
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
                    case ZR_AST_FUNCTION_DECLARATION: typeName = "FUNCTION_DECLARATION"; break;
                    case ZR_AST_STRUCT_DECLARATION: typeName = "STRUCT_DECLARATION"; break;
                    case ZR_AST_CLASS_DECLARATION: typeName = "CLASS_DECLARATION"; break;
                    case ZR_AST_INTERFACE_DECLARATION: typeName = "INTERFACE_DECLARATION"; break;
                    case ZR_AST_ENUM_DECLARATION: typeName = "ENUM_DECLARATION"; break;
                    case ZR_AST_ENUM_MEMBER: typeName = "ENUM_MEMBER"; break;
                    case ZR_AST_MODULE_DECLARATION: typeName = "MODULE_DECLARATION"; break;
                    case ZR_AST_IMPORT_EXPRESSION: typeName = "IMPORT_EXPRESSION"; break;
                    case ZR_AST_SCRIPT: typeName = "SCRIPT"; break;
                    default: break;
                }
                snprintf(errorMsg, sizeof(errorMsg), 
                        "Unexpected expression type: %s (type %d) at line %d:%d. "
                        "This node type should not be compiled as an expression. "
                        "Please check if it was incorrectly placed in an expression context.",
                        typeName, node->type, 
                        node->location.start.line, node->location.start.column);
                ZrParser_Compiler_Error(cs, errorMsg, node->location);
            }
            break;
    }

    cs->currentAst = oldCurrentAst;
}

