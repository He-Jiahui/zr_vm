//
// Created by Auto on 2025/01/XX.
//

#include "compiler_internal.h"

TZrBool extern_compiler_has_registered_type(SZrCompilerState *cs, SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

void extern_compiler_register_struct_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) {
    SZrStructDeclaration *structDecl;
    SZrTypePrototypeInfo info;
    TZrUInt32 currentOffset = 0;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_STRUCT_DECLARATION || cs->hasError) {
        return;
    }

    structDecl = &declarationNode->data.structDeclaration;
    if (structDecl->name == ZR_NULL || structDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, structDecl->name->name)) {
        return;
    }

    memset(&info, 0, sizeof(info));
    info.name = structDecl->name->name;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
    info.accessModifier = structDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(cs->state, &info.decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            SZrAstNode *member = structDecl->members->nodes[index];
            SZrTypeMemberInfo memberInfo;
            TZrInt64 explicitOffset = 0;

            if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD ||
                member->data.structField.name == ZR_NULL || member->data.structField.name->name == ZR_NULL) {
                continue;
            }

            memset(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.memberType = ZR_AST_STRUCT_FIELD;
            memberInfo.name = member->data.structField.name->name;
            memberInfo.accessModifier = member->data.structField.access;
            memberInfo.isStatic = member->data.structField.isStatic;
            memberInfo.isConst = member->data.structField.isConst;
            memberInfo.fieldType = member->data.structField.typeInfo;
            memberInfo.fieldTypeName = extract_type_name_string(cs, member->data.structField.typeInfo);
            memberInfo.fieldSize = calculate_type_size(cs, member->data.structField.typeInfo);
            if (memberInfo.fieldSize == 0) {
                memberInfo.fieldSize = ZR_ALIGN_SIZE;
            }

            if (extern_compiler_decorators_get_int_arg(member->data.structField.decorators, "offset", &explicitOffset)) {
                memberInfo.fieldOffset = (TZrUInt32)explicitOffset;
                currentOffset = memberInfo.fieldOffset + memberInfo.fieldSize;
            } else {
                currentOffset = align_offset(currentOffset, get_type_alignment(cs, member->data.structField.typeInfo));
                memberInfo.fieldOffset = currentOffset;
                currentOffset += memberInfo.fieldSize;
            }

            ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
        }
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

void extern_compiler_register_enum_prototype(SZrCompilerState *cs, SZrAstNode *declarationNode) {
    SZrEnumDeclaration *enumDecl;
    SZrTypePrototypeInfo info;
    SZrString *underlyingName = ZR_NULL;

    if (cs == ZR_NULL || declarationNode == ZR_NULL || declarationNode->type != ZR_AST_ENUM_DECLARATION || cs->hasError) {
        return;
    }

    enumDecl = &declarationNode->data.enumDeclaration;
    if (enumDecl->name == ZR_NULL || enumDecl->name->name == ZR_NULL ||
        extern_compiler_has_registered_type(cs, enumDecl->name->name)) {
        return;
    }

    memset(&info, 0, sizeof(info));
    info.name = enumDecl->name->name;
    info.type = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
    info.accessModifier = enumDecl->accessModifier;
    info.isImportedNative = ZR_FALSE;
    info.allowValueConstruction = ZR_TRUE;
    info.allowBoxedConstruction = ZR_TRUE;
    ZrCore_Array_Init(cs->state, &info.inherits, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info.implements, sizeof(SZrString *), 1);
    ZrCore_Array_Init(cs->state, &info.decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state, &info.members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);

    if (enumDecl->baseType != ZR_NULL) {
        underlyingName = extract_type_name_string(cs, enumDecl->baseType);
    }
    if (underlyingName == ZR_NULL) {
        underlyingName = extern_compiler_decorators_get_string_arg(enumDecl->decorators, "underlying");
    }
    if (underlyingName == ZR_NULL) {
        underlyingName = ZrCore_String_CreateFromNative(cs->state, "i32");
    }
    info.enumValueTypeName = underlyingName;

    if (enumDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < enumDecl->members->count; index++) {
            SZrAstNode *memberNode = enumDecl->members->nodes[index];
            SZrTypeMemberInfo memberInfo;

            if (memberNode == ZR_NULL || memberNode->type != ZR_AST_ENUM_MEMBER ||
                memberNode->data.enumMember.name == ZR_NULL || memberNode->data.enumMember.name->name == ZR_NULL) {
                continue;
            }

            memset(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.memberType = ZR_AST_CLASS_FIELD;
            memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
            memberInfo.isStatic = ZR_TRUE;
            memberInfo.name = memberNode->data.enumMember.name->name;
            memberInfo.fieldTypeName = info.name;
            ZrCore_Array_Push(cs->state, &info.members, &memberInfo);
        }
    }

    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &info);
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, info.name);
    }
    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterType(cs->state, cs->compileTimeTypeEnv, info.name);
    }
}

void compiler_register_extern_block_bindings(SZrCompilerState *cs, SZrExternBlock *externBlock) {
    if (cs == ZR_NULL || externBlock == ZR_NULL || externBlock->declarations == ZR_NULL || cs->hasError) {
        return;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        if (declaration->type == ZR_AST_STRUCT_DECLARATION) {
            extern_compiler_register_struct_prototype(cs, declaration);
        } else if (declaration->type == ZR_AST_ENUM_DECLARATION) {
            extern_compiler_register_enum_prototype(cs, declaration);
        }
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        if (declaration == ZR_NULL) {
            continue;
        }

        switch (declaration->type) {
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
                compiler_register_extern_function_type_binding_to_env(
                        cs,
                        declaration,
                        cs->typeEnv,
                        &declaration->data.externFunctionDeclaration);
                compiler_register_extern_function_type_binding_to_env(
                        cs,
                        declaration,
                        cs->compileTimeTypeEnv,
                        &declaration->data.externFunctionDeclaration);
                break;
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL) {
                    SZrString *delegateName = declaration->data.externDelegateDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, delegateName, delegateName);
                    compiler_register_named_value_binding_to_env(cs,
                                                                 cs->compileTimeTypeEnv,
                                                                 delegateName,
                                                                 delegateName);
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL) {
                    SZrString *structName = declaration->data.structDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, structName, structName);
                    compiler_register_named_value_binding_to_env(cs, cs->compileTimeTypeEnv, structName, structName);
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL) {
                    SZrString *enumName = declaration->data.enumDeclaration.name->name;
                    compiler_register_named_value_binding_to_env(cs, cs->typeEnv, enumName, enumName);
                    compiler_register_named_value_binding_to_env(cs, cs->compileTimeTypeEnv, enumName, enumName);
                }
                break;
            default:
                break;
        }
    }
}

void ZrParser_Compiler_PredeclareExternBindings(SZrCompilerState *cs, SZrAstNodeArray *statements) {
    if (cs == ZR_NULL || statements == ZR_NULL || cs->hasError || cs->externBindingsPredeclared) {
        return;
    }

    for (TZrSize index = 0; index < statements->count; index++) {
        SZrAstNode *statement = statements->nodes[index];
        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK) {
            continue;
        }
        compiler_register_extern_block_bindings(cs, &statement->data.externBlock);
        if (cs->hasError) {
            return;
        }
    }

    cs->externBindingsPredeclared = ZR_TRUE;
}

void compile_extern_block_declaration(SZrCompilerState *cs, SZrAstNode *node) {
    SZrExternBlock *externBlock;
    SZrString *libraryName;
    SZrString *ffiModuleName;
    SZrString *loadLibraryName;
    SZrString *getSymbolName;
    TZrUInt32 ffiModuleSlot = ZR_PARSER_SLOT_NONE;
    TZrUInt32 librarySlot = ZR_PARSER_SLOT_NONE;

    if (cs == ZR_NULL || node == ZR_NULL || cs->hasError) {
        return;
    }

    if (node->type != ZR_AST_EXTERN_BLOCK) {
        ZrParser_Compiler_Error(cs, "Expected extern block declaration", node->location);
        return;
    }

    externBlock = &node->data.externBlock;
    if (externBlock->libraryName == ZR_NULL || externBlock->libraryName->type != ZR_AST_STRING_LITERAL) {
        ZrParser_Compiler_Error(cs, "extern block requires a string library specifier", node->location);
        return;
    }

    libraryName = externBlock->libraryName->data.stringLiteral.value;
    ffiModuleName = ZrCore_String_CreateFromNative(cs->state, "zr.ffi");
    loadLibraryName = ZrCore_String_CreateFromNative(cs->state, "loadLibrary");
    getSymbolName = ZrCore_String_CreateFromNative(cs->state, "getSymbol");
    if (libraryName == ZR_NULL || ffiModuleName == ZR_NULL || loadLibraryName == ZR_NULL || getSymbolName == ZR_NULL) {
        ZrParser_Compiler_Error(cs, "failed to allocate extern ffi helper strings", node->location);
        return;
    }

    if (!cs->externBindingsPredeclared) {
        compiler_register_extern_block_bindings(cs, externBlock);
        if (cs->hasError) {
            return;
        }
    }

    if (externBlock->declarations == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < externBlock->declarations->count; index++) {
        SZrAstNode *declaration = externBlock->declarations->nodes[index];
        SZrString *bindingName = ZR_NULL;
        SZrTypeValue descriptorValue;

        if (declaration == ZR_NULL) {
            continue;
        }

        ZrCore_Value_ResetAsNull(&descriptorValue);
        switch (declaration->type) {
            case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                if (declaration->data.externDelegateDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.externDelegateDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_build_delegate_descriptor_value(cs, externBlock, declaration, ZR_TRUE, &descriptorValue)) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, &descriptorValue);
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                break;
            case ZR_AST_STRUCT_DECLARATION:
                if (declaration->data.structDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.structDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_build_struct_descriptor_value(cs, externBlock, declaration, &descriptorValue)) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, &descriptorValue);
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                break;
            case ZR_AST_ENUM_DECLARATION:
                if (declaration->data.enumDeclaration.name != ZR_NULL) {
                    bindingName = declaration->data.enumDeclaration.name->name;
                }
                if (bindingName != ZR_NULL &&
                    extern_compiler_build_enum_descriptor_value(cs, declaration, &descriptorValue)) {
                    TZrUInt32 localSlot = allocate_local_var(cs, bindingName);
                    emit_constant_to_slot(cs, localSlot, &descriptorValue);
                    ZrParser_Compiler_TrimStackToSlot(cs, localSlot);
                }
                break;
            case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
                SZrExternFunctionDeclaration *functionDecl = &declaration->data.externFunctionDeclaration;
                SZrString *entryName = functionDecl->name != ZR_NULL ? functionDecl->name->name : ZR_NULL;
                SZrString *entryOverride = extern_compiler_decorators_get_string_arg(functionDecl->decorators, "entry");
                TZrUInt32 localSlot;
                SZrTypeValue symbolArguments[2];

                if (functionDecl->name == ZR_NULL || functionDecl->name->name == ZR_NULL) {
                    ZrParser_Compiler_Error(cs, "extern function declaration is missing a name", declaration->location);
                    return;
                }
                if (entryOverride != ZR_NULL) {
                    entryName = entryOverride;
                }

                if (!extern_compiler_build_signature_descriptor_value(cs,
                                                                     externBlock,
                                                                     functionDecl->params,
                                                                     functionDecl->args,
                                                                     functionDecl->returnType,
                                                                     functionDecl->decorators,
                                                                     ZR_FALSE,
                                                                     declaration->location,
                                                                     &symbolArguments[1])) {
                    return;
                }
                ZrCore_Value_InitAsRawObject(cs->state, &symbolArguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(entryName));
                symbolArguments[0].type = ZR_VALUE_TYPE_STRING;

                if (ffiModuleSlot == ZR_PARSER_SLOT_NONE) {
                    SZrString *hiddenFfiName = create_hidden_extern_local_name(cs, "ffi");
                    SZrString *hiddenLibraryName = create_hidden_extern_local_name(cs, "library");
                    SZrTypeValue loadArguments[1];
                    if (hiddenFfiName == ZR_NULL || hiddenLibraryName == ZR_NULL) {
                        ZrParser_Compiler_Error(cs, "failed to allocate hidden extern locals", declaration->location);
                        return;
                    }

                    ffiModuleSlot = allocate_local_var(cs, hiddenFfiName);
                    if (!extern_compiler_emit_import_module_to_local(cs, ffiModuleName, ffiModuleSlot, declaration->location)) {
                        return;
                    }

                    librarySlot = allocate_local_var(cs, hiddenLibraryName);
                    ZrCore_Value_InitAsRawObject(cs->state, &loadArguments[0], ZR_CAST_RAW_OBJECT_AS_SUPER(libraryName));
                    loadArguments[0].type = ZR_VALUE_TYPE_STRING;
                    if (!extern_compiler_emit_module_function_call_to_local(cs,
                                                                           ffiModuleSlot,
                                                                           loadLibraryName,
                                                                           loadArguments,
                                                                           1,
                                                                           librarySlot,
                                                                           declaration->location)) {
                        return;
                    }
                }

                localSlot = allocate_local_var(cs, functionDecl->name->name);
                if (!extern_compiler_emit_method_call_to_local(cs,
                                                               librarySlot,
                                                               getSymbolName,
                                                               symbolArguments,
                                                               2,
                                                               localSlot,
                                                               declaration->location)) {
                    return;
                }
                break;
            }
            default:
                break;
        }

        if (cs->hasError) {
            return;
        }
    }
}

void ZrParser_Compiler_CompileExternBlock(SZrCompilerState *cs, SZrAstNode *node) {
    compile_extern_block_declaration(cs, node);
}

// 编译元函数（@constructor, @destructor 等）
