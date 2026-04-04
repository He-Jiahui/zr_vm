//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

EZrOwnershipQualifier get_member_receiver_qualifier(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_OWNERSHIP_QUALIFIER_NONE;
    }

    switch (node->type) {
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.receiverQualifier;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.receiverQualifier;
        default:
            return ZR_OWNERSHIP_QUALIFIER_NONE;
    }
}

EZrOwnershipQualifier get_implicit_this_ownership_qualifier(EZrOwnershipQualifier receiverQualifier) {
    if (receiverQualifier == ZR_OWNERSHIP_QUALIFIER_UNIQUE ||
        receiverQualifier == ZR_OWNERSHIP_QUALIFIER_BORROWED) {
        return ZR_OWNERSHIP_QUALIFIER_BORROWED;
    }

    return receiverQualifier;
}

// 初始化编译器状态

static EZrProtocolId compiler_protocol_id_from_type_name(SZrString *typeName) {
    const TZrChar *nativeName;
    TZrSize length = 0;

    if (typeName == ZR_NULL) {
        return ZR_PROTOCOL_ID_NONE;
    }

    nativeName = ZrCore_String_GetNativeString(typeName);
    if (nativeName == ZR_NULL) {
        return ZR_PROTOCOL_ID_NONE;
    }

    while (nativeName[length] != '\0' && nativeName[length] != '<') {
        length++;
    }

    if (length == 9 && strncmp(nativeName, "Equatable", length) == 0) {
        return ZR_PROTOCOL_ID_EQUATABLE;
    }
    if (length == 8 && strncmp(nativeName, "Hashable", length) == 0) {
        return ZR_PROTOCOL_ID_HASHABLE;
    }
    if (length == 10 && strncmp(nativeName, "Comparable", length) == 0) {
        return ZR_PROTOCOL_ID_COMPARABLE;
    }
    if (length == 8 && strncmp(nativeName, "Iterable", length) == 0) {
        return ZR_PROTOCOL_ID_ITERABLE;
    }
    if (length == 8 && strncmp(nativeName, "Iterator", length) == 0) {
        return ZR_PROTOCOL_ID_ITERATOR;
    }
    if (length == 9 && strncmp(nativeName, "ArrayLike", length) == 0) {
        return ZR_PROTOCOL_ID_ARRAY_LIKE;
    }

    return ZR_PROTOCOL_ID_NONE;
}

static void compiler_accumulate_protocol_mask_from_type_names(const SZrArray *typeNames, TZrUInt64 *protocolMask) {
    if (typeNames == ZR_NULL || protocolMask == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < typeNames->length; index++) {
        SZrString **typeNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)typeNames, index);
        EZrProtocolId protocolId;

        if (typeNamePtr == ZR_NULL || *typeNamePtr == ZR_NULL) {
            continue;
        }

        protocolId = compiler_protocol_id_from_type_name(*typeNamePtr);
        if (protocolId != ZR_PROTOCOL_ID_NONE) {
            *protocolMask |= ZR_PROTOCOL_BIT(protocolId);
        }
    }
}

static TZrUInt64 compiler_protocol_mask_from_prototype_info(SZrTypePrototypeInfo *info) {
    TZrUInt64 protocolMask = 0;

    if (info == ZR_NULL) {
        return 0;
    }

    compiler_accumulate_protocol_mask_from_type_names(&info->inherits, &protocolMask);
    compiler_accumulate_protocol_mask_from_type_names(&info->implements, &protocolMask);
    return protocolMask;
}

static TZrUInt32 compiler_member_contract_role_from_member_info(const SZrTypeMemberInfo *memberInfo) {
    const TZrChar *memberName;

    if (memberInfo == ZR_NULL || memberInfo->name == ZR_NULL) {
        return ZR_MEMBER_CONTRACT_ROLE_NONE;
    }

    memberName = ZrCore_String_GetNativeString(memberInfo->name);
    if (memberName == ZR_NULL) {
        return ZR_MEMBER_CONTRACT_ROLE_NONE;
    }

    if ((memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) &&
        strcmp(memberName, "current") == 0) {
        return ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD;
    }

    if (memberInfo->memberType == ZR_AST_STRUCT_METHOD || memberInfo->memberType == ZR_AST_CLASS_METHOD) {
        if (strcmp(memberName, "getIterator") == 0) {
            return ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT;
        }
        if (strcmp(memberName, "moveNext") == 0) {
            return ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT;
        }
        if (strcmp(memberName, "current") == 0) {
            return ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_METHOD;
        }
    }

    return ZR_MEMBER_CONTRACT_ROLE_NONE;
}

static TZrBool compiler_add_decorator_name_array_constant(SZrCompilerState *cs,
                                                          const SZrArray *decorators,
                                                          TZrUInt32 *outConstantIndex) {
    SZrObject *decoratorArray;
    SZrTypeValue arrayValue;

    if (outConstantIndex != ZR_NULL) {
        *outConstantIndex = 0;
    }

    if (cs == ZR_NULL || decorators == ZR_NULL || outConstantIndex == ZR_NULL || decorators->length == 0) {
        return ZR_TRUE;
    }

    decoratorArray = ZrCore_Object_NewCustomized(cs->state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (decoratorArray == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Object_Init(cs->state, decoratorArray);

    for (TZrSize index = 0; index < decorators->length; index++) {
        SZrTypeDecoratorInfo *decoratorInfo = (SZrTypeDecoratorInfo *)ZrCore_Array_Get((SZrArray *)decorators, index);
        SZrTypeValue keyValue;
        SZrTypeValue entryValue;

        if (decoratorInfo == ZR_NULL || decoratorInfo->name == ZR_NULL) {
            continue;
        }

        ZrCore_Value_InitAsInt(cs->state, &keyValue, (TZrInt64)index);
        ZrCore_Value_InitAsRawObject(cs->state,
                                     &entryValue,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorInfo->name));
        entryValue.type = ZR_VALUE_TYPE_STRING;
        ZrCore_Object_SetValue(cs->state, decoratorArray, &keyValue, &entryValue);
    }

    ZrCore_Value_InitAsRawObject(cs->state, &arrayValue, ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorArray));
    arrayValue.type = ZR_VALUE_TYPE_ARRAY;
    *outConstantIndex = add_constant(cs, &arrayValue);
    return ZR_TRUE;
}

TZrBool serialize_prototype_info_to_binary(SZrCompilerState *cs, SZrTypePrototypeInfo *info, 
                                                 TZrByte **outData, TZrSize *outSize) {
    if (cs == ZR_NULL || info == ZR_NULL || info->name == ZR_NULL || outData == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 注意：为了保持格式兼容，我们仍然使用字符串索引
    // 但这些索引现在指向 prototype 数据内部的字符串表，而不是常量池
    // TODO: 为了简化实现，我们暂时仍然使用常量池索引，但后续会改为内部字符串表
    
    // 1. 使用C原生结构收集数据，避免创建VM对象
    // 先将所有字符串添加到常量池，获取索引（临时方案，后续改为内部字符串表）
    SZrTypeValue nameValue;
    ZrCore_Value_InitAsRawObject(cs->state, &nameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(info->name));
    nameValue.type = ZR_VALUE_TYPE_STRING;
    TZrUInt32 nameStringIndex = add_constant(cs, &nameValue);
    
    // 2. 添加继承类型名称字符串到常量池
    TZrUInt32 *inheritStringIndices = ZR_NULL;
    TZrUInt32 *decoratorNameIndices = ZR_NULL;
    TZrUInt32 inheritsCount = (TZrUInt32)info->inherits.length;
    TZrUInt32 decoratorsCount = (TZrUInt32)info->decorators.length;
    if (inheritsCount > 0) {
        inheritStringIndices = (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, inheritsCount * sizeof(TZrUInt32));
        if (inheritStringIndices == ZR_NULL) {
            return ZR_FALSE;
        }
        
        for (TZrSize i = 0; i < info->inherits.length; i++) {
            SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&info->inherits, i);
            if (inheritTypeNamePtr != ZR_NULL && *inheritTypeNamePtr != ZR_NULL) {
                SZrTypeValue inheritValue;
                ZrCore_Value_InitAsRawObject(cs->state, &inheritValue, ZR_CAST_RAW_OBJECT_AS_SUPER(*inheritTypeNamePtr));
                inheritValue.type = ZR_VALUE_TYPE_STRING;
                inheritStringIndices[i] = add_constant(cs, &inheritValue);
            } else {
                inheritStringIndices[i] = 0;
            }
        }
    }

    if (decoratorsCount > 0) {
        decoratorNameIndices =
                (TZrUInt32 *)ZrCore_Memory_RawMalloc(cs->state->global, decoratorsCount * sizeof(TZrUInt32));
        if (decoratorNameIndices == ZR_NULL) {
            if (inheritStringIndices != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
            }
            return ZR_FALSE;
        }

        for (TZrSize i = 0; i < info->decorators.length; i++) {
            SZrTypeDecoratorInfo *decoratorInfo =
                    (SZrTypeDecoratorInfo *)ZrCore_Array_Get(&info->decorators, i);
            if (decoratorInfo != ZR_NULL && decoratorInfo->name != ZR_NULL) {
                SZrTypeValue decoratorNameValue;
                ZrCore_Value_InitAsRawObject(cs->state,
                                            &decoratorNameValue,
                                            ZR_CAST_RAW_OBJECT_AS_SUPER(decoratorInfo->name));
                decoratorNameValue.type = ZR_VALUE_TYPE_STRING;
                decoratorNameIndices[i] = add_constant(cs, &decoratorNameValue);
            } else {
                decoratorNameIndices[i] = 0;
            }
        }
    }
    
    // 3. 计算序列化数据大小（使用C原生结构）
    TZrUInt32 membersCount = (TZrUInt32)info->members.length;
    TZrSize serializedSize = sizeof(SZrCompiledPrototypeInfo) + 
                             (inheritsCount > 0 ? inheritsCount * sizeof(TZrUInt32) : 0) +
                             (decoratorsCount > 0 ? decoratorsCount * sizeof(TZrUInt32) : 0) +
                             membersCount * sizeof(SZrCompiledMemberInfo);
    
    // 4. 分配序列化数据缓冲区（C原生内存，非VM对象）
    TZrByte *serializedData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, serializedSize);
    if (serializedData == ZR_NULL) {
        if (inheritStringIndices != ZR_NULL) {
            ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
        }
        if (decoratorNameIndices != ZR_NULL) {
            ZrCore_Memory_RawFree(cs->state->global, decoratorNameIndices, decoratorsCount * sizeof(TZrUInt32));
        }
        return ZR_FALSE;
    }
    
    // 5. 填充序列化数据（使用C原生结构，避免指针，所有数据直接嵌入）
    SZrCompiledPrototypeInfo *protoInfo = (SZrCompiledPrototypeInfo *)serializedData;
    ZrCore_Memory_RawSet(protoInfo, 0, sizeof(*protoInfo));
    protoInfo->nameStringIndex = nameStringIndex;
    protoInfo->type = (TZrUInt32)info->type;
    protoInfo->accessModifier = (TZrUInt32)info->accessModifier;
    protoInfo->inheritsCount = inheritsCount;
    protoInfo->membersCount = membersCount;
    protoInfo->protocolMask = compiler_protocol_mask_from_prototype_info(info);
    protoInfo->hasDecoratorMetadata = info->hasDecoratorMetadata ? ZR_TRUE : ZR_FALSE;
    protoInfo->decoratorMetadataConstantIndex =
            info->hasDecoratorMetadata ? add_constant(cs, &info->decoratorMetadataValue) : 0;
    protoInfo->decoratorsCount = decoratorsCount;
    
    // 复制继承类型索引数组到序列化数据中（紧跟在结构体后面）
    TZrUInt32 *embeddedInheritIndices = (TZrUInt32 *)(serializedData + sizeof(SZrCompiledPrototypeInfo));
    if (inheritsCount > 0 && inheritStringIndices != ZR_NULL) {
        memcpy(embeddedInheritIndices, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
        ZrCore_Memory_RawFree(cs->state->global, inheritStringIndices, inheritsCount * sizeof(TZrUInt32));
    }

    TZrUInt32 *embeddedDecoratorIndices = embeddedInheritIndices + inheritsCount;
    if (decoratorsCount > 0 && decoratorNameIndices != ZR_NULL) {
        memcpy(embeddedDecoratorIndices, decoratorNameIndices, decoratorsCount * sizeof(TZrUInt32));
        ZrCore_Memory_RawFree(cs->state->global, decoratorNameIndices, decoratorsCount * sizeof(TZrUInt32));
    }

    // 序列化成员信息（紧跟在继承数组后面）
    SZrCompiledMemberInfo *members = (SZrCompiledMemberInfo *)(serializedData + 
                                                                 sizeof(SZrCompiledPrototypeInfo) +
                                                                 inheritsCount * sizeof(TZrUInt32) +
                                                                 decoratorsCount * sizeof(TZrUInt32));
    for (TZrSize i = 0; i < info->members.length; i++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, i);
        if (memberInfo == ZR_NULL) {
            continue;
        }
        
        SZrCompiledMemberInfo *compiledMember = &members[i];
        ZrCore_Memory_RawSet(compiledMember, 0, sizeof(*compiledMember));
        compiledMember->memberType = (TZrUInt32)memberInfo->memberType;
        compiledMember->accessModifier = (TZrUInt32)memberInfo->accessModifier;
        compiledMember->isStatic = memberInfo->isStatic ? ZR_TRUE : ZR_FALSE;
        compiledMember->isConst = memberInfo->isConst ? ZR_TRUE : ZR_FALSE;
        compiledMember->isUsingManaged = memberInfo->isUsingManaged ? ZR_TRUE : ZR_FALSE;
        compiledMember->ownershipQualifier = (TZrUInt32)memberInfo->ownershipQualifier;
        compiledMember->callsClose = memberInfo->callsClose ? ZR_TRUE : ZR_FALSE;
        compiledMember->callsDestructor = memberInfo->callsDestructor ? ZR_TRUE : ZR_FALSE;
        compiledMember->declarationOrder = memberInfo->declarationOrder;
        compiledMember->contractRole = compiler_member_contract_role_from_member_info(memberInfo);
        compiledMember->hasDecoratorMetadata = memberInfo->hasDecoratorMetadata ? ZR_TRUE : ZR_FALSE;
        compiledMember->decoratorMetadataConstantIndex =
                memberInfo->hasDecoratorMetadata ? add_constant(cs, &memberInfo->decoratorMetadataValue) : 0;
        compiledMember->hasDecoratorNames = memberInfo->decorators.length > 0 ? ZR_TRUE : ZR_FALSE;
        compiledMember->decoratorNamesConstantIndex = 0;
        if (compiledMember->hasDecoratorNames &&
            !compiler_add_decorator_name_array_constant(cs,
                                                        &memberInfo->decorators,
                                                        &compiledMember->decoratorNamesConstantIndex)) {
            ZrCore_Memory_RawFree(cs->state->global, serializedData, serializedSize);
            return ZR_FALSE;
        }

        // 添加成员名称字符串到常量池（临时方案）
        if (memberInfo->name != ZR_NULL) {
            SZrTypeValue memberNameValue;
            ZrCore_Value_InitAsRawObject(cs->state, &memberNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->name));
            memberNameValue.type = ZR_VALUE_TYPE_STRING;
            compiledMember->nameStringIndex = add_constant(cs, &memberNameValue);
        } else {
            compiledMember->nameStringIndex = 0;
        }
        
        // 字段特定信息
        if (memberInfo->memberType == ZR_AST_STRUCT_FIELD || memberInfo->memberType == ZR_AST_CLASS_FIELD) {
            if (memberInfo->fieldTypeName != ZR_NULL) {
                SZrTypeValue fieldTypeNameValue;
                ZrCore_Value_InitAsRawObject(cs->state, &fieldTypeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->fieldTypeName));
                fieldTypeNameValue.type = ZR_VALUE_TYPE_STRING;
                compiledMember->fieldTypeNameStringIndex = add_constant(cs, &fieldTypeNameValue);
            } else {
                compiledMember->fieldTypeNameStringIndex = 0;
            }
            compiledMember->fieldOffset = memberInfo->fieldOffset;
            compiledMember->fieldSize = memberInfo->fieldSize;
            // 方法字段清零
            compiledMember->isMetaMethod = ZR_FALSE;
            compiledMember->metaType = 0;
            compiledMember->functionConstantIndex = 0;
            compiledMember->parameterCount = 0;
            compiledMember->returnTypeNameStringIndex = 0;
        }
        
        // 方法特定信息
        if (memberInfo->memberType == ZR_AST_STRUCT_METHOD || 
            memberInfo->memberType == ZR_AST_STRUCT_META_FUNCTION ||
            memberInfo->memberType == ZR_AST_CLASS_METHOD ||
            memberInfo->memberType == ZR_AST_CLASS_META_FUNCTION) {
            compiledMember->isMetaMethod = memberInfo->isMetaMethod ? ZR_TRUE : ZR_FALSE;
            compiledMember->metaType = (TZrUInt32)memberInfo->metaType;
            if (memberInfo->compiledFunction != ZR_NULL) {
                SZrTypeValue functionValue;
                ZrCore_Value_InitAsRawObject(cs->state, &functionValue,
                                       ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->compiledFunction));
                compiledMember->functionConstantIndex = add_constant(cs, &functionValue);
            } else {
                compiledMember->functionConstantIndex = memberInfo->functionConstantIndex;
            }
            compiledMember->parameterCount = memberInfo->parameterCount;
            // 处理返回类型名称
            if (memberInfo->returnTypeName != ZR_NULL) {
                SZrTypeValue returnTypeNameValue;
                ZrCore_Value_InitAsRawObject(cs->state, &returnTypeNameValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberInfo->returnTypeName));
                returnTypeNameValue.type = ZR_VALUE_TYPE_STRING;
                compiledMember->returnTypeNameStringIndex = add_constant(cs, &returnTypeNameValue);
            } else {
                compiledMember->returnTypeNameStringIndex = 0; // 无返回类型（void）
            }
            // 字段字段清零
            compiledMember->fieldTypeNameStringIndex = 0;
            compiledMember->fieldOffset = 0;
            compiledMember->fieldSize = 0;
        } else {
            // 非方法成员，返回类型字段清零
            compiledMember->returnTypeNameStringIndex = 0;
        }
    }
    
    // 6. 返回序列化数据（不存储到常量池）
    *outData = serializedData;
    *outSize = serializedSize;
    
    return ZR_TRUE;
}

static void compiler_compile_compile_time_runtime_support(SZrCompilerState *cs, SZrAstNode *statement) {
    SZrAstNode *declaration;
    TZrBool oldSupportFlag;

    if (cs == ZR_NULL || statement == ZR_NULL || statement->type != ZR_AST_COMPILE_TIME_DECLARATION || cs->hasError) {
        return;
    }

    if (cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL ||
        !cs->state->global->emitCompileTimeRuntimeSupport) {
        return;
    }

    declaration = statement->data.compileTimeDeclaration.declaration;
    if (declaration == ZR_NULL) {
        return;
    }

    oldSupportFlag = cs->isCompilingCompileTimeRuntimeSupport;
    cs->isCompilingCompileTimeRuntimeSupport = ZR_TRUE;

    switch (declaration->type) {
        case ZR_AST_VARIABLE_DECLARATION: {
            EZrAccessModifier oldAccessModifier = declaration->data.variableDeclaration.accessModifier;
            declaration->data.variableDeclaration.accessModifier = ZR_ACCESS_PROTECTED;
            ZrParser_Statement_Compile(cs, declaration);
            declaration->data.variableDeclaration.accessModifier = oldAccessModifier;
            break;
        }
        case ZR_AST_FUNCTION_DECLARATION:
            compile_function_declaration(cs, declaration);
            break;
        default:
            break;
    }

    cs->isCompilingCompileTimeRuntimeSupport = oldSupportFlag;
}

// 编译脚本
void compile_script(SZrCompilerState *cs, SZrAstNode *node) {
    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_SCRIPT) {
        ZrParser_Compiler_Error(cs, "Expected script node", node->location);
        return;
    }

    SZrScript *script = &node->data.script;

    if (cs->semanticContext != ZR_NULL) {
        ZrParser_SemanticContext_Reset(cs->semanticContext);
        if (cs->hirModule != ZR_NULL) {
            ZrParser_HirModule_Free(cs->state, cs->hirModule);
            cs->hirModule = ZR_NULL;
        }
        cs->hirModule = ZrParser_HirModule_New(cs->state, cs->semanticContext, node);
    }

    // 设置脚本级别标志（用于区分脚本级变量和函数内变量）
    cs->isScriptLevel = ZR_TRUE;
    
    // 保存脚本 AST 引用（用于类型查找）
    cs->scriptAst = node;

    // 1. 编译模块声明（如果有）
    if (script->moduleName != ZR_NULL) {
        // 处理模块声明（注册模块到全局模块表）
        // 注意：模块注册在运行时进行，编译器只需要记录模块名称
        // 模块名称可以通过entry function的常量池或元数据存储
        // 运行时加载模块时会创建模块对象并注册到全局模块注册表
        // TODO: 这里暂时不生成特殊指令，模块注册在模块加载时自动进行
    }

    // 2. 首先收集并执行所有编译期声明
    if (script->statements != ZR_NULL) {
        enter_scope(cs);

        // 第一遍：收集并执行编译期声明
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL && stmt->type == ZR_AST_COMPILE_TIME_DECLARATION) {
                // 执行编译期声明
                ZrParser_CompileTimeDeclaration_Execute(cs, stmt);
                
                // 如果遇到致命错误，停止编译
                if (cs->hasFatalError) {
                    printf("  Fatal compile-time error encountered, stopping compilation\n");
                    return;
                }
            }
        }

        if (cs->hasCompileTimeError) {
            cs->hasError = ZR_TRUE;
            return;
        }

        ZrParser_Compiler_PredeclareExternBindings(cs, script->statements);
        if (cs->hasError) {
            return;
        }

        ZrParser_Compiler_PredeclareFunctionBindings(cs, script->statements);
        if (cs->hasError) {
            return;
        }

        // 第二遍：编译运行时代码
        for (TZrSize i = 0; i < script->statements->count; i++) {
            SZrAstNode *stmt = script->statements->nodes[i];
            if (stmt != ZR_NULL) {
                if (stmt->type == ZR_AST_COMPILE_TIME_DECLARATION) {
                    compiler_compile_compile_time_runtime_support(cs, stmt);
                    continue;
                }
                
                // 根据语句类型编译
                switch (stmt->type) {
                    case ZR_AST_FUNCTION_DECLARATION:
                        compile_function_declaration(cs, stmt);
                        break;
                    case ZR_AST_VARIABLE_DECLARATION:
                    case ZR_AST_EXPRESSION_STATEMENT:
                    case ZR_AST_USING_STATEMENT:
                    case ZR_AST_BLOCK:
                    case ZR_AST_RETURN_STATEMENT:
                    case ZR_AST_THROW_STATEMENT:
                    case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
                    case ZR_AST_IF_EXPRESSION:
                    case ZR_AST_WHILE_LOOP:
                    case ZR_AST_FOR_LOOP:
                    case ZR_AST_FOREACH_LOOP:
                        ZrParser_Statement_Compile(cs, stmt);
                        break;
                    case ZR_AST_TEST_DECLARATION:
                        compile_test_declaration(cs, stmt);
                        break;
                    case ZR_AST_STRUCT_DECLARATION:
                        compile_struct_declaration(cs, stmt);
                        break;
                    case ZR_AST_EXTERN_BLOCK:
                        compile_extern_block_declaration(cs, stmt);
                        break;
                    case ZR_AST_CLASS_DECLARATION:
                        compile_class_declaration(cs, stmt);
                        break;
                    case ZR_AST_INTERFACE_DECLARATION:
                        compile_interface_declaration(cs, stmt);
                        break;
                    case ZR_AST_ENUM_DECLARATION:
                        // 处理enum声明
                        // TODO: enum可以编译为常量或对象，这里暂时跳过
                        // 后续可以实现enum的编译
                        break;
                    default:
                        // 其他顶层声明类型（intermediate等）
                        // TODO: 目前先跳过，后续实现
                        printf("    Skipping statement type %d (not implemented yet)\n", stmt->type);
                        break;
                }

                if (cs->hasCompileTimeError) {
                    cs->hasError = ZR_TRUE;
                    return;
                }

                // 即使有错误，也继续编译后续语句（除非是致命错误）
                // 这样可以尽可能多地编译成功的语句
                if (cs->hasError && !cs->hasFatalError) {
                    printf("    Compilation error at statement %zu, resetting error and continuing...\n", i);
                    // 清除当前语句的阻塞状态，保留总体失败标记以继续收集后续错误
                    cs->hasError = ZR_FALSE;
                } else if (cs->hasFatalError) {
                    printf("  Fatal error encountered, stopping compilation\n");
                    return;
                }
            }
        }
        if (cs->hadRecoverableError) {
            cs->hasError = ZR_TRUE;
            return;
        }
        if (!cs->hasError && !compiler_build_script_typed_metadata(cs)) {
            ZrParser_Compiler_Error(cs, "Failed to build typed metadata for compiled script", node->location);
            return;
        }

        printf("  Finished compiling statements, total instructions: %zu\n", cs->instructionCount);
        exit_scope(cs);
    }

    // 3. 在返回前添加导出收集代码（如果有导出的变量）
    // 导出收集在运行时进行（在内部模块导入 helper 执行完 __entry 后）
    // 这里只需要确保导出信息被正确记录到函数中
    // 导出的变量信息已存储在 cs->pubVariables 和 cs->proVariables 中
    // 这些信息将在编译完成后复制到函数的 exportedVariables 字段中
    
    // 4. 如果没有显式返回，添加隐式返回
    if (!cs->hasError) {
        // 使用 instructions.length 而不是 instructionCount，确保同步
        if (cs->instructions.length == 0) {
            // 如果没有任何指令，添加隐式返回 null
            TZrUInt32 resultSlot = allocate_stack_slot(cs);
            SZrTypeValue nullValue;
            ZrCore_Value_ResetAsNull(&nullValue);
            TZrUInt32 constantIndex = add_constant(cs, &nullValue);
            TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                       (TZrInt32) constantIndex);
            emit_instruction(cs, inst);

            TZrInstruction returnInst =
                    create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
            emit_instruction(cs, returnInst);
        } else {
            // 检查最后一条指令是否是 RETURN
            if (cs->instructions.length > 0) {
                // 使用 length 而不是 instructionCount，因为 length 是数组的实际长度
                TZrInstruction *lastInst =
                        (TZrInstruction *) ZrCore_Array_Get(&cs->instructions, cs->instructions.length - 1);
                if (lastInst != ZR_NULL) {
                    EZrInstructionCode lastOpcode = (EZrInstructionCode) lastInst->instruction.operationCode;
                    if (lastOpcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
                        // 添加隐式返回 null
                        TZrUInt32 resultSlot = allocate_stack_slot(cs);
                        SZrTypeValue nullValue;
                        ZrCore_Value_ResetAsNull(&nullValue);
                        TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                        TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT),
                                                                   (TZrUInt16) resultSlot, (TZrInt32) constantIndex);
                        emit_instruction(cs, inst);

                        TZrInstruction returnInst =
                                create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                        emit_instruction(cs, returnInst);
                    }
                }
            } else {
                // 如果没有任何指令，添加隐式返回 null
                TZrUInt32 resultSlot = allocate_stack_slot(cs);
                SZrTypeValue nullValue;
                ZrCore_Value_ResetAsNull(&nullValue);
                TZrUInt32 constantIndex = add_constant(cs, &nullValue);
                TZrInstruction inst = create_instruction_1(ZR_INSTRUCTION_ENUM(GET_CONSTANT), (TZrUInt16) resultSlot,
                                                           (TZrInt32) constantIndex);
                emit_instruction(cs, inst);

                TZrInstruction returnInst =
                        create_instruction_2(ZR_INSTRUCTION_ENUM(FUNCTION_RETURN), 1, (TZrUInt16) resultSlot, 0);
                emit_instruction(cs, returnInst);
            }
        }
    }
    
    // 5. 将 prototype 信息序列化为二进制数据并存储到 function->prototypeData
    // 运行时创建逻辑将在内部模块导入 helper 中实现（在创建模块后）
    // 使用紧凑二进制格式存储，不再使用常量池
    
    if (cs->typePrototypes.length > 0) {
        // 计算所有 prototype 数据的总大小
        TZrSize totalPrototypeDataSize = 0;
        TZrSize serializablePrototypeCount = 0;
        TZrByte **prototypeDataArray = (TZrByte **)ZrCore_Memory_RawMalloc(cs->state->global, cs->typePrototypes.length * sizeof(TZrByte *));
        TZrSize *prototypeDataSizes = (TZrSize *)ZrCore_Memory_RawMalloc(cs->state->global, cs->typePrototypes.length * sizeof(TZrSize));
        
        if (prototypeDataArray == ZR_NULL || prototypeDataSizes == ZR_NULL) {
            if (prototypeDataArray != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray, cs->typePrototypes.length * sizeof(TZrByte *));
            }
            if (prototypeDataSizes != ZR_NULL) {
                ZrCore_Memory_RawFree(cs->state->global, prototypeDataSizes, cs->typePrototypes.length * sizeof(TZrSize));
            }
        } else {
            // 序列化每个 prototype 信息
            for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, i);
                if (info == ZR_NULL || info->name == ZR_NULL || info->isImportedNative) {
                    prototypeDataArray[i] = ZR_NULL;
                    prototypeDataSizes[i] = 0;
                    continue;
                }
                
                // 序列化prototype信息为二进制数据（不存储到常量池）
                TZrByte *prototypeData = ZR_NULL;
                TZrSize prototypeDataSize = 0;
                if (serialize_prototype_info_to_binary(cs, info, &prototypeData, &prototypeDataSize)) {
                    prototypeDataArray[i] = prototypeData;
                    prototypeDataSizes[i] = prototypeDataSize;
                    totalPrototypeDataSize += prototypeDataSize;
                    serializablePrototypeCount++;
                } else {
                    prototypeDataArray[i] = ZR_NULL;
                    prototypeDataSizes[i] = 0;
                }
            }
            
            // 将所有 prototype 数据合并到一个连续的缓冲区中
            if (totalPrototypeDataSize > 0) {
                // 在数据前添加一个头部：prototype 数量（TZrUInt32）
                TZrSize finalDataSize = sizeof(TZrUInt32) + totalPrototypeDataSize;
                TZrByte *finalPrototypeData = (TZrByte *)ZrCore_Memory_RawMalloc(cs->state->global, finalDataSize);
                if (finalPrototypeData != ZR_NULL) {
                    // 写入 prototype 数量
                    TZrUInt32 *prototypeCountPtr = (TZrUInt32 *)finalPrototypeData;
                    *prototypeCountPtr = (TZrUInt32)serializablePrototypeCount;
                    
                    // 复制每个 prototype 的数据
                    TZrByte *currentPos = finalPrototypeData + sizeof(TZrUInt32);
                    for (TZrSize i = 0; i < cs->typePrototypes.length; i++) {
                        if (prototypeDataArray[i] != ZR_NULL && prototypeDataSizes[i] > 0) {
                            memcpy(currentPos, prototypeDataArray[i], prototypeDataSizes[i]);
                            currentPos += prototypeDataSizes[i];
                            // 释放单个 prototype 数据
                            ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray[i], prototypeDataSizes[i]);
                        }
                    }
                    
                    // 存储到 function
                    cs->currentFunction->prototypeData = finalPrototypeData;
                    cs->currentFunction->prototypeDataLength = (TZrUInt32)finalDataSize;
                    cs->currentFunction->prototypeCount = (TZrUInt32)serializablePrototypeCount;
                }
            } else {
                cs->currentFunction->prototypeData = ZR_NULL;
                cs->currentFunction->prototypeDataLength = 0;
                cs->currentFunction->prototypeCount = 0;
            }
            
            // 释放临时数组
            ZrCore_Memory_RawFree(cs->state->global, prototypeDataArray, cs->typePrototypes.length * sizeof(TZrByte *));
            ZrCore_Memory_RawFree(cs->state->global, prototypeDataSizes, cs->typePrototypes.length * sizeof(TZrSize));
        }
    } else {
        cs->currentFunction->prototypeData = ZR_NULL;
        cs->currentFunction->prototypeDataLength = 0;
        cs->currentFunction->prototypeCount = 0;
    }
    
    // 重置脚本级别标志
    cs->isScriptLevel = ZR_FALSE;
}

// 主编译入口（占位实现）
SZrFunction *ZrParser_Compiler_Compile(SZrState *state, SZrAstNode *ast) {
    if (state == ZR_NULL || ast == ZR_NULL) {
        return ZR_NULL;
    }

    SZrCompilerState cs;
    ZrParser_CompilerState_Init(&cs, state);

    if (!compiler_validate_task_effects(&cs, ast)) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 创建新函数
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 编译脚本
    compile_script(&cs, ast);

    if (cs.hasError) {
        // 错误信息已在 ZrParser_Compiler_Error 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.currentFunction);
        }
        if (cs.topLevelFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.topLevelFunction);
        }
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 如果有顶层函数声明，返回它；否则返回脚本函数
    SZrFunction *func = (cs.topLevelFunction != ZR_NULL) ? cs.topLevelFunction : cs.currentFunction;
    if (!compiler_assemble_final_function(&cs,
                                          func,
                                          ast,
                                          func == cs.currentFunction,
                                          cs.topLevelFunction != ZR_NULL)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    // 执行指令优化（占位实现，后续填充具体逻辑）
    optimize_instructions(&cs);

    if (!compiler_build_function_semir_metadata(state, func)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    if (!compiler_quicken_execbc_function(state, func)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_NULL;
    }

    ZrParser_CompilerState_Free(&cs);
    return func;
}

ZR_PARSER_API void ZrParser_Compiler_CompileStructDeclaration(SZrCompilerState *cs, SZrAstNode *node) {
    compile_struct_declaration(cs, node);
}

ZR_PARSER_API void ZrParser_Compiler_CompileClassDeclaration(SZrCompilerState *cs, SZrAstNode *node) {
    compile_class_declaration(cs, node);
}

ZR_PARSER_API void ZrParser_Compiler_CompileInterfaceDeclaration(SZrCompilerState *cs, SZrAstNode *node) {
    compile_interface_declaration(cs, node);
}

// 编译 AST 为函数和测试函数列表（新接口）
TZrBool ZrParser_Compiler_CompileWithTests(SZrState *state, SZrAstNode *ast, SZrCompileResult *result) {
    if (state == ZR_NULL || ast == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    // 初始化结果结构体
    result->mainFunction = ZR_NULL;
    result->testFunctions = ZR_NULL;
    result->testFunctionCount = 0;

    SZrCompilerState cs;
    ZrParser_CompilerState_Init(&cs, state);

    if (!compiler_validate_task_effects(&cs, ast)) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    // 创建新函数
    cs.currentFunction = ZrCore_Function_New(state);
    if (cs.currentFunction == ZR_NULL) {
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    // 编译脚本
    compile_script(&cs, ast);

    if (cs.hasError) {
        // 错误信息已在 ZrParser_Compiler_Error 中输出（包含行列号）
        printf("\n=== Compilation Summary ===\n");
        printf("Status: FAILED\n");
        printf("Reason: %s\n", (cs.errorMessage != ZR_NULL) ? cs.errorMessage : "Unknown error");
        if (cs.currentFunction != ZR_NULL) {
            ZrCore_Function_Free(state, cs.currentFunction);
        }
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    // 将编译结果复制到 SZrFunction（与 ZrParser_Compiler_Compile 共用装配逻辑）
    SZrFunction *func = cs.currentFunction;
    SZrGlobalState *global = state->global;
    if (!compiler_assemble_final_function(&cs, func, ast, ZR_TRUE, ZR_FALSE)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    // 执行指令优化（占位实现，后续填充具体逻辑）
    optimize_instructions(&cs);

    // 复制测试函数列表
    result->mainFunction = func;
    if (cs.testFunctions.length > 0) {
        TZrSize testFuncSize = cs.testFunctions.length * sizeof(SZrFunction *);
        result->testFunctions = (SZrFunction **) ZrCore_Memory_RawMallocWithType(
                global, testFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (result->testFunctions == ZR_NULL) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
        // 复制测试函数指针
        SZrFunction **srcTestArray = (SZrFunction **) cs.testFunctions.head;
        for (TZrSize i = 0; i < cs.testFunctions.length; i++) {
            result->testFunctions[i] = srcTestArray[i];
        }
        result->testFunctionCount = cs.testFunctions.length;
    }

    for (TZrSize i = 0; i < result->testFunctionCount; i++) {
        if (!compiler_attach_detached_function_prototype_context(state, result->testFunctions[i], func)) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
    }

    if (!compiler_build_function_semir_metadata(state, func)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < result->testFunctionCount; i++) {
        if (!compiler_build_function_semir_metadata(state, result->testFunctions[i])) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
    }

    if (!compiler_quicken_execbc_function(state, func)) {
        ZrCore_Function_Free(state, func);
        ZrParser_CompilerState_Free(&cs);
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i < result->testFunctionCount; i++) {
        if (!compiler_quicken_execbc_function(state, result->testFunctions[i])) {
            ZrCore_Function_Free(state, func);
            ZrParser_CompilerState_Free(&cs);
            return ZR_FALSE;
        }
    }

    ZrParser_CompilerState_Free(&cs);
    return ZR_TRUE;
}

// 释放编译结果
void ZrParser_CompileResult_Free(SZrState *state, SZrCompileResult *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    // 释放测试函数数组（函数对象本身由GC管理，不需要释放）
    if (result->testFunctions != ZR_NULL && result->testFunctionCount > 0) {
        SZrGlobalState *global = state->global;
        TZrSize testFuncSize = result->testFunctionCount * sizeof(SZrFunction *);
        ZrCore_Memory_RawFreeWithType(global, result->testFunctions, testFuncSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        result->testFunctions = ZR_NULL;
        result->testFunctionCount = 0;
    }

    // 主函数由调用者负责释放（如果不需要可以调用ZrFunctionFree）
    // 这里不释放，因为调用者可能还需要使用
}

// 编译源代码为函数（封装了从解析到编译的全流程）
struct SZrFunction *ZrParser_Source_Compile(struct SZrState *state, const TZrChar *source, TZrSize sourceLength, struct SZrString *sourceName) {
    if (state == ZR_NULL || source == ZR_NULL || sourceLength == 0) {
        return ZR_NULL;
    }
    
    // 解析源代码为AST
    SZrAstNode *ast = ZrParser_Parse(state, source, sourceLength, sourceName);
    if (ast == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 编译AST为函数
    SZrFunction *func = ZrParser_Compiler_Compile(state, ast);
    
    // 释放AST
    ZrParser_Ast_Free(state, ast);
    
    return func;
}

// 注册 compileSource 函数到 globalState
void ZrParser_ToGlobalState_Register(struct SZrState *state) {
    if (state == ZR_NULL || state->global == ZR_NULL) {
        return;
    }
    
    // 使用 API 设置 compileSource 函数指针，避免直接访问内部结构
    ZrCore_GlobalState_SetCompileSource(state->global, ZrParser_Source_Compile);
}
