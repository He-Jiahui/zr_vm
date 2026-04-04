//
// Created by Auto on 2025/01/XX.
//

#include "compile_expression_internal.h"

static void record_template_segment_semantics(SZrCompilerState *cs, SZrAstNode *segmentNode) {
    SZrTemplateSegment segment;

    if (cs == ZR_NULL || segmentNode == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return;
    }

    memset(&segment, 0, sizeof(segment));
    if (segmentNode->type == ZR_AST_STRING_LITERAL) {
        segment.isInterpolation = ZR_FALSE;
        segment.staticText = segmentNode->data.stringLiteral.value;
        if (segment.staticText == ZR_NULL) {
            segment.staticText = ZrCore_String_Create(cs->state, "", 0);
        }
    } else if (segmentNode->type == ZR_AST_INTERPOLATED_SEGMENT) {
        segment.isInterpolation = ZR_TRUE;
        segment.expression = segmentNode->data.interpolatedSegment.expression;
    } else {
        return;
    }

    ZrParser_Semantic_AppendTemplateSegment(cs->semanticContext, &segment);
}

static TZrUInt32 compile_template_segment_into_string_slot(SZrCompilerState *cs,
                                                         SZrAstNode *segmentNode,
                                                         TZrUInt32 targetSlot) {
    SZrInferredType inferredType;
    TZrBool hasInferredType = ZR_FALSE;

    if (cs == ZR_NULL || segmentNode == ZR_NULL || cs->hasError) {
        return ZR_PARSER_SLOT_NONE;
    }

    if (segmentNode->type == ZR_AST_STRING_LITERAL) {
        SZrString *value = segmentNode->data.stringLiteral.value;
        if (value == ZR_NULL) {
            value = ZrCore_String_Create(cs->state, "", 0);
            if (value == ZR_NULL) {
                ZrParser_Compiler_Error(cs,
                                "Failed to allocate empty template string segment",
                                segmentNode->location);
                return ZR_PARSER_SLOT_NONE;
            }
        }

        if (emit_string_constant(cs, value) == ZR_PARSER_SLOT_NONE) {
            return ZR_PARSER_SLOT_NONE;
        }
        return normalize_top_result_to_slot(cs, targetSlot);
    }

    if (segmentNode->type != ZR_AST_INTERPOLATED_SEGMENT) {
        ZrParser_Compiler_Error(cs, "Unexpected template string segment", segmentNode->location);
        return ZR_PARSER_SLOT_NONE;
    }

    if (segmentNode->data.interpolatedSegment.expression == ZR_NULL) {
        SZrString *emptyString = ZrCore_String_Create(cs->state, "", 0);
        if (emptyString == ZR_NULL || emit_string_constant(cs, emptyString) == ZR_PARSER_SLOT_NONE) {
            ZrParser_Compiler_Error(cs, "Failed to allocate empty template string segment", segmentNode->location);
            return ZR_PARSER_SLOT_NONE;
        }
        return normalize_top_result_to_slot(cs, targetSlot);
    }

    if (compile_expression_into_slot(cs,
                                     segmentNode->data.interpolatedSegment.expression,
                                     targetSlot) == ZR_PARSER_SLOT_NONE) {
        return ZR_PARSER_SLOT_NONE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    hasInferredType = ZrParser_ExpressionType_Infer(cs,
                                            segmentNode->data.interpolatedSegment.expression,
                                            &inferredType);
    if (!hasInferredType || inferredType.baseType != ZR_VALUE_TYPE_STRING) {
        emit_type_conversion(cs, targetSlot, targetSlot, ZR_INSTRUCTION_ENUM(TO_STRING));
    }
    if (hasInferredType) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
    }

    collapse_stack_to_slot(cs, targetSlot);
    return targetSlot;
}

void compile_template_string_literal(SZrCompilerState *cs, SZrAstNode *node) {
    SZrAstNodeArray *segments;
    TZrUInt32 resultSlot;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected template string literal", node->location);
        return;
    }

    segments = node->data.templateStringLiteral.segments;
    if (segments == ZR_NULL || segments->count == 0) {
        SZrString *emptyString = ZrCore_String_Create(cs->state, "", 0);
        if (emptyString == ZR_NULL || emit_string_constant(cs, emptyString) == ZR_PARSER_SLOT_NONE) {
            ZrParser_Compiler_Error(cs, "Failed to allocate empty template string", node->location);
        }
        return;
    }

    for (TZrSize i = 0; i < segments->count; i++) {
        record_template_segment_semantics(cs, segments->nodes[i]);
    }

    resultSlot = allocate_stack_slot(cs);
    if (compile_template_segment_into_string_slot(cs, segments->nodes[0], resultSlot) == ZR_PARSER_SLOT_NONE) {
        return;
    }

    for (TZrSize i = 1; i < segments->count; i++) {
        TZrUInt32 nextSlot = allocate_stack_slot(cs);
        TZrInstruction addInst;

        if (compile_template_segment_into_string_slot(cs, segments->nodes[i], nextSlot) == ZR_PARSER_SLOT_NONE) {
            return;
        }

        addInst = create_instruction_2(ZR_INSTRUCTION_ENUM(ADD_STRING),
                                       (TZrUInt16)resultSlot,
                                       (TZrUInt16)resultSlot,
                                       (TZrUInt16)nextSlot);
        emit_instruction(cs, addInst);
        collapse_stack_to_slot(cs, resultSlot);
    }
}

// 编译字面量
void compile_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    SZrTypeValue constantValue;
    ZrCore_Value_ResetAsNull(&constantValue);
    TZrUInt32 constantIndex = 0;
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    
    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL: {
            TZrBool value = node->data.booleanLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, &constantValue, value ? 1 : 0);
            constantValue.type = ZR_VALUE_TYPE_BOOL;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_INTEGER_LITERAL: {
            TZrInt64 value = node->data.integerLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_FLOAT_LITERAL: {
            TZrDouble value = node->data.floatLiteral.value;
            ZrCore_Value_InitAsFloat(cs->state, &constantValue, value);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_STRING_LITERAL: {
            SZrString *value = node->data.stringLiteral.value;
            if (value != ZR_NULL) {
                ZrCore_Value_InitAsRawObject(cs->state, &constantValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
                constantValue.type = ZR_VALUE_TYPE_STRING;
                constantIndex = add_constant(cs, &constantValue);
                TZrInstruction inst = create_instruction_1(
                        ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
                emit_instruction(cs, inst);
            }
            break;
        }
        
        case ZR_AST_CHAR_LITERAL: {
            TZrChar value = node->data.charLiteral.value;
            ZrCore_Value_InitAsInt(cs->state, &constantValue, (TZrInt64)value);
            constantValue.type = ZR_VALUE_TYPE_INT8;
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        case ZR_AST_NULL_LITERAL: {
            ZrCore_Value_ResetAsNull(&constantValue);
            constantIndex = add_constant(cs, &constantValue);
            TZrInstruction inst = create_instruction_1(
                    ZR_INSTRUCTION_ENUM(GET_CONSTANT), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)constantIndex);
            emit_instruction(cs, inst);
            break;
        }
        
        default:
            ZrParser_Compiler_Error(cs, "Unexpected literal type", node->location);
            break;
    }
}

// 编译标识符
void compile_identifier(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_IDENTIFIER_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected identifier", node->location);
        return;
    }
    
    SZrString *name = node->data.identifier.name;
    if (name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Identifier name is null", node->location);
        return;
    }
    
    ZR_UNUSED_PARAMETER(ZrCore_String_GetNativeString(name));
    
    // 重要：先查找局部变量（包括闭包变量）
    // 如果存在同名的局部变量，它会覆盖全局的 zr 对象
    // 这是作用域规则：局部变量优先于全局对象
    TZrUInt32 localVarIndex = find_local_var(cs, name);
    if (localVarIndex != ZR_PARSER_SLOT_NONE) {
        // 找到局部变量：使用 GET_STACK
        // 即使这个变量名是 "zr"，也使用局部变量而不是全局 zr 对象
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction inst = create_instruction_1(
                ZR_INSTRUCTION_ENUM(GET_STACK), ZR_COMPILE_SLOT_U16(destSlot), (TZrInt32)localVarIndex);
        emit_instruction(cs, inst);
        return;
    }
    
    // 查找闭包变量（在局部变量之后，但在全局对象之前）
    TZrUInt32 closureVarIndex = find_closure_var(cs, name);
    if (closureVarIndex != ZR_PARSER_INDEX_NONE) {
        // 闭包变量：根据 inStack 标志选择使用 GET_CLOSURE 还是 GETUPVAL
        // 即使这个变量名是 "zr"，也使用闭包变量而不是全局 zr 对象
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        
        // 获取闭包变量信息以检查 inStack 标志
        SZrFunctionClosureVariable *closureVar = (SZrFunctionClosureVariable *)ZrCore_Array_Get(&cs->closureVars, closureVarIndex);
        if (closureVar != ZR_NULL && closureVar->inStack) {
            // 变量在栈上，使用 GETUPVAL
            // GETUPVAL 格式: operandExtra = destSlot, operand1[0] = closureVarIndex, operand1[1] = 0
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(GETUPVAL), (TZrUInt16)destSlot, (TZrUInt16)closureVarIndex, 0);
            emit_instruction(cs, inst);
        } else {
            // 变量不在栈上，使用 GET_CLOSURE
            // GET_CLOSURE 格式: operandExtra = destSlot, operand1[0] = closureVarIndex, operand1[1] = 0
            TZrInstruction inst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_CLOSURE), (TZrUInt16)destSlot, (TZrUInt16)closureVarIndex, 0);
            emit_instruction(cs, inst);
        }
        return;
    }

    {
        SZrTypeValue compileTimeValue;
        if (ZrParser_Compiler_TryGetCompileTimeValue(cs, name, &compileTimeValue)) {
            TZrUInt32 destSlot = allocate_stack_slot(cs);
            emit_constant_to_slot_local(cs, destSlot, &compileTimeValue, node->location);
            return;
        }
    }
    
    // 只有在没有找到局部变量和闭包变量的情况下，才检查是否是全局关键字 "zr"
    // 这样可以确保局部变量能够覆盖全局的 zr 对象
    TZrNativeString varNameStr;
    TZrSize nameLen;
    if (name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        varNameStr = ZrCore_String_GetNativeStringShort(name);
        nameLen = name->shortStringLength;
    } else {
        varNameStr = ZrCore_String_GetNativeString(name);
        nameLen = name->longStringLength;
    }
    
    // 检查是否是 "zr" 全局对象
    // 注意：只有在没有局部变量覆盖的情况下才会到达这里
    if (nameLen == 2 && varNameStr[0] == 'z' && varNameStr[1] == 'r') {
        // 使用 GET_GLOBAL 指令获取全局 zr 对象
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        // GET_GLOBAL 格式: operandExtra = destSlot, operand1[0] = 0, operand1[1] = 0
        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_GLOBAL), (TZrUInt16)destSlot, 0);
        emit_instruction(cs, inst);
        return;
    }
    
    // 尝试作为父函数的子函数访问（使用 GET_SUB_FUNCTION）
    // 在编译时查找子函数索引，而不是使用名称常量
    // GET_SUB_FUNCTION 通过索引直接访问 childFunctionList，这是编译时确定的静态索引
    TZrUInt32 childFunctionIndex = find_child_function_index(cs, name);
    if (childFunctionIndex != ZR_PARSER_INDEX_NONE) {
        // 找到子函数索引，生成 GET_SUB_FUNCTION 指令
        // GET_SUB_FUNCTION 格式: operandExtra = destSlot, operand1[0] = childFunctionIndex, operand1[1] = 0
        TZrUInt32 destSlot = allocate_stack_slot(cs);
        TZrInstruction getSubFuncInst = create_instruction_2(ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION), (TZrUInt16)destSlot, (TZrUInt16)childFunctionIndex, 0);
        emit_instruction(cs, getSubFuncInst);
        return;
    }
    
    // 如果不是子函数，尝试作为全局对象（zr）的成员访问（使用 GET_GLOBAL + GET_MEMBER）
    // 1. 使用 GET_GLOBAL 获取全局 zr 对象
    TZrUInt32 globalObjSlot = allocate_stack_slot(cs);
    TZrInstruction getGlobalInst =
            create_instruction_0(ZR_INSTRUCTION_ENUM(GET_GLOBAL), ZR_COMPILE_SLOT_U16(globalObjSlot));
    emit_instruction(cs, getGlobalInst);

    {
        TZrUInt32 memberId = compiler_get_or_add_member_entry(cs, name);
        if (memberId == ZR_PARSER_MEMBER_ID_NONE) {
            ZrParser_Compiler_Error(cs, "Failed to register global member symbol", cs->currentAst->location);
            return;
        }

        emit_instruction(cs,
                         create_instruction_2(ZR_INSTRUCTION_ENUM(GET_MEMBER),
                                              (TZrUInt16)globalObjSlot,
                                              (TZrUInt16)globalObjSlot,
                                              (TZrUInt16)memberId));
    }
}

// 编译一元表达式

void compile_array_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_ARRAY_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected array literal", node->location);
        return;
    }
    
    SZrArrayLiteral *arrayLiteral = &node->data.arrayLiteral;
    
    // 1. 创建空数组对象
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createArrayInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_ARRAY), (TZrUInt16)destSlot);
    emit_instruction(cs, createArrayInst);
    
    // 2. 编译每个元素并设置到数组中
    if (arrayLiteral->elements != ZR_NULL) {
        for (TZrSize i = 0; i < arrayLiteral->elements->count; i++) {
            SZrAstNode *elemNode = arrayLiteral->elements->nodes[i];
            if (elemNode == ZR_NULL) {
                continue;
            }
            
            // 编译元素值
            ZrParser_Expression_Compile(cs, elemNode);
            TZrUInt32 valueSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
            
            // 创建索引常量
            SZrTypeValue indexValue;
            ZrCore_Value_InitAsInt(cs->state, &indexValue, (TZrInt64)i);
            TZrUInt32 indexConstantIndex = add_constant(cs, &indexValue);
            
            // 将索引压栈
            TZrUInt32 indexSlot = allocate_stack_slot(cs);
            TZrInstruction getIndexInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)indexSlot, (TZrInt32)indexConstantIndex);
            emit_instruction(cs, getIndexInst);
            
            TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_BY_INDEX),
                                                               (TZrUInt16)valueSlot,
                                                               (TZrUInt16)destSlot,
                                                               (TZrUInt16)indexSlot);
            emit_instruction(cs, setTableInst);
            
            // 丢弃元素表达式留下的所有临时值，只保留数组对象本身。
            collapse_stack_to_slot(cs, destSlot);
        }
    }
    
    // 3. 数组对象已经在 destSlot，结果留在 destSlot
    collapse_stack_to_slot(cs, destSlot);
}

// 编译对象字面量
void compile_object_literal(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_OBJECT_LITERAL) {
        ZrParser_Compiler_Error(cs, "Expected object literal", node->location);
        return;
    }
    
    SZrObjectLiteral *objectLiteral = &node->data.objectLiteral;
    
    // 1. 创建空对象
    TZrUInt32 destSlot = allocate_stack_slot(cs);
    TZrInstruction createObjectInst = create_instruction_0(ZR_INSTRUCTION_ENUM(CREATE_OBJECT), (TZrUInt16)destSlot);
    emit_instruction(cs, createObjectInst);
    
    // 2. 编译每个键值对并设置到对象中
    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *kvNode = objectLiteral->properties->nodes[i];
            if (kvNode == ZR_NULL || kvNode->type != ZR_AST_KEY_VALUE_PAIR) {
                continue;
            }
            
            SZrKeyValuePair *kv = &kvNode->data.keyValuePair;
            
            // 编译键
            if (kv->key != ZR_NULL) {
                // 键可能是标识符、字符串字面量或表达式（计算键）
                if (kv->key->type == ZR_AST_IDENTIFIER_LITERAL) {
                    // 标识符键：转换为字符串常量
                    SZrString *keyName = kv->key->data.identifier.name;
                    if (keyName != ZR_NULL) {
                        SZrTypeValue keyValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(keyName));
                        keyValue.type = ZR_VALUE_TYPE_STRING;
                        TZrUInt32 keyConstantIndex = add_constant(cs, &keyValue);
                        
                        TZrUInt32 keySlot = allocate_stack_slot(cs);
                        TZrInstruction getKeyInst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16)keySlot, (TZrInt32)keyConstantIndex);
                        emit_instruction(cs, getKeyInst);
                        
                        // 编译值
                        ZrParser_Expression_Compile(cs, kv->value);
                        TZrUInt32 valueSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
                        
                        TZrUInt32 memberId = compiler_get_or_add_member_entry(cs, keyName);
                        TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_MEMBER),
                                                                           (TZrUInt16)valueSlot,
                                                                           (TZrUInt16)destSlot,
                                                                           (TZrUInt16)memberId);
                        emit_instruction(cs, setTableInst);
                        
                        // 丢弃属性表达式留下的所有临时值，只保留对象本身。
                        collapse_stack_to_slot(cs, destSlot);
                    }
                } else {
                    // 键是表达式（字符串字面量或计算键）
                    ZrParser_Expression_Compile(cs, kv->key);
                    TZrUInt32 keySlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
                    
                    // 编译值
                    ZrParser_Expression_Compile(cs, kv->value);
                    TZrUInt32 valueSlot = ZR_COMPILE_SLOT_U32(cs->stackSlotCount - 1);
                    
                    TZrInstruction setTableInst = create_instruction_2(ZR_INSTRUCTION_ENUM(SET_BY_INDEX),
                                                                       (TZrUInt16)valueSlot,
                                                                       (TZrUInt16)destSlot,
                                                                       (TZrUInt16)keySlot);
                    emit_instruction(cs, setTableInst);
                    
                    // 丢弃属性表达式留下的所有临时值，只保留对象本身。
                    collapse_stack_to_slot(cs, destSlot);
                }
            }
        }
    }
    
    // 3. 对象已经在 destSlot，结果留在 destSlot
    collapse_stack_to_slot(cs, destSlot);
}

// 编译 Lambda 表达式

