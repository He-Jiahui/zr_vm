//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

static void compiler_struct_append_parameter_type(SZrCompilerState *cs,
                                                  SZrArray *parameterTypes,
                                                  SZrType *typeInfo) {
    SZrInferredType paramType;

    if (cs == ZR_NULL || parameterTypes == ZR_NULL) {
        return;
    }

    if (typeInfo != ZR_NULL && ZrParser_AstTypeToInferredType_Convert(cs, typeInfo, &paramType)) {
        ZrCore_Array_Push(cs->state, parameterTypes, &paramType);
        return;
    }

    ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Push(cs->state, parameterTypes, &paramType);
}

static void compiler_struct_collect_parameter_types(SZrCompilerState *cs,
                                                    SZrArray *parameterTypes,
                                                    SZrAstNodeArray *params) {
    if (cs == ZR_NULL || parameterTypes == ZR_NULL || params == ZR_NULL || params->count == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state, parameterTypes, sizeof(SZrInferredType), params->count);
    for (TZrSize paramIndex = 0; paramIndex < params->count; paramIndex++) {
        SZrAstNode *paramNode = params->nodes[paramIndex];
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        compiler_struct_append_parameter_type(cs, parameterTypes, paramNode->data.parameter.typeInfo);
    }
}

void compiler_collect_parameter_passing_modes(SZrState *state,
                                              SZrArray *parameterPassingModes,
                                              SZrAstNodeArray *params) {
    if (state == ZR_NULL || parameterPassingModes == ZR_NULL || params == ZR_NULL || params->count == 0) {
        return;
    }

    ZrCore_Array_Init(state, parameterPassingModes, sizeof(EZrParameterPassingMode), params->count);
    for (TZrSize paramIndex = 0; paramIndex < params->count; paramIndex++) {
        SZrAstNode *paramNode = params->nodes[paramIndex];
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        passingMode = paramNode->data.parameter.passingMode;
        ZrCore_Array_Push(state, parameterPassingModes, &passingMode);
    }
}

SZrString *get_type_name_from_inferred_type(SZrCompilerState *cs, const SZrInferredType *inferredType) {
    if (cs == ZR_NULL || inferredType == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 如果有用户定义类型名，直接返回
    if (inferredType->typeName != ZR_NULL) {
        return inferredType->typeName;
    }
    
    // 根据基础类型返回对应的类型名称字符串
    const char *typeNameStr = ZR_NULL;
    switch (inferredType->baseType) {
        case ZR_VALUE_TYPE_INT8: typeNameStr = "i8"; break;
        case ZR_VALUE_TYPE_INT16: typeNameStr = "i16"; break;
        case ZR_VALUE_TYPE_INT32: typeNameStr = "i32"; break;
        case ZR_VALUE_TYPE_INT64: typeNameStr = "int"; break;
        case ZR_VALUE_TYPE_UINT8: typeNameStr = "u8"; break;
        case ZR_VALUE_TYPE_UINT16: typeNameStr = "u16"; break;
        case ZR_VALUE_TYPE_UINT32: typeNameStr = "u32"; break;
        case ZR_VALUE_TYPE_UINT64: typeNameStr = "uint"; break;
        case ZR_VALUE_TYPE_FLOAT: typeNameStr = "float"; break;
        case ZR_VALUE_TYPE_DOUBLE: typeNameStr = "double"; break;
        case ZR_VALUE_TYPE_BOOL: typeNameStr = "bool"; break;
        case ZR_VALUE_TYPE_STRING: typeNameStr = "string"; break;
        case ZR_VALUE_TYPE_OBJECT: typeNameStr = "object"; break;
        default: typeNameStr = "object"; break;
    }
    
    if (typeNameStr != ZR_NULL) {
        return ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)typeNameStr);
    }
    
    return ZR_NULL;
}

static TZrBool compiler_append_text_fragment(char *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *offset,
                                             const TZrChar *fragment) {
    TZrSize fragmentLength;

    if (buffer == ZR_NULL || offset == ZR_NULL || fragment == ZR_NULL) {
        return ZR_FALSE;
    }

    fragmentLength = strlen(fragment);
    if (*offset + fragmentLength + 1 > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *offset, fragment, fragmentLength);
    *offset += fragmentLength;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

static TZrBool compiler_append_ast_node_display_text(SZrCompilerState *cs,
                                                     const SZrAstNode *node,
                                                     char *buffer,
                                                     TZrSize bufferSize,
                                                     TZrSize *offset) {
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_TYPE: {
            SZrString *typeName = extract_type_name_string(cs, (SZrType *)&node->data.type);
            return typeName != ZR_NULL &&
                   compiler_append_text_fragment(buffer, bufferSize, offset, ZrCore_String_GetNativeString(typeName));
        }
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name != ZR_NULL &&
                   compiler_append_text_fragment(buffer,
                                                bufferSize,
                                                offset,
                                                ZrCore_String_GetNativeString(node->data.identifier.name));
        case ZR_AST_INTEGER_LITERAL:
            if (node->data.integerLiteral.literal != ZR_NULL) {
                return compiler_append_text_fragment(buffer,
                                                     bufferSize,
                                                     offset,
                                                     ZrCore_String_GetNativeString(node->data.integerLiteral.literal));
            }
            {
                char integerBuffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];
                snprintf(integerBuffer, sizeof(integerBuffer), "%lld", (long long)node->data.integerLiteral.value);
                return compiler_append_text_fragment(buffer, bufferSize, offset, integerBuffer);
            }
        case ZR_AST_FLOAT_LITERAL:
            if (node->data.floatLiteral.literal != ZR_NULL) {
                return compiler_append_text_fragment(buffer,
                                                     bufferSize,
                                                     offset,
                                                     ZrCore_String_GetNativeString(node->data.floatLiteral.literal));
            }
            return ZR_FALSE;
        case ZR_AST_UNARY_EXPRESSION:
            return node->data.unaryExpression.op.op != ZR_NULL &&
                   compiler_append_text_fragment(buffer, bufferSize, offset, node->data.unaryExpression.op.op) &&
                   compiler_append_ast_node_display_text(cs,
                                                        node->data.unaryExpression.argument,
                                                        buffer,
                                                        bufferSize,
                                                        offset);
        case ZR_AST_BINARY_EXPRESSION:
            return compiler_append_ast_node_display_text(cs,
                                                        node->data.binaryExpression.left,
                                                        buffer,
                                                        bufferSize,
                                                        offset) &&
                   compiler_append_text_fragment(buffer, bufferSize, offset, " ") &&
                   node->data.binaryExpression.op.op != ZR_NULL &&
                   compiler_append_text_fragment(buffer, bufferSize, offset, node->data.binaryExpression.op.op) &&
                   compiler_append_text_fragment(buffer, bufferSize, offset, " ") &&
                   compiler_append_ast_node_display_text(cs,
                                                        node->data.binaryExpression.right,
                                                        buffer,
                                                        bufferSize,
                                                        offset);
        default:
            return ZR_FALSE;
    }
}

SZrString *extract_generic_argument_name_string(SZrCompilerState *cs, SZrAstNode *node) {
    char buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type != ZR_AST_TYPE) {
        SZrTypeValue evaluatedValue;
        if (ZrParser_Compiler_EvaluateCompileTimeExpression(cs, node, &evaluatedValue)) {
            char integerBuffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];

            switch (evaluatedValue.type) {
                case ZR_VALUE_TYPE_INT8:
                case ZR_VALUE_TYPE_INT16:
                case ZR_VALUE_TYPE_INT32:
                case ZR_VALUE_TYPE_INT64:
                    snprintf(integerBuffer,
                             sizeof(integerBuffer),
                             "%lld",
                             (long long)evaluatedValue.value.nativeObject.nativeInt64);
                    return ZrCore_String_CreateFromNative(cs->state, integerBuffer);
                case ZR_VALUE_TYPE_UINT8:
                case ZR_VALUE_TYPE_UINT16:
                case ZR_VALUE_TYPE_UINT32:
                case ZR_VALUE_TYPE_UINT64:
                    snprintf(integerBuffer,
                             sizeof(integerBuffer),
                             "%llu",
                             (unsigned long long)evaluatedValue.value.nativeObject.nativeUInt64);
                    return ZrCore_String_CreateFromNative(cs->state, integerBuffer);
                default:
                    break;
            }
        }
    }

    if (!compiler_append_ast_node_display_text(cs, node, buffer, sizeof(buffer), &offset)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, offset);
}

static SZrString *compiler_extract_type_name_node_string(SZrCompilerState *cs, SZrAstNode *typeNameNode) {
    if (cs == ZR_NULL || typeNameNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeNameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeNameNode->data.identifier.name;
    }

    if (typeNameNode->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNameNode->data.genericType;
        char buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
        TZrSize offset = 0;

        if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL) {
            return ZR_NULL;
        }

        buffer[0] = '\0';
        if (!compiler_append_text_fragment(buffer,
                                           sizeof(buffer),
                                           &offset,
                                           ZrCore_String_GetNativeString(genericType->name->name)) ||
            !compiler_append_text_fragment(buffer, sizeof(buffer), &offset, "<")) {
            return ZR_NULL;
        }

        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrAstNode *paramNode = genericType->params->nodes[index];
                SZrString *paramTypeName;

                if (index > 0 && !compiler_append_text_fragment(buffer, sizeof(buffer), &offset, ", ")) {
                    return ZR_NULL;
                }

                paramTypeName = extract_generic_argument_name_string(cs, paramNode);
                if (paramTypeName == ZR_NULL ||
                    !compiler_append_text_fragment(buffer,
                                                   sizeof(buffer),
                                                   &offset,
                                                   ZrCore_String_GetNativeString(paramTypeName))) {
                    return ZR_NULL;
                }
            }
        }

        if (!compiler_append_text_fragment(buffer, sizeof(buffer), &offset, ">")) {
            return ZR_NULL;
        }

        return ZrCore_String_Create(cs->state, buffer, offset);
    }

    if (typeNameNode->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = &typeNameNode->data.tupleType;
        char buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
        TZrSize offset = 0;

        buffer[0] = '\0';
        if (!compiler_append_text_fragment(buffer, sizeof(buffer), &offset, "(")) {
            return ZR_NULL;
        }

        if (tupleType->elements != ZR_NULL) {
            for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                SZrAstNode *elemNode = tupleType->elements->nodes[index];
                SZrString *elemTypeName;

                if (index > 0 && !compiler_append_text_fragment(buffer, sizeof(buffer), &offset, ", ")) {
                    return ZR_NULL;
                }

                elemTypeName = extract_generic_argument_name_string(cs, elemNode);
                if (elemTypeName == ZR_NULL ||
                    !compiler_append_text_fragment(buffer,
                                                   sizeof(buffer),
                                                   &offset,
                                                   ZrCore_String_GetNativeString(elemTypeName))) {
                    return ZR_NULL;
                }
            }
        }

        if (!compiler_append_text_fragment(buffer, sizeof(buffer), &offset, ")")) {
            return ZR_NULL;
        }

        return ZrCore_String_Create(cs->state, buffer, offset);
    }

    return ZR_NULL;
}

// 辅助函数：从类型节点提取类型名称字符串
SZrString *extract_type_name_string(SZrCompilerState *cs, SZrType *type) {
    SZrString *baseName;
    char buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL) {
        return ZR_NULL;
    }

    baseName = compiler_extract_type_name_node_string(cs, type->name);
    if (baseName == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!compiler_append_text_fragment(buffer, sizeof(buffer), &offset, ZrCore_String_GetNativeString(baseName))) {
        return ZR_NULL;
    }

    if (type->subType != ZR_NULL) {
        SZrString *subTypeName = extract_type_name_string(cs, type->subType);
        if (subTypeName == ZR_NULL ||
            !compiler_append_text_fragment(buffer, sizeof(buffer), &offset, ".") ||
            !compiler_append_text_fragment(buffer,
                                           sizeof(buffer),
                                           &offset,
                                           ZrCore_String_GetNativeString(subTypeName))) {
            return ZR_NULL;
        }
    }

    for (TZrInt32 index = 0; index < type->dimensions; index++) {
        if (!compiler_append_text_fragment(buffer, sizeof(buffer), &offset, "[]")) {
            return ZR_NULL;
        }
    }

    return ZrCore_String_Create(cs->state, buffer, offset);
}

void compiler_collect_generic_parameter_info(SZrCompilerState *cs,
                                             SZrArray *genericParameters,
                                             SZrGenericDeclaration *genericDeclaration) {
    if (cs == ZR_NULL || genericParameters == ZR_NULL || genericDeclaration == ZR_NULL ||
        genericDeclaration->params == ZR_NULL) {
        return;
    }

    if (!genericParameters->isValid || genericParameters->head == ZR_NULL ||
        genericParameters->capacity == 0 || genericParameters->elementSize == 0) {
        ZrCore_Array_Init(cs->state,
                          genericParameters,
                          sizeof(SZrTypeGenericParameterInfo),
                          genericDeclaration->params->count > 0 ? genericDeclaration->params->count : 1);
    }

    for (TZrSize index = 0; index < genericDeclaration->params->count; index++) {
        SZrAstNode *paramNode = genericDeclaration->params->nodes[index];
        SZrTypeGenericParameterInfo genericInfo;
        SZrParameter *parameter;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        memset(&genericInfo, 0, sizeof(genericInfo));
        genericInfo.name = parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL;
        genericInfo.genericKind = parameter->genericKind;
        genericInfo.variance = parameter->variance;
        genericInfo.requiresClass = parameter->genericRequiresClass;
        genericInfo.requiresStruct = parameter->genericRequiresStruct;
        genericInfo.requiresNew = parameter->genericRequiresNew;
        ZrCore_Array_Init(cs->state,
                          &genericInfo.constraintTypeNames,
                          sizeof(SZrString *),
                          parameter->genericTypeConstraints != ZR_NULL ? parameter->genericTypeConstraints->count : 1);

        if (parameter->genericTypeConstraints != ZR_NULL) {
            for (TZrSize constraintIndex = 0; constraintIndex < parameter->genericTypeConstraints->count;
                 constraintIndex++) {
                SZrAstNode *constraintNode = parameter->genericTypeConstraints->nodes[constraintIndex];
                SZrString *constraintName;

                if (constraintNode == ZR_NULL || constraintNode->type != ZR_AST_TYPE) {
                    continue;
                }

                constraintName = extract_type_name_string(cs, &constraintNode->data.type);
                if (constraintName != ZR_NULL) {
                    ZrCore_Array_Push(cs->state, &genericInfo.constraintTypeNames, &constraintName);
                }
            }
        }

        ZrCore_Array_Push(cs->state, genericParameters, &genericInfo);
    }
}

// 辅助函数：计算类型的大小（字节数）
// 返回0表示未知类型，需要在运行时确定
TZrUInt32 calculate_type_size(SZrCompilerState *cs, SZrType *type) {
    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL) {
        return 0;
    }
    
    // 处理数组类型
    if (type->dimensions > 0) {
        // 数组本身是指针类型，固定8字节
        // 注意：数组元素的实际大小在运行时确定，这里只返回指针大小
        return sizeof(TZrPtr); // 8 bytes
    }
    
    // 处理基本类型（通过类型名称字符串匹配）
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = type->name->data.identifier.name;
        if (typeName == ZR_NULL) {
            return 0;
        }
        
        TZrNativeString typeNameStr = ZrCore_String_GetNativeStringShort(typeName);
        if (typeNameStr == ZR_NULL) {
            return 0;
        }
        
        // 匹配基本类型名称
        if (strcmp(typeNameStr, "int") == 0 || strcmp(typeNameStr, "i64") == 0) {
            return sizeof(TZrInt64); // 8 bytes
        }
        if (strcmp(typeNameStr, "i8") == 0) {
            return sizeof(TZrInt8); // 1 byte
        }
        if (strcmp(typeNameStr, "i16") == 0) {
            return sizeof(TZrInt16); // 2 bytes
        }
        if (strcmp(typeNameStr, "i32") == 0) {
            return sizeof(TZrInt32); // 4 bytes
        }
        if (strcmp(typeNameStr, "uint") == 0 || strcmp(typeNameStr, "u64") == 0) {
            return sizeof(TZrUInt64); // 8 bytes
        }
        if (strcmp(typeNameStr, "u8") == 0) {
            return sizeof(TZrUInt8); // 1 byte
        }
        if (strcmp(typeNameStr, "u16") == 0) {
            return sizeof(TZrUInt16); // 2 bytes
        }
        if (strcmp(typeNameStr, "u32") == 0) {
            return sizeof(TZrUInt32); // 4 bytes
        }
        if (strcmp(typeNameStr, "float") == 0 || strcmp(typeNameStr, "f32") == 0) {
            return sizeof(TZrFloat32); // 4 bytes
        }
        if (strcmp(typeNameStr, "double") == 0 || strcmp(typeNameStr, "f64") == 0 || strcmp(typeNameStr, "f") == 0) {
            return sizeof(TZrDouble); // 8 bytes
        }
        if (strcmp(typeNameStr, "bool") == 0) {
            return sizeof(TZrBool); // 1 byte
        }
        if (strcmp(typeNameStr, "string") == 0 || strcmp(typeNameStr, "str") == 0) {
            return sizeof(TZrPtr); // 指针大小 8 bytes
        }
        if (strcmp(typeNameStr, "object") == 0 || strcmp(typeNameStr, "obj") == 0) {
            return sizeof(TZrPtr); // 指针大小 8 bytes
        }
        
        // 如果是自定义类型（struct/class），大小未知，需要在运行时确定
        // 返回0表示需要运行时计算
        return 0;
    }
    
    return 0;
}

// 辅助函数：应用对齐规则计算偏移量
TZrUInt32 align_offset(TZrUInt32 offset, TZrUInt32 align) {
    // 对齐到align字节边界
    return ((offset + align - 1) / align) * align;
}

// 辅助函数：确定类型的对齐要求（字节）
TZrUInt32 get_type_alignment(SZrCompilerState *cs, SZrType *type) {
    if (cs == ZR_NULL || type == ZR_NULL) {
        return ZR_ALIGN_SIZE; // 默认对齐
    }
    
    TZrUInt32 size = calculate_type_size(cs, type);
    if (size == 0) {
        return ZR_ALIGN_SIZE; // 未知类型，默认对齐
    }
    
    // 对齐要求通常是类型大小的幂（但不超过默认对齐）
    if (size <= 1) return 1;
    if (size <= 2) return 2;
    if (size <= 4) return 4;
    return ZR_ALIGN_SIZE; // 默认对齐
}

// 编译 struct 声明
void compile_struct_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_STRUCT_DECLARATION) {
        ZrParser_Statement_Compile(cs, node);
        return;
    }
    
    SZrStructDeclaration *structDecl = &node->data.structDeclaration;
    
    // 获取类型名称
    if (structDecl->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Struct declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = structDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Struct name is null", node->location);
        return;
    }
    
    // 设置当前类型名称（用于成员字段 const 检查）
    SZrString *oldTypeName = cs->currentTypeName;
    SZrTypePrototypeInfo *oldTypePrototypeInfo = cs->currentTypePrototypeInfo;
    SZrAstNode *oldTypeNode = cs->currentTypeNode;
    cs->currentTypeName = typeName;
    cs->currentTypeNode = node;
    
    // 创建 prototype 信息结构
    SZrTypePrototypeInfo info;
    memset(&info, 0, sizeof(info));
    info.name = typeName;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    info.accessModifier = structDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    
    // 初始化继承数组
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(cs->state,
                      &info.genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      structDecl->generic != ZR_NULL && structDecl->generic->params != ZR_NULL
                              ? structDecl->generic->params->count
                              : 1);
    compiler_collect_generic_parameter_info(cs, &info.genericParameters, structDecl->generic);
    
    // 处理继承关系
    if (structDecl->inherits != ZR_NULL && structDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < structDecl->inherits->count; i++) {
            SZrAstNode *inheritType = structDecl->inherits->nodes[i];
            if (inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE) {
                SZrString *inheritTypeName = extract_type_name_string(cs, &inheritType->data.type);
                if (inheritTypeName != ZR_NULL) {
                    ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
                    if (info.extendsTypeName == ZR_NULL) {
                        info.extendsTypeName = inheritTypeName;
                    }
                }
            }
        }
    }
    
    // 初始化成员数组
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_MEDIUM);
    cs->currentTypePrototypeInfo = &info;
    
    // 处理成员信息
    if (structDecl->members != ZR_NULL && structDecl->members->count > 0) {
        for (TZrSize i = 0; i < structDecl->members->count; i++) {
            SZrAstNode *member = structDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            SZrTypeMemberInfo memberInfo;
            memset(&memberInfo, 0, sizeof(memberInfo));

            // 初始化所有字段
            memberInfo.memberType = member->type;
            memberInfo.isStatic = ZR_FALSE;
            memberInfo.isConst = ZR_FALSE;
            memberInfo.isUsingManaged = ZR_FALSE;
            memberInfo.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
            memberInfo.callsClose = ZR_FALSE;
            memberInfo.callsDestructor = ZR_FALSE;
            memberInfo.declarationOrder = (TZrUInt32)i;
            memberInfo.accessModifier = ZR_ACCESS_PRIVATE;
            memberInfo.name = ZR_NULL;
            memberInfo.fieldType = ZR_NULL;
            memberInfo.fieldTypeName = ZR_NULL;
            memberInfo.fieldOffset = 0;
            memberInfo.fieldSize = 0;
            memberInfo.compiledFunction = ZR_NULL;
            memberInfo.functionConstantIndex = 0;
            memberInfo.parameterCount = 0;
            ZrCore_Array_Construct(&memberInfo.parameterTypes);
            ZrCore_Array_Construct(&memberInfo.genericParameters);
            ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
            memberInfo.declarationNode = member;
            memberInfo.metaType = ZR_META_ENUM_MAX;
            memberInfo.isMetaMethod = ZR_FALSE;
            memberInfo.returnTypeName = ZR_NULL;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_STRUCT_FIELD: {
                    SZrStructField *field = &member->data.structField;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isStatic = field->isStatic;
                    memberInfo.isConst = field->isConst;
                    memberInfo.isUsingManaged = field->isUsingManaged;
                    if (field->name != ZR_NULL) {
                        memberInfo.name = field->name->name;
                    }

                    if (field->isStatic && field->isUsingManaged) {
                        ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "static using fields are not supported",
                                           member->location);
                        cs->currentTypeName = oldTypeName;
                        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                        cs->currentTypeNode = oldTypeNode;
                        return;
                    }
                    
                    // 处理字段类型信息
                    if (field->typeInfo != ZR_NULL) {
                        memberInfo.fieldType = field->typeInfo;
                        memberInfo.fieldTypeName = extract_type_name_string(cs, field->typeInfo);
                        memberInfo.ownershipQualifier = field->typeInfo->ownershipQualifier;
                        // 计算字段大小（用于偏移量计算）
                        memberInfo.fieldSize = calculate_type_size(cs, field->typeInfo);
                    } else if (field->init != ZR_NULL) {
                        // 没有类型注解，从初始值推断类型
                        SZrInferredType inferredType;
                        if (ZrParser_ExpressionType_Infer(cs, field->init, &inferredType)) {
                            memberInfo.fieldTypeName = get_type_name_from_inferred_type(cs, &inferredType);
                            memberInfo.ownershipQualifier = inferredType.ownershipQualifier;
                            // 根据推断类型计算字段大小
                            switch (inferredType.baseType) {
                                case ZR_VALUE_TYPE_INT8: memberInfo.fieldSize = sizeof(TZrInt8); break;
                                case ZR_VALUE_TYPE_INT16: memberInfo.fieldSize = sizeof(TZrInt16); break;
                                case ZR_VALUE_TYPE_INT32: memberInfo.fieldSize = sizeof(TZrInt32); break;
                                case ZR_VALUE_TYPE_INT64: memberInfo.fieldSize = sizeof(TZrInt64); break;
                                case ZR_VALUE_TYPE_UINT8: memberInfo.fieldSize = sizeof(TZrUInt8); break;
                                case ZR_VALUE_TYPE_UINT16: memberInfo.fieldSize = sizeof(TZrUInt16); break;
                                case ZR_VALUE_TYPE_UINT32: memberInfo.fieldSize = sizeof(TZrUInt32); break;
                                case ZR_VALUE_TYPE_UINT64: memberInfo.fieldSize = sizeof(TZrUInt64); break;
                                case ZR_VALUE_TYPE_FLOAT: memberInfo.fieldSize = sizeof(TZrFloat32); break;
                                case ZR_VALUE_TYPE_DOUBLE: memberInfo.fieldSize = sizeof(TZrDouble); break;
                                case ZR_VALUE_TYPE_BOOL: memberInfo.fieldSize = sizeof(TZrBool); break;
                                case ZR_VALUE_TYPE_STRING:
                                case ZR_VALUE_TYPE_OBJECT:
                                default:
                                    memberInfo.fieldSize = sizeof(TZrPtr); // 指针大小
                                    break;
                            }
                            ZrParser_InferredType_Free(cs->state, &inferredType);
                        } else {
                            // 类型推断失败，默认为object类型
                            memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                            memberInfo.fieldSize = sizeof(TZrPtr);
                        }
                    } else {
                        // 没有类型注解和初始值，默认为object类型（8字节指针）
                        memberInfo.fieldTypeName = ZrCore_String_CreateFromNative(cs->state, "object");
                        memberInfo.fieldSize = sizeof(TZrPtr);
                    }

                    if (memberInfo.isUsingManaged &&
                        memberInfo.ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_WEAK) {
                        memberInfo.callsClose = ZR_TRUE;
                        memberInfo.callsDestructor = ZR_TRUE;
                    }
                    
                    // 字段偏移量将在所有字段收集后统一计算
                    // 这里先设置为0，后续会根据字段顺序和对齐规则计算
                    break;
                }
                case ZR_AST_STRUCT_METHOD: {
                    SZrStructMethod *method = &member->data.structMethod;
                    TZrUInt32 compiledParameterCount = 0;
                    memberInfo.accessModifier = method->access;
                    memberInfo.isStatic = method->isStatic;
                    memberInfo.receiverQualifier = method->receiverQualifier;
                    if (method->name != ZR_NULL) {
                        memberInfo.name = method->name->name;
                    }
                    // 处理返回类型信息
                    if (method->returnType != ZR_NULL) {
                        memberInfo.returnTypeName = extract_type_name_string(cs, method->returnType);
                    } else {
                        memberInfo.returnTypeName = ZR_NULL; // 无返回类型（void）
                    }
                    ZrCore_Array_Init(cs->state,
                                      &memberInfo.genericParameters,
                                      sizeof(SZrTypeGenericParameterInfo),
                                      method->generic != ZR_NULL && method->generic->params != ZR_NULL
                                              ? method->generic->params->count
                                              : 1);
                    compiler_collect_generic_parameter_info(cs, &memberInfo.genericParameters, method->generic);
                    // 方法信息（函数引用等）将在方法编译后设置
                    // 需要将方法编译为函数并存储函数引用索引
                    // 注意：方法编译应该在prototype创建时进行，这里只记录方法信息
                    // 实际的函数编译和引用索引设置需要在方法编译完成后进行
                    if (method->params != ZR_NULL) {
                        memberInfo.parameterCount = (TZrUInt32)method->params->count;
                        compiler_struct_collect_parameter_types(cs, &memberInfo.parameterTypes, method->params);
                        compiler_collect_parameter_passing_modes(cs->state,
                                                                 &memberInfo.parameterPassingModes,
                                                                 method->params);
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    {
                        SZrFunction *compiledMethod =
                                compile_class_member_function(cs, member, ZR_NULL, !method->isStatic, &compiledParameterCount);
                        if (compiledMethod == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            cs->currentTypeNode = oldTypeNode;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state,
                                                     &functionValue,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMethod));
                        memberInfo.compiledFunction = compiledMethod;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                    }
                    break;
                }
                case ZR_AST_STRUCT_META_FUNCTION: {
                    SZrStructMetaFunction *metaFunc = &member->data.structMetaFunction;
                    TZrUInt32 compiledParameterCount = 0;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                        memberInfo.metaType = compiler_resolve_meta_type_name(metaFunc->meta->name);
                        memberInfo.isMetaMethod = memberInfo.metaType != ZR_META_ENUM_MAX;
                    }
                    // 处理返回类型信息
                    if (metaFunc->returnType != ZR_NULL) {
                        memberInfo.returnTypeName = extract_type_name_string(cs, metaFunc->returnType);
                    } else {
                        memberInfo.returnTypeName = ZR_NULL; // 无返回类型（void）
                    }
                    // 元方法的函数引用索引将在编译后设置
                    // TODO: 需要将元方法编译为函数并存储函数引用索引
                    if (metaFunc->params != ZR_NULL) {
                        memberInfo.parameterCount = (TZrUInt32)metaFunc->params->count;
                        compiler_struct_collect_parameter_types(cs, &memberInfo.parameterTypes, metaFunc->params);
                        compiler_collect_parameter_passing_modes(cs->state,
                                                                 &memberInfo.parameterPassingModes,
                                                                 metaFunc->params);
                    }
                    if (memberInfo.isMetaMethod) {
                        SZrFunction *compiledMeta =
                                compile_class_member_function(cs, member, ZR_NULL, !metaFunc->isStatic, &compiledParameterCount);
                        if (compiledMeta == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            cs->currentTypeNode = oldTypeNode;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state,
                                                     &functionValue,
                                                     ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMeta));
                        memberInfo.compiledFunction = compiledMeta;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                    }
                    break;
                }
                default:
                    // 忽略未知成员类型
                    continue;
            }
            
            if (memberInfo.name != ZR_NULL) {
                ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
            }
        }
    }
    
    // 计算struct字段偏移量（仅对非静态字段）
    TZrUInt32 currentOffset = 0;
    for (TZrSize i = 0; i < info.members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info.members, i);
        if (memberInfo == ZR_NULL) {
            continue;
        }
        
        // 只处理非静态字段
        if (memberInfo->memberType == ZR_AST_STRUCT_FIELD && !memberInfo->isStatic) {
            // 获取字段对齐要求
            TZrUInt32 align = ZR_ALIGN_SIZE; // 默认对齐
            if (memberInfo->fieldType != ZR_NULL) {
                align = get_type_alignment(cs, memberInfo->fieldType);
            }
            
            // 应用对齐
            currentOffset = align_offset(currentOffset, align);
            
            // 设置字段偏移量
            memberInfo->fieldOffset = currentOffset;
            
            // 增加偏移量（如果字段大小为0，表示未知类型，使用默认大小）
            TZrUInt32 fieldSize = memberInfo->fieldSize;
            if (fieldSize == 0) {
                fieldSize = ZR_ALIGN_SIZE; // 默认大小
            }
            currentOffset += fieldSize;
        }
    }
    
    // 将 prototype 信息添加到数组
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    
    // 注册类型名称到类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }
    
    // 恢复当前类型名称
    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
    cs->currentTypeNode = oldTypeNode;
}

