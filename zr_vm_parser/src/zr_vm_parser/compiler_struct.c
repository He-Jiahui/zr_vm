//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

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

// 辅助函数：从类型节点提取类型名称字符串
SZrString *extract_type_name_string(SZrCompilerState *cs, SZrType *type) {
    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *baseName = type->name->data.identifier.name;
        // 处理数组维度
        if (type->dimensions > 0) {
            // 构建数组类型名称，例如 "int[]" 或 "int[][]"
            TZrNativeString baseNameStr = ZrCore_String_GetNativeStringShort(baseName);
            if (baseNameStr == ZR_NULL) {
                baseNameStr = *ZrCore_String_GetNativeStringLong(baseName);
            }
            if (baseNameStr != ZR_NULL) {
                TZrSize baseLen = strlen(baseNameStr);
                TZrSize totalLen = baseLen + type->dimensions * 2; // 每个维度需要 "[]"
                char *arrayTypeName = (char *)ZrCore_Memory_RawMalloc(cs->state->global, totalLen + 1);
                if (arrayTypeName != ZR_NULL) {
                    strcpy(arrayTypeName, baseNameStr);
                    for (TZrInt32 i = 0; i < type->dimensions; i++) {
                        strcat(arrayTypeName, "[]");
                    }
                    SZrString *result = ZrCore_String_CreateFromNative(cs->state, arrayTypeName);
                    ZrCore_Memory_RawFree(cs->state->global, arrayTypeName, totalLen + 1);
                    return result;
                }
            }
        }
        return baseName;
    }
    
    // 处理泛型类型（如 Array<int>）
    if (type->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &type->name->data.genericType;
        if (genericType->name != ZR_NULL) {
            TZrNativeString genericNameStr = ZrCore_String_GetNativeStringShort(genericType->name->name);
            if (genericNameStr == ZR_NULL) {
                genericNameStr = *ZrCore_String_GetNativeStringLong(genericType->name->name);
            }
            if (genericNameStr != ZR_NULL) {
                // 构建泛型类型名称，例如 "Array<int>"
                TZrSize nameLen = strlen(genericNameStr);
                TZrSize totalLen = nameLen + 2; // "<" 和 ">"
                
                // 计算参数类型名称的总长度
                if (genericType->params != ZR_NULL && genericType->params->count > 0) {
                    for (TZrSize i = 0; i < genericType->params->count; i++) {
                        SZrAstNode *paramNode = genericType->params->nodes[i];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                            SZrString *paramTypeName = extract_type_name_string(cs, &paramNode->data.type);
                            if (paramTypeName != ZR_NULL) {
                                TZrNativeString paramStr = ZrCore_String_GetNativeStringShort(paramTypeName);
                                if (paramStr == ZR_NULL) {
                                    paramStr = *ZrCore_String_GetNativeStringLong(paramTypeName);
                                }
                                if (paramStr != ZR_NULL) {
                                    totalLen += strlen(paramStr);
                                    if (i > 0) {
                                        totalLen += 2; // ", "
                                    }
                                }
                            }
                        }
                    }
                }
                
                char *genericTypeName = (char *)ZrCore_Memory_RawMalloc(cs->state->global, totalLen + 1);
                if (genericTypeName != ZR_NULL) {
                    strcpy(genericTypeName, genericNameStr);
                    strcat(genericTypeName, "<");
                    if (genericType->params != ZR_NULL && genericType->params->count > 0) {
                        for (TZrSize i = 0; i < genericType->params->count; i++) {
                            if (i > 0) {
                                strcat(genericTypeName, ", ");
                            }
                            SZrAstNode *paramNode = genericType->params->nodes[i];
                            if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                                SZrString *paramTypeName = extract_type_name_string(cs, &paramNode->data.type);
                                if (paramTypeName != ZR_NULL) {
                                    TZrNativeString paramStr = ZrCore_String_GetNativeStringShort(paramTypeName);
                                    if (paramStr == ZR_NULL) {
                                        paramStr = *ZrCore_String_GetNativeStringLong(paramTypeName);
                                    }
                                    if (paramStr != ZR_NULL) {
                                        strcat(genericTypeName, paramStr);
                                    }
                                }
                            }
                        }
                    }
                    strcat(genericTypeName, ">");
                    SZrString *result = ZrCore_String_CreateFromNative(cs->state, genericTypeName);
                    ZrCore_Memory_RawFree(cs->state->global, genericTypeName, totalLen + 1);
                    return result;
                }
            }
        }
        return ZR_NULL;
    }
    
    // 处理元组类型（如 (int, string)）
    if (type->name->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = &type->name->data.tupleType;
        if (tupleType->elements != ZR_NULL && tupleType->elements->count > 0) {
            TZrSize totalLen = 2; // "(" 和 ")"
            // 计算元素类型名称的总长度
            for (TZrSize i = 0; i < tupleType->elements->count; i++) {
                SZrAstNode *elemNode = tupleType->elements->nodes[i];
                if (elemNode != ZR_NULL && elemNode->type == ZR_AST_TYPE) {
                    SZrString *elemTypeName = extract_type_name_string(cs, &elemNode->data.type);
                    if (elemTypeName != ZR_NULL) {
                        TZrNativeString elemStr = ZrCore_String_GetNativeStringShort(elemTypeName);
                        if (elemStr == ZR_NULL) {
                            elemStr = *ZrCore_String_GetNativeStringLong(elemTypeName);
                        }
                        if (elemStr != ZR_NULL) {
                            totalLen += strlen(elemStr);
                            if (i > 0) {
                                totalLen += 2; // ", "
                            }
                        }
                    }
                }
            }
            
            char *tupleTypeName = (char *)ZrCore_Memory_RawMalloc(cs->state->global, totalLen + 1);
            if (tupleTypeName != ZR_NULL) {
                strcpy(tupleTypeName, "(");
                for (TZrSize i = 0; i < tupleType->elements->count; i++) {
                    if (i > 0) {
                        strcat(tupleTypeName, ", ");
                    }
                    SZrAstNode *elemNode = tupleType->elements->nodes[i];
                    if (elemNode != ZR_NULL && elemNode->type == ZR_AST_TYPE) {
                        SZrString *elemTypeName = extract_type_name_string(cs, &elemNode->data.type);
                        if (elemTypeName != ZR_NULL) {
                            TZrNativeString elemStr = ZrCore_String_GetNativeStringShort(elemTypeName);
                            if (elemStr == ZR_NULL) {
                                elemStr = *ZrCore_String_GetNativeStringLong(elemTypeName);
                            }
                            if (elemStr != ZR_NULL) {
                                strcat(tupleTypeName, elemStr);
                            }
                        }
                    }
                }
                strcat(tupleTypeName, ")");
                SZrString *result = ZrCore_String_CreateFromNative(cs->state, tupleTypeName);
                ZrCore_Memory_RawFree(cs->state->global, tupleTypeName, totalLen + 1);
                return result;
            }
        }
        return ZR_NULL;
    }
    
    return ZR_NULL;
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
        ZrParser_Compiler_Error(cs, "Expected struct declaration node", node->location);
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
    cs->currentTypeName = typeName;
    
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
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 2);
    
    // 处理继承关系
    if (structDecl->inherits != ZR_NULL && structDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < structDecl->inherits->count; i++) {
            SZrAstNode *inheritType = structDecl->inherits->nodes[i];
            if (inheritType != ZR_NULL && inheritType->type == ZR_AST_TYPE) {
                SZrType *type = &inheritType->data.type;
                // TODO: 提取类型名称（简化处理，只处理简单类型名）
                if (type->name != ZR_NULL && type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *inheritTypeName = type->name->data.identifier.name;
                    if (inheritTypeName != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
                    }
                }
            }
        }
    }
    
    // 初始化成员数组
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 16);
    cs->currentTypePrototypeInfo = &info;
    
    // 处理成员信息
    if (structDecl->members != ZR_NULL && structDecl->members->count > 0) {
        for (TZrSize i = 0; i < structDecl->members->count; i++) {
            SZrAstNode *member = structDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            SZrTypeMemberInfo memberInfo;
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
            memberInfo.metaType = 0; // ZR_META_ENUM_MAX表示非元方法
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
                    // 方法信息（函数引用等）将在方法编译后设置
                    // 需要将方法编译为函数并存储函数引用索引
                    // 注意：方法编译应该在prototype创建时进行，这里只记录方法信息
                    // 实际的函数编译和引用索引设置需要在方法编译完成后进行
                    if (method->params != ZR_NULL) {
                        memberInfo.parameterCount = (TZrUInt32)method->params->count;
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_STRUCT_META_FUNCTION: {
                    SZrStructMetaFunction *metaFunc = &member->data.structMetaFunction;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                        
                        // 提取元方法类型（如@constructor -> ZR_META_CONSTRUCTOR）
                        // 通过元方法名称匹配
                        TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
                        if (metaName != ZR_NULL) {
                            if (strcmp(metaName, "constructor") == 0) {
                                memberInfo.metaType = ZR_META_CONSTRUCTOR;
                            } else if (strcmp(metaName, "destructor") == 0) {
                                memberInfo.metaType = ZR_META_DESTRUCTOR;
                            } else if (strcmp(metaName, "add") == 0) {
                                memberInfo.metaType = ZR_META_ADD;
                            } else if (strcmp(metaName, "toString") == 0) {
                                memberInfo.metaType = ZR_META_TO_STRING;
                            }
                            // TODO: 添加更多元方法类型匹配
                            memberInfo.isMetaMethod = ZR_TRUE;
                        }
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
    
    // 编译元函数（特别是构造函数）
    if (structDecl->members != ZR_NULL && structDecl->members->count > 0) {
        for (TZrSize i = 0; i < structDecl->members->count; i++) {
            SZrAstNode *member = structDecl->members->nodes[i];
            if (member == ZR_NULL) {
                continue;
            }
            
            if (member->type == ZR_AST_STRUCT_META_FUNCTION) {
                SZrStructMetaFunction *metaFunc = &member->data.structMetaFunction;
                if (metaFunc->meta != ZR_NULL) {
                    TZrNativeString metaName = ZrCore_String_GetNativeStringShort(metaFunc->meta->name);
                    if (metaName != ZR_NULL) {
                        EZrMetaType metaType = 0;
                        if (strcmp(metaName, "constructor") == 0) {
                            metaType = ZR_META_CONSTRUCTOR;
                        } else if (strcmp(metaName, "destructor") == 0) {
                            metaType = ZR_META_DESTRUCTOR;
                        } else if (strcmp(metaName, "add") == 0) {
                            metaType = ZR_META_ADD;
                        } else if (strcmp(metaName, "toString") == 0) {
                            metaType = ZR_META_TO_STRING;
                        }
                        
                        if (metaType != 0) {
                            compile_meta_function(cs, member, metaType);
                        }
                    }
                }
            }
        }
    }
    
    // 恢复当前类型名称
    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
}

