//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

static void compiler_class_append_parameter_type(SZrCompilerState *cs,
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

static void compiler_class_collect_parameter_types(SZrCompilerState *cs,
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

        compiler_class_append_parameter_type(cs, parameterTypes, paramNode->data.parameter.typeInfo);
    }
}

void compile_class_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }
    
    if (node->type != ZR_AST_CLASS_DECLARATION) {
        ZrParser_Compiler_Error(cs, "Expected class declaration node", node->location);
        return;
    }
    
    SZrClassDeclaration *classDecl = &node->data.classDeclaration;
    
    // 获取类型名称
    if (classDecl->name == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Class declaration must have a valid name", node->location);
        return;
    }
    
    SZrString *typeName = classDecl->name->name;
    if (typeName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "Class name is null", node->location);
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
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
    info.accessModifier = classDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    
    // 初始化继承数组
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 4);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 2);
    SZrString *primarySuperTypeName = ZR_NULL;
    // 处理继承关系
    if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
        for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
            SZrAstNode *inheritType = classDecl->inherits->nodes[i];
            SZrString *inheritTypeName = extract_simple_type_name_from_type_node(inheritType);
            if (inheritTypeName != ZR_NULL) {
                if (primarySuperTypeName == ZR_NULL) {
                    primarySuperTypeName = inheritTypeName;
                }
                ZrCore_Array_Push(cs->state, &info.inherits, &inheritTypeName);
            }
        }
    }
    
    // 初始化成员数组
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), 16);
    cs->currentTypePrototypeInfo = &info;
    
    // 处理成员信息
    if (classDecl->members != ZR_NULL && classDecl->members->count > 0) {
        for (TZrSize i = 0; i < classDecl->members->count; i++) {
            SZrAstNode *member = classDecl->members->nodes[i];
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
            memberInfo.metaType = 0;
            memberInfo.isMetaMethod = ZR_FALSE;
            memberInfo.returnTypeName = ZR_NULL;
            
            // 根据成员类型提取信息
            switch (member->type) {
                case ZR_AST_CLASS_FIELD: {
                    SZrClassField *field = &member->data.classField;
                    memberInfo.accessModifier = field->access;
                    memberInfo.isStatic = field->isStatic; // class字段也有isStatic
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
                    break;
                }
                case ZR_AST_CLASS_METHOD: {
                    SZrClassMethod *method = &member->data.classMethod;
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
                    compiler_class_collect_parameter_types(cs, &memberInfo.parameterTypes, method->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    {
                        TZrUInt32 compiledParameterCount = 0;
                        SZrFunction *compiledMethod =
                                compile_class_member_function(cs, member, primarySuperTypeName,
                                                              !method->isStatic, &compiledParameterCount);
                        if (compiledMethod == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMethod));
                        memberInfo.compiledFunction = compiledMethod;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        ZR_UNUSED_PARAMETER(compiledParameterCount);
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_CLASS_PROPERTY: {
                    SZrClassProperty *property = &member->data.classProperty;
                    memberInfo.accessModifier = property->access;
                    memberInfo.isStatic = property->isStatic;
                    memberInfo.memberType = ZR_AST_CLASS_METHOD;
                    if (property->modifier != ZR_NULL) {
                        TZrUInt32 compiledParameterCount = 0;
                        if (property->modifier->type == ZR_AST_PROPERTY_GET) {
                            SZrPropertyGet *getter = &property->modifier->data.propertyGet;
                            if (getter->name != ZR_NULL) {
                                memberInfo.name =
                                        compiler_create_hidden_property_accessor_name(cs, getter->name->name, ZR_FALSE);
                            }
                            if (getter->targetType != ZR_NULL) {
                                memberInfo.returnTypeName = extract_type_name_string(cs, getter->targetType);
                            }
                        } else if (property->modifier->type == ZR_AST_PROPERTY_SET) {
                            SZrPropertySet *setter = &property->modifier->data.propertySet;
                            if (setter->name != ZR_NULL) {
                                memberInfo.name =
                                        compiler_create_hidden_property_accessor_name(cs, setter->name->name, ZR_TRUE);
                            }
                            memberInfo.returnTypeName = ZR_NULL;
                            ZrCore_Array_Init(cs->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), 1);
                            compiler_class_append_parameter_type(cs, &memberInfo.parameterTypes, setter->targetType);
                        }
                        memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;

                        SZrFunction *compiledProperty =
                                compile_class_member_function(cs, member, primarySuperTypeName, !property->isStatic,
                                                              &compiledParameterCount);
                        if (compiledProperty == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledProperty));
                        memberInfo.compiledFunction = compiledProperty;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        ZR_UNUSED_PARAMETER(compiledParameterCount);
                    }
                    memberInfo.isMetaMethod = ZR_FALSE;
                    break;
                }
                case ZR_AST_CLASS_META_FUNCTION: {
                    SZrClassMetaFunction *metaFunc = &member->data.classMetaFunction;
                    memberInfo.accessModifier = metaFunc->access;
                    memberInfo.isStatic = metaFunc->isStatic;
                    if (metaFunc->meta != ZR_NULL) {
                        memberInfo.name = metaFunc->meta->name;
                        
                        // 提取元方法类型
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
                    compiler_class_collect_parameter_types(cs, &memberInfo.parameterTypes, metaFunc->params);
                    memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                    if (memberInfo.isMetaMethod) {
                        TZrUInt32 compiledParameterCount = 0;
                        SZrFunction *compiledMeta =
                                compile_class_member_function(cs, member, primarySuperTypeName,
                                                              !metaFunc->isStatic, &compiledParameterCount);
                        if (compiledMeta == ZR_NULL) {
                            cs->currentTypeName = oldTypeName;
                            cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
                            return;
                        }

                        SZrTypeValue functionValue;
                        ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                               ZR_CAST_RAW_OBJECT_AS_SUPER(compiledMeta));
                        memberInfo.compiledFunction = compiledMeta;
                        memberInfo.functionConstantIndex = add_constant(cs, &functionValue);
                        ZR_UNUSED_PARAMETER(compiledParameterCount);
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
    
    // 将 prototype 信息添加到数组
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    
    // 注册类型名称到类型环境
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, typeName);
    }

    emit_class_static_field_initializers(cs, node);
    if (cs->hasError) {
        cs->currentTypeName = oldTypeName;
        cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
        return;
    }
    
    // 恢复当前类型名称
    cs->currentTypeName = oldTypeName;
    cs->currentTypePrototypeInfo = oldTypePrototypeInfo;
}

// 序列化的prototype信息结构（紧凑二进制格式）
typedef struct SZrSerializedPrototypeInfo {
    // 基本信息
    TZrUInt32 nameStringIndex;              // 类型名称字符串在常量池中的索引
    TZrUInt32 type;                         // EZrObjectPrototypeType
    TZrUInt32 accessModifier;               // EZrAccessModifier
    
    // 继承关系
    TZrUInt32 inheritsCount;                // 继承类型数量
    TZrUInt32 *inheritStringIndices;        // 继承类型名称字符串索引数组（动态分配）
    
    // 成员信息
    TZrUInt32 membersCount;                 // 成员数量
    // 成员数据紧随其后（动态数组）
} SZrSerializedPrototypeInfo;

// 序列化的成员信息结构（紧凑二进制格式）
typedef struct SZrSerializedMemberInfo {
    TZrUInt32 memberType;                   // EZrAstNodeType
    TZrUInt32 nameStringIndex;              // 成员名称字符串在常量池中的索引（如果为0表示无名）
    TZrUInt32 accessModifier;               // EZrAccessModifier
    TZrUInt32 isStatic;                     // TZrBool (0或1)
    
    // 字段特定信息（仅当memberType为STRUCT_FIELD或CLASS_FIELD时有效）
    TZrUInt32 fieldTypeNameStringIndex;     // 字段类型名称字符串索引（如果为0表示无类型名）
    TZrUInt32 fieldOffset;                  // 字段偏移量
    TZrUInt32 fieldSize;                    // 字段大小
    
    // 方法特定信息（仅当memberType为METHOD或META_FUNCTION时有效）
    TZrUInt32 isMetaMethod;                 // TZrBool (0或1)
    TZrUInt32 metaType;                     // EZrMetaType
    TZrUInt32 functionConstantIndex;        // 函数在常量池中的索引
    TZrUInt32 parameterCount;               // 参数数量
} SZrSerializedMemberInfo;

// 将prototype信息序列化为二进制数据（不存储到常量池）
// 编译时使用C原生结构，避免创建VM对象，提高编译速度
// 运行时（module.c）会从 function->prototypeData 读取并创建VM对象
// 返回：ZR_TRUE 表示成功，ZR_FALSE 表示失败
// 注意：outData 指向的内存需要调用者释放
