//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compiler_internal.h"
#include "compile_time_binding_metadata.h"
#include "compile_time_executor_internal.h"
#include "compile_statement_internal.h"
#include "type_inference_internal.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_instruction_conf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TZrBool compile_statement_trace_enabled(void);
static void compile_statement_trace(const TZrChar *format, ...);

ZR_PARSER_API void ZrParser_Statement_Compile(SZrCompilerState *cs, SZrAstNode *node);
static void compile_using_statement(SZrCompilerState *cs, SZrAstNode *node);
static TZrNativeString compile_statement_get_native_string(SZrString *value);
SZrAstNode *find_type_declaration(SZrCompilerState *cs, SZrString *typeName);
SZrTypePrototypeInfo *find_compiler_type_prototype(SZrCompilerState *cs, SZrString *typeName);
SZrTypeMemberInfo *find_compiler_type_member(SZrCompilerState *cs,
                                             SZrString *typeName,
                                             SZrString *memberName);

static void compile_statement_register_type_value_alias(SZrCompilerState *cs,
                                                        SZrString *name,
                                                        SZrAstNode *valueNode) {
    SZrInferredType aliasType;

    if (cs == ZR_NULL || name == ZR_NULL || valueNode == ZR_NULL ||
        valueNode->type != ZR_AST_TYPE_LITERAL_EXPRESSION ||
        valueNode->data.typeLiteralExpression.typeInfo == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(cs, valueNode->data.typeLiteralExpression.typeInfo, &aliasType)) {
        ZrParser_InferredType_Free(cs->state, &aliasType);
        return;
    }

    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            ZrParser_InferredType_Free(cs->state, &binding->type);
            ZrParser_InferredType_Copy(cs->state, &binding->type, &aliasType);
            ZrParser_InferredType_Free(cs->state, &aliasType);
            return;
        }
    }

    {
        SZrTypeBinding binding;
        binding.name = name;
        ZrParser_InferredType_Init(cs->state, &binding.type, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &binding.type, &aliasType);
        ZrCore_Array_Push(cs->state, &cs->typeValueAliases, &binding);
    }

    ZrParser_InferredType_Free(cs->state, &aliasType);
}

static TZrBool compile_statement_has_type_value_alias(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void compile_statement_register_type_binding_alias(SZrCompilerState *cs,
                                                          SZrString *name,
                                                          SZrString *qualifiedTypeName) {
    SZrInferredType aliasType;

    if (cs == ZR_NULL || name == ZR_NULL || qualifiedTypeName == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_InitFull(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, qualifiedTypeName);
    for (TZrSize index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            ZrParser_InferredType_Free(cs->state, &binding->type);
            ZrParser_InferredType_Copy(cs->state, &binding->type, &aliasType);
            ZrParser_InferredType_Free(cs->state, &aliasType);
            return;
        }
    }

    {
        SZrTypeBinding binding;
        binding.name = name;
        ZrParser_InferredType_Init(cs->state, &binding.type, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &binding.type, &aliasType);
        ZrCore_Array_Push(cs->state, &cs->typeValueAliases, &binding);
    }

    ZrParser_InferredType_Free(cs->state, &aliasType);
}

static SZrString *compile_statement_build_qualified_type_name(SZrCompilerState *cs,
                                                              SZrString *moduleName,
                                                              SZrString *typeName) {
    TZrNativeString moduleNameText;
    TZrNativeString typeNameText;
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (cs == ZR_NULL || moduleName == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNameText = compile_statement_get_native_string(moduleName);
    typeNameText = compile_statement_get_native_string(typeName);
    if (moduleNameText == ZR_NULL || typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    written = snprintf(buffer, sizeof(buffer), "%s.%s", moduleNameText, typeNameText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static TZrBool compile_statement_report_duplicate_type_name(SZrCompilerState *cs,
                                                            SZrString *name,
                                                            SZrFileRange location) {
    TZrNativeString nameText;
    TZrChar errorMessage[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = compile_statement_get_native_string(name);
    if (nameText != ZR_NULL) {
        snprintf(errorMessage,
                 sizeof(errorMessage),
                 "Type name '%s' is already declared in this context",
                 nameText);
    } else {
        snprintf(errorMessage, sizeof(errorMessage), "Type name is already declared in this context");
    }

    ZrParser_Compiler_Error(cs, errorMessage, location);
    return ZR_FALSE;
}

static TZrBool compile_statement_register_explicit_type_name(SZrCompilerState *cs,
                                                             SZrString *name,
                                                             SZrFileRange location) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (compile_statement_has_type_value_alias(cs, name) ||
        (cs->typeEnv != ZR_NULL && ZrParser_TypeEnvironment_LookupType(cs->typeEnv, name)) ||
        find_type_declaration(cs, name) != ZR_NULL) {
        return compile_statement_report_duplicate_type_name(cs, name, location);
    }

    if (cs->typeEnv != ZR_NULL && !ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, name)) {
        ZrParser_Compiler_Error(cs, "Failed to register explicit imported type binding", location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool compile_statement_try_register_imported_destructured_type_aliases(SZrCompilerState *cs,
                                                                                 SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrString *moduleName;
    SZrTypePrototypeInfo *modulePrototype;
    SZrDestructuringObject *destructuring;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_TRUE;
    }

    decl = &node->data.variableDeclaration;
    if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_DESTRUCTURING_OBJECT ||
        decl->value == ZR_NULL || decl->value->type != ZR_AST_IMPORT_EXPRESSION ||
        decl->value->data.importExpression.modulePath == ZR_NULL ||
        decl->value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
        decl->value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_TRUE;
    }

    moduleName = decl->value->data.importExpression.modulePath->data.stringLiteral.value;
    if (moduleName == ZR_NULL || !ensure_import_module_compile_info(cs, moduleName)) {
        return ZR_TRUE;
    }

    modulePrototype = find_compiler_type_prototype(cs, moduleName);
    if (modulePrototype == ZR_NULL || modulePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return ZR_TRUE;
    }

    destructuring = &decl->pattern->data.destructuringObject;
    if (destructuring->keys == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < destructuring->keys->count; index++) {
        SZrAstNode *keyNode = destructuring->keys->nodes[index];
        SZrString *keyName;
        SZrTypeMemberInfo *memberInfo;

        if (keyNode == ZR_NULL || keyNode->type != ZR_AST_IDENTIFIER_LITERAL ||
            keyNode->data.identifier.name == ZR_NULL) {
            continue;
        }

        keyName = keyNode->data.identifier.name;
        memberInfo = find_compiler_type_member(cs, moduleName, keyName);
        if (memberInfo == ZR_NULL || memberInfo->fieldTypeName == ZR_NULL ||
            !ZrCore_String_Equal(memberInfo->fieldTypeName, keyName) ||
            find_compiler_type_prototype(cs, keyName) == ZR_NULL) {
            continue;
        }

        if (!compile_statement_register_explicit_type_name(cs, keyName, keyNode->location)) {
            return ZR_FALSE;
        }

        {
            SZrString *qualifiedTypeName = compile_statement_build_qualified_type_name(cs, moduleName, keyName);
            if (qualifiedTypeName != ZR_NULL) {
                compile_statement_register_type_binding_alias(cs, keyName, qualifiedTypeName);
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool compile_statement_refill_io_chunk(SZrIo *io) {
    SZrState *state;
    TZrSize readSize = 0;
    TZrBytePtr buffer;

    if (io == ZR_NULL || io->read == ZR_NULL || io->state == ZR_NULL) {
        return ZR_FALSE;
    }

    state = io->state;
    ZR_THREAD_UNLOCK(state);
    buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TZrBytePtr compile_statement_read_all_from_io(SZrState *state, SZrIo *io, TZrSize *outSize) {
    SZrGlobalState *global;
    TZrSize capacity;
    TZrSize totalSize;
    TZrBytePtr buffer;

    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    capacity = (io->remained > 0) ? io->remained : ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY;
    buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    totalSize = 0;
    while (io->remained > 0 || compile_statement_refill_io_chunk(io)) {
        if (totalSize + io->remained + 1 > capacity) {
            TZrSize newCapacity = capacity;
            TZrBytePtr newBuffer;

            while (totalSize + io->remained + 1 > newCapacity) {
                newCapacity *= 2;
            }

            newBuffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global,
                                                                    newCapacity + 1,
                                                                    ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (newBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(newBuffer, buffer, totalSize);
            }

            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = newBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, io->remained);
        totalSize += io->remained;
        io->pointer += io->remained;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static TZrNativeString compile_statement_get_native_string(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return (TZrNativeString)ZrCore_String_GetNativeString(value);
}

static SZrAstNode *compile_statement_find_imported_decorator_meta_method(SZrAstNodeArray *members,
                                                                         const TZrChar *metaName,
                                                                         TZrBool isStructDecorator) {
    if (members == ZR_NULL || metaName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize i = 0; i < members->count; i++) {
        SZrAstNode *member = members->nodes[i];
        SZrIdentifier *meta = ZR_NULL;

        if (member == ZR_NULL) {
            continue;
        }

        if (!isStructDecorator && member->type == ZR_AST_CLASS_META_FUNCTION) {
            meta = member->data.classMetaFunction.meta;
        } else if (isStructDecorator && member->type == ZR_AST_STRUCT_META_FUNCTION) {
            meta = member->data.structMetaFunction.meta;
        }

        if (meta != ZR_NULL && meta->name != ZR_NULL && ZrCore_String_GetNativeString(meta->name) != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(meta->name), metaName) == 0) {
            return member;
        }
    }

    return ZR_NULL;
}

static SZrImportedCompileTimeModule *compile_statement_find_imported_compile_time_module(SZrCompilerState *cs,
                                                                                         SZrString *moduleName) {
    if (cs == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->importedCompileTimeModules.length; index++) {
        SZrImportedCompileTimeModule **modulePtr =
                (SZrImportedCompileTimeModule **)ZrCore_Array_Get(&cs->importedCompileTimeModules, index);
        if (modulePtr != ZR_NULL && *modulePtr != ZR_NULL && (*modulePtr)->moduleName != ZR_NULL &&
            ZrCore_String_Equal((*modulePtr)->moduleName, moduleName)) {
            return *modulePtr;
        }
    }

    return ZR_NULL;
}

static SZrImportedCompileTimeModule *compile_statement_find_imported_compile_time_module_alias(
        SZrCompilerState *cs,
        SZrString *aliasName) {
    if (cs == ZR_NULL || aliasName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->importedCompileTimeModuleAliases.length; index++) {
        SZrImportedCompileTimeModuleAlias *alias =
                (SZrImportedCompileTimeModuleAlias *)ZrCore_Array_Get(&cs->importedCompileTimeModuleAliases, index);
        if (alias != ZR_NULL && alias->aliasName != ZR_NULL && alias->module != ZR_NULL &&
            ZrCore_String_Equal(alias->aliasName, aliasName)) {
            return alias->module;
        }
    }

    return ZR_NULL;
}

typedef struct SZrImportedCompileTimeVariableBindingContext {
    SZrCompilerState *cs;
    SZrImportedCompileTimeModule *module;
    SZrCompileTimeBindingSourceVariable *variables;
    TZrSize variableCount;
} SZrImportedCompileTimeVariableBindingContext;

static void compile_statement_typed_type_ref_init_unknown(SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static SZrFunctionCompileTimeVariableInfo *compile_statement_find_imported_compile_time_variable(
        const SZrImportedCompileTimeModule *module,
        SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeVariables.length; index++) {
        SZrFunctionCompileTimeVariableInfo **infoPtr =
                (SZrFunctionCompileTimeVariableInfo **)ZrCore_Array_Get((SZrArray *)&module->compileTimeVariables, index);
        if (infoPtr != ZR_NULL && *infoPtr != ZR_NULL && (*infoPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*infoPtr)->name, name)) {
            return *infoPtr;
        }
    }

    return ZR_NULL;
}

static void compile_statement_free_imported_compile_time_variables(SZrState *state, SZrImportedCompileTimeModule *module) {
    if (state == ZR_NULL || module == ZR_NULL || !module->compileTimeVariables.isValid ||
        module->compileTimeVariables.head == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < module->compileTimeVariables.length; index++) {
        SZrFunctionCompileTimeVariableInfo **infoPtr =
                (SZrFunctionCompileTimeVariableInfo **)ZrCore_Array_Get(&module->compileTimeVariables, index);
        if (infoPtr == ZR_NULL || *infoPtr == ZR_NULL) {
            continue;
        }
        if ((*infoPtr)->pathBindings != ZR_NULL && (*infoPtr)->pathBindingCount > 0) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          (*infoPtr)->pathBindings,
                                          sizeof(SZrFunctionCompileTimePathBinding) * (*infoPtr)->pathBindingCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        ZrCore_Memory_RawFreeWithType(state->global,
                                      *infoPtr,
                                      sizeof(SZrFunctionCompileTimeVariableInfo),
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    ZrCore_Array_Free(state, &module->compileTimeVariables);
}

static SZrCompileTimeBindingSourceVariable *compile_statement_find_imported_binding_source_variable(
        TZrPtr userData,
        SZrString *name) {
    SZrImportedCompileTimeVariableBindingContext *context =
            (SZrImportedCompileTimeVariableBindingContext *)userData;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->variableCount; index++) {
        if (context->variables[index].name != ZR_NULL &&
            ZrCore_String_Equal(context->variables[index].name, name)) {
            return &context->variables[index];
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeFunction *compile_statement_find_imported_compile_time_function(
        const SZrImportedCompileTimeModule *module,
        SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get((SZrArray *)&module->compileTimeFunctions, index);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return *funcPtr;
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeFunction *compile_statement_find_imported_compile_time_function_callback(
        TZrPtr userData,
        SZrString *name) {
    SZrImportedCompileTimeVariableBindingContext *context =
            (SZrImportedCompileTimeVariableBindingContext *)userData;
    return context != ZR_NULL ? compile_statement_find_imported_compile_time_function(context->module, name) : ZR_NULL;
}

static SZrCompileTimeDecoratorClass *compile_statement_find_imported_compile_time_decorator_class(
        const SZrImportedCompileTimeModule *module,
        SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeDecoratorClasses.length; index++) {
        SZrCompileTimeDecoratorClass **classPtr =
                (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get((SZrArray *)&module->compileTimeDecoratorClasses,
                                                                  index);
        if (classPtr != ZR_NULL && *classPtr != ZR_NULL && (*classPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*classPtr)->name, name)) {
            return *classPtr;
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeDecoratorClass *compile_statement_find_imported_compile_time_decorator_class_callback(
        TZrPtr userData,
        SZrString *name) {
    SZrImportedCompileTimeVariableBindingContext *context =
            (SZrImportedCompileTimeVariableBindingContext *)userData;
    return context != ZR_NULL ? compile_statement_find_imported_compile_time_decorator_class(context->module, name)
                              : ZR_NULL;
}

static SZrCompileTimeFunction *compile_statement_create_imported_compile_time_function(SZrCompilerState *cs,
                                                                                       SZrAstNode *node,
                                                                                       SZrFileRange location) {
    SZrFunctionDeclaration *funcDecl;
    SZrCompileTimeFunction *func;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_FUNCTION_DECLARATION ||
        node->data.functionDeclaration.name == ZR_NULL ||
        node->data.functionDeclaration.name->name == ZR_NULL) {
        return ZR_NULL;
    }

    funcDecl = &node->data.functionDeclaration;
    func = (SZrCompileTimeFunction *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                     sizeof(SZrCompileTimeFunction),
                                                                     ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (func == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(func, 0, sizeof(*func));
    ZrCore_Array_Init(cs->state,
                      &func->paramTypes,
                      sizeof(SZrInferredType),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    ZrCore_Array_Init(cs->state,
                      &func->paramNames,
                      sizeof(SZrString *),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    ZrCore_Array_Init(cs->state,
                      &func->paramHasDefaultValues,
                      sizeof(TZrBool),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    ZrCore_Array_Init(cs->state,
                      &func->paramDefaultValues,
                      sizeof(SZrTypeValue),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);

    func->name = funcDecl->name->name;
    func->declaration = node;
    func->location = location;

    if (funcDecl->returnType != ZR_NULL &&
        !ZrParser_AstTypeToInferredType_Convert(cs, funcDecl->returnType, &func->returnType)) {
        ZrParser_InferredType_Free(cs->state, &func->returnType);
        ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
    }

    if (funcDecl->params != ZR_NULL) {
        for (TZrSize i = 0; i < funcDecl->params->count; i++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[i];
            SZrInferredType paramType;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            if (paramNode->data.parameter.typeInfo != ZR_NULL &&
                !ZrParser_AstTypeToInferredType_Convert(cs, paramNode->data.parameter.typeInfo, &paramType)) {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            } else if (paramNode->data.parameter.typeInfo == ZR_NULL) {
                ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            }

            ZrCore_Array_Push(cs->state, &func->paramTypes, &paramType);
            {
                SZrString *paramName = paramNode->data.parameter.name != ZR_NULL
                                               ? paramNode->data.parameter.name->name
                                               : ZR_NULL;
                TZrBool hasDefaultValue = ZR_FALSE;
                SZrTypeValue defaultValue;

                ZrCore_Value_ResetAsNull(&defaultValue);
                ZrCore_Array_Push(cs->state, &func->paramNames, &paramName);
                ZrCore_Array_Push(cs->state, &func->paramHasDefaultValues, &hasDefaultValue);
                ZrCore_Array_Push(cs->state, &func->paramDefaultValues, &defaultValue);
            }
        }
    }

    return func;
}

static SZrFunctionCompileTimeVariableInfo *compile_statement_create_imported_compile_time_variable_info(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrFileRange location) {
    SZrFunctionCompileTimeVariableInfo *info;
    SZrVariableDeclaration *decl;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION ||
        node->data.variableDeclaration.pattern == ZR_NULL ||
        node->data.variableDeclaration.pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.variableDeclaration.pattern->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    info = (SZrFunctionCompileTimeVariableInfo *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionCompileTimeVariableInfo),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(info, 0, sizeof(*info));
    decl = &node->data.variableDeclaration;
    info->name = decl->pattern->data.identifier.name;
    info->lineInSourceStart = location.start.line > 0 ? (TZrUInt32)location.start.line : 0;
    info->lineInSourceEnd = location.end.line > 0 ? (TZrUInt32)location.end.line : 0;
    compile_statement_typed_type_ref_init_unknown(&info->type);
    return info;
}

static void compile_statement_io_typed_type_ref_to_inferred(SZrCompilerState *cs,
                                                            const SZrIoFunctionTypedTypeRef *typeRef,
                                                            SZrInferredType *result) {
    if (cs == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (typeRef == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return;
    }

    if (typeRef->isArray) {
        SZrInferredType elementType;

        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = typeRef->ownershipQualifier;
        result->isNullable = typeRef->isNullable;
        ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
        ZrParser_InferredType_InitFull(cs->state,
                                       &elementType,
                                       typeRef->elementBaseType,
                                       ZR_FALSE,
                                       typeRef->elementTypeName);
        ZrCore_Array_Push(cs->state, &result->elementTypes, &elementType);
        return;
    }

    if (typeRef->typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state,
                                       result,
                                       typeRef->baseType,
                                       typeRef->isNullable,
                                       typeRef->typeName);
    } else {
        ZrParser_InferredType_Init(cs->state, result, typeRef->baseType);
        result->isNullable = typeRef->isNullable;
    }
    result->ownershipQualifier = typeRef->ownershipQualifier;
}

static SZrFunctionCompileTimeVariableInfo *compile_statement_create_imported_compile_time_variable_projection(
        SZrCompilerState *cs,
        const SZrIoFunctionCompileTimeVariableInfo *sourceInfo) {
    SZrFunctionCompileTimeVariableInfo *info;

    if (cs == ZR_NULL || sourceInfo == ZR_NULL || sourceInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    info = (SZrFunctionCompileTimeVariableInfo *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionCompileTimeVariableInfo),
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (info == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(info, 0, sizeof(*info));
    info->name = sourceInfo->name;
    info->lineInSourceStart = sourceInfo->lineInSourceStart;
    info->lineInSourceEnd = sourceInfo->lineInSourceEnd;
    compile_statement_typed_type_ref_init_unknown(&info->type);
    info->type.baseType = sourceInfo->type.baseType;
    info->type.isNullable = sourceInfo->type.isNullable;
    info->type.ownershipQualifier = sourceInfo->type.ownershipQualifier;
    info->type.isArray = sourceInfo->type.isArray;
    info->type.typeName = sourceInfo->type.typeName;
    info->type.elementBaseType = sourceInfo->type.elementBaseType;
    info->type.elementTypeName = sourceInfo->type.elementTypeName;

    if (sourceInfo->pathBindingsLength > 0) {
        info->pathBindings = (SZrFunctionCompileTimePathBinding *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrFunctionCompileTimePathBinding) * sourceInfo->pathBindingsLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (info->pathBindings == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          info,
                                          sizeof(SZrFunctionCompileTimeVariableInfo),
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_NULL;
        }
        ZrCore_Memory_RawSet(info->pathBindings,
                             0,
                             sizeof(SZrFunctionCompileTimePathBinding) * sourceInfo->pathBindingsLength);
        for (TZrSize index = 0; index < sourceInfo->pathBindingsLength; index++) {
            info->pathBindings[index].path = sourceInfo->pathBindings[index].path;
            info->pathBindings[index].targetKind = sourceInfo->pathBindings[index].targetKind;
            info->pathBindings[index].targetName = sourceInfo->pathBindings[index].targetName;
        }
        info->pathBindingCount = (TZrUInt32)sourceInfo->pathBindingsLength;
    }

    return info;
}

static TZrBool compile_statement_io_constant_to_value(SZrState *state,
                                                      const SZrIoFunctionConstantVariable *source,
                                                      SZrTypeValue *result) {
    if (state == ZR_NULL || source == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    switch (source->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_TRUE;
        case ZR_VALUE_TYPE_BOOL:
            result->type = ZR_VALUE_TYPE_BOOL;
            result->value.nativeObject.nativeBool = source->value.nativeObject.nativeBool;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            result->type = source->type;
            result->value.nativeObject.nativeInt64 = source->value.nativeObject.nativeInt64;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            result->type = source->type;
            result->value.nativeObject.nativeUInt64 = source->value.nativeObject.nativeUInt64;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            result->type = source->type;
            result->value.nativeObject.nativeDouble = source->value.nativeObject.nativeDouble;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;
        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            if (source->value.object == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(state, result, source->value.object);
            result->type = source->type;
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static SZrCompileTimeFunction *compile_statement_create_imported_compile_time_function_projection(
        SZrCompilerState *cs,
        SZrString *moduleName,
        const SZrIoFunctionCompileTimeFunctionInfo *info,
        SZrFileRange location) {
    SZrCompileTimeFunction *func;

    if (cs == ZR_NULL || moduleName == ZR_NULL || info == ZR_NULL || info->name == ZR_NULL) {
        return ZR_NULL;
    }

    func = (SZrCompileTimeFunction *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                     sizeof(SZrCompileTimeFunction),
                                                                     ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (func == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(func, 0, sizeof(*func));
    ZrCore_Array_Init(cs->state, &func->paramTypes, sizeof(SZrInferredType), info->parameterCount);
    ZrCore_Array_Init(cs->state, &func->paramNames, sizeof(SZrString *), info->parameterCount);
    ZrCore_Array_Init(cs->state, &func->paramHasDefaultValues, sizeof(TZrBool), info->parameterCount);
    ZrCore_Array_Init(cs->state, &func->paramDefaultValues, sizeof(SZrTypeValue), info->parameterCount);
    ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);

    func->name = info->name;
    func->location = location;
    func->runtimeProjectionModuleName = moduleName;
    func->runtimeProjectionExportName = info->name;
    func->isRuntimeProjection = ZR_TRUE;

    compile_statement_io_typed_type_ref_to_inferred(cs, &info->returnType, &func->returnType);
    for (TZrSize index = 0; index < info->parameterCount; index++) {
        SZrInferredType paramType;
        SZrString *paramName = info->parameters != ZR_NULL ? info->parameters[index].name : ZR_NULL;
        TZrBool hasDefaultValue =
                (info->parameters != ZR_NULL && info->parameters[index].hasDefaultValue) ? ZR_TRUE : ZR_FALSE;
        SZrTypeValue defaultValue;

        compile_statement_io_typed_type_ref_to_inferred(cs,
                                                        info->parameters != ZR_NULL ? &info->parameters[index].type
                                                                                    : ZR_NULL,
                                                        &paramType);
        ZrCore_Value_ResetAsNull(&defaultValue);
        if (hasDefaultValue &&
            !compile_statement_io_constant_to_value(cs->state, &info->parameters[index].defaultValue, &defaultValue)) {
            ZrCore_Array_Free(cs->state, &func->paramTypes);
            ZrCore_Array_Free(cs->state, &func->paramNames);
            ZrCore_Array_Free(cs->state, &func->paramHasDefaultValues);
            ZrCore_Array_Free(cs->state, &func->paramDefaultValues);
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          func,
                                          sizeof(SZrCompileTimeFunction),
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }
        ZrCore_Array_Push(cs->state, &func->paramTypes, &paramType);
        ZrCore_Array_Push(cs->state, &func->paramNames, &paramName);
        ZrCore_Array_Push(cs->state, &func->paramHasDefaultValues, &hasDefaultValue);
        ZrCore_Array_Push(cs->state, &func->paramDefaultValues, &defaultValue);
    }

    return func;
}

static TZrBool compile_statement_register_imported_compile_time_function_alias(
        SZrCompilerState *cs,
        SZrString *aliasName,
        const SZrCompileTimeFunction *sourceFunc,
        SZrFileRange location) {
    SZrCompileTimeFunction *func;

    if (cs == ZR_NULL || aliasName == ZR_NULL || sourceFunc == ZR_NULL) {
        return ZR_FALSE;
    }

    if (sourceFunc->declaration != ZR_NULL) {
        return register_compile_time_function_alias(cs, aliasName, sourceFunc->declaration, location);
    }

    func = find_compile_time_function(cs, aliasName);
    if (func == ZR_NULL) {
        func = (SZrCompileTimeFunction *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                         sizeof(SZrCompileTimeFunction),
                                                                         ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (func == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(func, 0, sizeof(*func));
        ZrCore_Array_Init(cs->state, &func->paramTypes, sizeof(SZrInferredType), sourceFunc->paramTypes.length);
        ZrCore_Array_Init(cs->state, &func->paramNames, sizeof(SZrString *), sourceFunc->paramNames.length);
        ZrCore_Array_Init(cs->state,
                          &func->paramHasDefaultValues,
                          sizeof(TZrBool),
                          sourceFunc->paramHasDefaultValues.length);
        ZrCore_Array_Init(cs->state,
                          &func->paramDefaultValues,
                          sizeof(SZrTypeValue),
                          sourceFunc->paramDefaultValues.length);
        ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(cs->state, &cs->compileTimeFunctions, &func);
    } else {
        for (TZrSize index = 0; index < func->paramTypes.length; index++) {
            SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(&func->paramTypes, index);
            if (paramType != ZR_NULL) {
                ZrParser_InferredType_Free(cs->state, paramType);
            }
        }
        func->paramTypes.length = 0;
        func->paramNames.length = 0;
        func->paramHasDefaultValues.length = 0;
        func->paramDefaultValues.length = 0;
        ZrParser_InferredType_Free(cs->state, &func->returnType);
        ZrParser_InferredType_Init(cs->state, &func->returnType, ZR_VALUE_TYPE_OBJECT);
    }

    func->name = aliasName;
    func->declaration = ZR_NULL;
    func->location = location;
    func->runtimeProjectionModuleName = sourceFunc->runtimeProjectionModuleName;
    func->runtimeProjectionExportName = sourceFunc->runtimeProjectionExportName;
    func->isRuntimeProjection = sourceFunc->isRuntimeProjection;
    ZrParser_InferredType_Copy(cs->state, &func->returnType, &sourceFunc->returnType);

    for (TZrSize index = 0; index < sourceFunc->paramTypes.length; index++) {
        SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&sourceFunc->paramTypes, index);
        SZrString **sourceNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&sourceFunc->paramNames, index);
        TZrBool *sourceHasDefaultPtr =
                (TZrBool *)ZrCore_Array_Get((SZrArray *)&sourceFunc->paramHasDefaultValues, index);
        SZrTypeValue *sourceDefaultValue =
                (SZrTypeValue *)ZrCore_Array_Get((SZrArray *)&sourceFunc->paramDefaultValues, index);
        SZrInferredType copiedType;
        SZrString *copiedName = sourceNamePtr != ZR_NULL ? *sourceNamePtr : ZR_NULL;
        TZrBool copiedHasDefault = sourceHasDefaultPtr != ZR_NULL ? *sourceHasDefaultPtr : ZR_FALSE;
        SZrTypeValue copiedDefaultValue;

        ZrParser_InferredType_Init(cs->state, &copiedType, ZR_VALUE_TYPE_OBJECT);
        if (sourceType != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &copiedType);
            ZrParser_InferredType_Copy(cs->state, &copiedType, sourceType);
        }
        ZrCore_Value_ResetAsNull(&copiedDefaultValue);
        if (sourceDefaultValue != ZR_NULL) {
            ZrCore_Value_Copy(cs->state, &copiedDefaultValue, sourceDefaultValue);
        }
        ZrCore_Array_Push(cs->state, &func->paramTypes, &copiedType);
        ZrCore_Array_Push(cs->state, &func->paramNames, &copiedName);
        ZrCore_Array_Push(cs->state, &func->paramHasDefaultValues, &copiedHasDefault);
        ZrCore_Array_Push(cs->state, &func->paramDefaultValues, &copiedDefaultValue);
    }

    if (cs->compileTimeTypeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterFunction(cs->state,
                                                  cs->compileTimeTypeEnv,
                                                  func->name,
                                                  &func->returnType,
                                                  &func->paramTypes);
    }

    return ZR_TRUE;
}

static SZrCompileTimeDecoratorClass *compile_statement_create_imported_compile_time_decorator_class(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrFileRange location) {
    SZrCompileTimeDecoratorClass *decoratorClass;
    SZrAstNodeArray *members = ZR_NULL;
    TZrBool isStructDecorator = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_CLASS_DECLARATION) {
        if (node->data.classDeclaration.name == ZR_NULL || node->data.classDeclaration.name->name == ZR_NULL) {
            return ZR_NULL;
        }
        members = node->data.classDeclaration.members;
    } else if (node->type == ZR_AST_STRUCT_DECLARATION) {
        if (node->data.structDeclaration.name == ZR_NULL || node->data.structDeclaration.name->name == ZR_NULL) {
            return ZR_NULL;
        }
        members = node->data.structDeclaration.members;
        isStructDecorator = ZR_TRUE;
    } else {
        return ZR_NULL;
    }

    decoratorClass = (SZrCompileTimeDecoratorClass *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrCompileTimeDecoratorClass),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (decoratorClass == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(decoratorClass, 0, sizeof(*decoratorClass));
    decoratorClass->name = node->type == ZR_AST_CLASS_DECLARATION ? node->data.classDeclaration.name->name
                                                                  : node->data.structDeclaration.name->name;
    decoratorClass->declaration = node;
    decoratorClass->decorateMethod =
            compile_statement_find_imported_decorator_meta_method(members, "decorate", isStructDecorator);
    decoratorClass->constructorMethod =
            compile_statement_find_imported_decorator_meta_method(members, "constructor", isStructDecorator);
    decoratorClass->isStructDecorator = isStructDecorator;
    decoratorClass->location = location;
    return decoratorClass;
}

static TZrBool compile_statement_collect_imported_compile_time_declarations(SZrCompilerState *cs,
                                                                            SZrImportedCompileTimeModule *module,
                                                                            SZrAstNode *scriptAst) {
    SZrScript *script;
    SZrArray bindingSources;
    SZrImportedCompileTimeVariableBindingContext bindingContext;
    SZrCompileTimeBindingResolver resolver;

    if (cs == ZR_NULL || module == ZR_NULL || scriptAst == ZR_NULL || scriptAst->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    ZrCore_Array_Init(cs->state,
                      &bindingSources,
                      sizeof(SZrCompileTimeBindingSourceVariable),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    script = &scriptAst->data.script;
    if (script->statements == ZR_NULL) {
        ZrCore_Array_Free(cs->state, &bindingSources);
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < script->statements->count; index++) {
        SZrAstNode *statement = script->statements->nodes[index];
        SZrAstNode *declaration;

        if (statement == ZR_NULL || statement->type != ZR_AST_COMPILE_TIME_DECLARATION) {
            continue;
        }

        declaration = statement->data.compileTimeDeclaration.declaration;
        if (declaration == ZR_NULL) {
            continue;
        }

        if (declaration->type == ZR_AST_FUNCTION_DECLARATION) {
            SZrCompileTimeFunction *functionInfo =
                    compile_statement_create_imported_compile_time_function(cs, declaration, statement->location);
            if (functionInfo == ZR_NULL) {
                ZrCore_Array_Free(cs->state, &bindingSources);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &module->compileTimeFunctions, &functionInfo);
        } else if (declaration->type == ZR_AST_CLASS_DECLARATION || declaration->type == ZR_AST_STRUCT_DECLARATION) {
            SZrCompileTimeDecoratorClass *decoratorClass =
                    compile_statement_create_imported_compile_time_decorator_class(cs, declaration, statement->location);
            if (decoratorClass == ZR_NULL) {
                ZrCore_Array_Free(cs->state, &bindingSources);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &module->compileTimeDecoratorClasses, &decoratorClass);
        } else if (declaration->type == ZR_AST_VARIABLE_DECLARATION) {
            SZrFunctionCompileTimeVariableInfo *variableInfo =
                    compile_statement_create_imported_compile_time_variable_info(cs, declaration, statement->location);
            SZrCompileTimeBindingSourceVariable sourceVariable;

            if (variableInfo == ZR_NULL) {
                ZrCore_Array_Free(cs->state, &bindingSources);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(cs->state, &module->compileTimeVariables, &variableInfo);
            ZrCore_Memory_RawSet(&sourceVariable, 0, sizeof(sourceVariable));
            sourceVariable.name = variableInfo->name;
            sourceVariable.value = declaration->data.variableDeclaration.value;
            sourceVariable.info = variableInfo;
            ZrCore_Array_Push(cs->state, &bindingSources, &sourceVariable);
        }
    }

    bindingContext.cs = cs;
    bindingContext.module = module;
    bindingContext.variables = (SZrCompileTimeBindingSourceVariable *)bindingSources.head;
    bindingContext.variableCount = bindingSources.length;
    resolver.state = cs->state;
    resolver.userData = &bindingContext;
    resolver.findVariable = compile_statement_find_imported_binding_source_variable;
    resolver.findFunction = compile_statement_find_imported_compile_time_function_callback;
    resolver.findDecoratorClass = compile_statement_find_imported_compile_time_decorator_class_callback;
    if (bindingSources.length > 0 && !ZrParser_CompileTimeBinding_ResolveAll(
            &resolver,
            (SZrCompileTimeBindingSourceVariable *)bindingSources.head,
            bindingSources.length)) {
        ZrCore_Array_Free(cs->state, &bindingSources);
        return ZR_FALSE;
    }

    ZrCore_Array_Free(cs->state, &bindingSources);
    return ZR_TRUE;
}

static TZrBool compile_statement_collect_imported_compile_time_declarations_from_binary(
        SZrCompilerState *cs,
        SZrImportedCompileTimeModule *module,
        const SZrIoFunction *entryFunction) {
    if (cs == ZR_NULL || module == ZR_NULL || entryFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < entryFunction->compileTimeVariableInfosLength; index++) {
        const SZrIoFunctionCompileTimeVariableInfo *info = &entryFunction->compileTimeVariableInfos[index];
        SZrFunctionCompileTimeVariableInfo *variableInfo =
                compile_statement_create_imported_compile_time_variable_projection(cs, info);
        if (variableInfo == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Array_Push(cs->state, &module->compileTimeVariables, &variableInfo);
    }

    for (TZrSize index = 0; index < entryFunction->compileTimeFunctionInfosLength; index++) {
        const SZrIoFunctionCompileTimeFunctionInfo *info = &entryFunction->compileTimeFunctionInfos[index];
        SZrFileRange location = {{0, 0, info->lineInSourceStart}, {0, 0, info->lineInSourceEnd}, ZR_NULL};
        SZrCompileTimeFunction *functionInfo =
                compile_statement_create_imported_compile_time_function_projection(cs, module->moduleName, info, location);
        if (functionInfo == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Array_Push(cs->state, &module->compileTimeFunctions, &functionInfo);
    }

    return ZR_TRUE;
}

static SZrImportedCompileTimeModule *compile_statement_load_imported_compile_time_module(
        SZrCompilerState *cs,
        SZrString *moduleName) {
    SZrGlobalState *global;
    SZrIo io;
    TZrBytePtr sourceBuffer = ZR_NULL;
    TZrSize sourceSize = 0;
    SZrAstNode *scriptAst = ZR_NULL;
    SZrImportedCompileTimeModule *module = ZR_NULL;
    TZrNativeString moduleNameText;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    module = compile_statement_find_imported_compile_time_module(cs, moduleName);
    if (module != ZR_NULL) {
        return module;
    }

    global = cs->state->global;
    if (global->sourceLoader == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNameText = compile_statement_get_native_string(moduleName);
    if (moduleNameText == ZR_NULL || !global->sourceLoader(cs->state, moduleNameText, ZR_NULL, &io)) {
        return ZR_NULL;
    }

    if (io.isBinary) {
        SZrIoSource *source;

        source = ZrCore_Io_ReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(cs->state, io.customData);
        }

        if (source == ZR_NULL ||
            source->modulesLength == 0 ||
            source->modules == ZR_NULL ||
            source->modules[0].entryFunction == ZR_NULL) {
            return ZR_NULL;
        }

        module = (SZrImportedCompileTimeModule *)ZrCore_Memory_RawMallocWithType(global,
                                                                                  sizeof(SZrImportedCompileTimeModule),
                                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (module == ZR_NULL) {
            return ZR_NULL;
        }

        ZrCore_Memory_RawSet(module, 0, sizeof(*module));
        module->moduleName = moduleName;
        ZrCore_Array_Init(cs->state,
                          &module->compileTimeVariables,
                          sizeof(SZrFunctionCompileTimeVariableInfo *),
                          source->modules[0].entryFunction->compileTimeVariableInfosLength);
        ZrCore_Array_Init(cs->state,
                          &module->compileTimeFunctions,
                          sizeof(SZrCompileTimeFunction *),
                          source->modules[0].entryFunction->compileTimeFunctionInfosLength);
        ZrCore_Array_Init(cs->state,
                          &module->compileTimeDecoratorClasses,
                          sizeof(SZrCompileTimeDecoratorClass *),
                          ZR_PARSER_INITIAL_CAPACITY_TINY);

        if (!compile_statement_collect_imported_compile_time_declarations_from_binary(cs,
                                                                                      module,
                                                                                      source->modules[0].entryFunction)) {
            compile_statement_free_imported_compile_time_variables(cs->state, module);
            if (module->compileTimeFunctions.isValid && module->compileTimeFunctions.head != ZR_NULL) {
                ZrCore_Array_Free(cs->state, &module->compileTimeFunctions);
            }
            if (module->compileTimeDecoratorClasses.isValid && module->compileTimeDecoratorClasses.head != ZR_NULL) {
                ZrCore_Array_Free(cs->state, &module->compileTimeDecoratorClasses);
            }
            ZrCore_Memory_RawFreeWithType(global,
                                          module,
                                          sizeof(SZrImportedCompileTimeModule),
                                          ZR_MEMORY_NATIVE_TYPE_ARRAY);
            return ZR_NULL;
        }

        ZrCore_Array_Push(cs->state, &cs->importedCompileTimeModules, &module);
        return module;
    }

    sourceBuffer = compile_statement_read_all_from_io(cs->state, &io, &sourceSize);
    if (io.close != ZR_NULL) {
        io.close(cs->state, io.customData);
    }
    if (sourceBuffer == ZR_NULL || sourceSize == 0) {
        if (sourceBuffer != ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          sourceBuffer,
                                          sourceSize + 1,
                                          ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        }
        return ZR_NULL;
    }

    scriptAst = ZrParser_Parse(cs->state, (const TZrChar *)sourceBuffer, sourceSize, moduleName);
    ZrCore_Memory_RawFreeWithType(global,
                                  sourceBuffer,
                                  sourceSize + 1,
                                  ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (scriptAst == ZR_NULL) {
        return ZR_NULL;
    }

    module = (SZrImportedCompileTimeModule *)ZrCore_Memory_RawMallocWithType(global,
                                                                              sizeof(SZrImportedCompileTimeModule),
                                                                              ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (module == ZR_NULL) {
        ZrParser_Ast_Free(cs->state, scriptAst);
        return ZR_NULL;
    }

    ZrCore_Memory_RawSet(module, 0, sizeof(*module));
    module->moduleName = moduleName;
    module->scriptAst = scriptAst;
    ZrCore_Array_Init(cs->state,
                      &module->compileTimeVariables,
                      sizeof(SZrFunctionCompileTimeVariableInfo *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state,
                      &module->compileTimeFunctions,
                      sizeof(SZrCompileTimeFunction *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(cs->state,
                      &module->compileTimeDecoratorClasses,
                      sizeof(SZrCompileTimeDecoratorClass *),
                      ZR_PARSER_INITIAL_CAPACITY_TINY);

    if (!compile_statement_collect_imported_compile_time_declarations(cs, module, scriptAst)) {
        compile_statement_free_imported_compile_time_variables(cs->state, module);
        if (module->compileTimeFunctions.isValid && module->compileTimeFunctions.head != ZR_NULL) {
            ZrCore_Array_Free(cs->state, &module->compileTimeFunctions);
        }
        if (module->compileTimeDecoratorClasses.isValid && module->compileTimeDecoratorClasses.head != ZR_NULL) {
            ZrCore_Array_Free(cs->state, &module->compileTimeDecoratorClasses);
        }
        ZrParser_Ast_Free(cs->state, scriptAst);
        ZrCore_Memory_RawFreeWithType(global,
                                      module,
                                      sizeof(SZrImportedCompileTimeModule),
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
        return ZR_NULL;
    }

    ZrCore_Array_Push(cs->state, &cs->importedCompileTimeModules, &module);
    return module;
}

static TZrBool compile_statement_try_register_imported_compile_time_module_alias(SZrCompilerState *cs,
                                                                                 SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrString *aliasName;
    SZrString *moduleName;
    SZrImportedCompileTimeModule *module;
    SZrImportedCompileTimeModuleAlias alias;
    TZrBool updated = ZR_FALSE;

    if (cs == ZR_NULL || node == ZR_NULL || !cs->isScriptLevel || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_TRUE;
    }

    decl = &node->data.variableDeclaration;
    if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL || decl->value == ZR_NULL ||
        decl->value->type != ZR_AST_IMPORT_EXPRESSION ||
        decl->value->data.importExpression.modulePath == ZR_NULL ||
        decl->value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
        decl->value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_TRUE;
    }

    aliasName = decl->pattern->data.identifier.name;
    moduleName = decl->value->data.importExpression.modulePath->data.stringLiteral.value;
    if (aliasName == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_TRUE;
    }

    module = compile_statement_load_imported_compile_time_module(cs, moduleName);
    if (module == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < cs->importedCompileTimeModuleAliases.length; index++) {
        SZrImportedCompileTimeModuleAlias *existing =
                (SZrImportedCompileTimeModuleAlias *)ZrCore_Array_Get(&cs->importedCompileTimeModuleAliases, index);
        if (existing != ZR_NULL && existing->aliasName != ZR_NULL &&
            ZrCore_String_Equal(existing->aliasName, aliasName)) {
            existing->module = module;
            updated = ZR_TRUE;
            break;
        }
    }

    if (!updated) {
        alias.aliasName = aliasName;
        alias.module = module;
        ZrCore_Array_Push(cs->state, &cs->importedCompileTimeModuleAliases, &alias);
    }

    for (TZrSize index = 0; index < module->compileTimeDecoratorClasses.length; index++) {
        SZrCompileTimeDecoratorClass **classPtr =
                (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get(&module->compileTimeDecoratorClasses, index);
        if (classPtr == ZR_NULL || *classPtr == ZR_NULL || (*classPtr)->name == ZR_NULL) {
            continue;
        }
        if (!register_compile_time_decorator_class_alias(cs,
                                                         (*classPtr)->name,
                                                         (*classPtr)->declaration,
                                                         node->location)) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; index < module->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&module->compileTimeFunctions, index);
        if (funcPtr == ZR_NULL || *funcPtr == ZR_NULL || (*funcPtr)->name == ZR_NULL) {
            continue;
        }
        if (!compile_statement_register_imported_compile_time_function_alias(cs,
                                                                             (*funcPtr)->name,
                                                                             *funcPtr,
                                                                             node->location)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool compile_statement_try_register_imported_compile_time_member_alias(SZrCompilerState *cs,
                                                                                 SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrPrimaryExpression *primary;
    SZrString *aliasName;
    SZrString *moduleAliasName;
    SZrString *memberName = ZR_NULL;
    SZrString *relativePath = ZR_NULL;
    SZrImportedCompileTimeModule *module;
    SZrCompileTimeDecoratorClass *decoratorClass;
    SZrCompileTimeFunction *functionInfo;
    SZrFunctionCompileTimeVariableInfo *variableInfo;
    const SZrFunctionCompileTimePathBinding *bindingInfo = ZR_NULL;
    TZrSize memberCount;

    if (cs == ZR_NULL || node == ZR_NULL || !cs->isScriptLevel || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_TRUE;
    }

    decl = &node->data.variableDeclaration;
    if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL || decl->value == ZR_NULL ||
        decl->value->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_TRUE;
    }

    primary = &decl->value->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->property->data.identifier.name == ZR_NULL || primary->members == ZR_NULL || primary->members->count == 0) {
        return ZR_TRUE;
    }

    memberCount = primary->members->count;
    if (!ZrParser_CompileTimeBinding_BuildStaticMemberPath(cs->state, primary->members, 0, 1, &memberName)) {
        return ZR_TRUE;
    }

    aliasName = decl->pattern->data.identifier.name;
    moduleAliasName = primary->property->data.identifier.name;
    if (aliasName == ZR_NULL || moduleAliasName == ZR_NULL || memberName == ZR_NULL) {
        return ZR_TRUE;
    }

    module = compile_statement_find_imported_compile_time_module_alias(cs, moduleAliasName);
    if (module == ZR_NULL) {
        return ZR_TRUE;
    }

    decoratorClass = compile_statement_find_imported_compile_time_decorator_class(module, memberName);
    if (decoratorClass != ZR_NULL) {
        return register_compile_time_decorator_class_alias(cs, aliasName, decoratorClass->declaration, node->location);
    }

    functionInfo = compile_statement_find_imported_compile_time_function(module, memberName);
    if (functionInfo != ZR_NULL) {
        return compile_statement_register_imported_compile_time_function_alias(cs,
                                                                              aliasName,
                                                                              functionInfo,
                                                                              node->location);
    }

    variableInfo = compile_statement_find_imported_compile_time_variable(module, memberName);
    if (variableInfo == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrParser_CompileTimeBinding_BuildStaticMemberPath(cs->state,
                                                           primary->members,
                                                           1,
                                                           memberCount,
                                                           &relativePath)) {
        return ZR_TRUE;
    }

    bindingInfo = ZrParser_CompileTimeBinding_FindPath(variableInfo, relativePath);
    if (bindingInfo == ZR_NULL || bindingInfo->targetName == ZR_NULL) {
        return ZR_TRUE;
    }

    if (bindingInfo->targetKind == ZR_COMPILE_TIME_BINDING_TARGET_DECORATOR_CLASS) {
        decoratorClass = compile_statement_find_imported_compile_time_decorator_class(module, bindingInfo->targetName);
        return decoratorClass != ZR_NULL
                       ? register_compile_time_decorator_class_alias(cs,
                                                                     aliasName,
                                                                     decoratorClass->declaration,
                                                                     node->location)
                       : ZR_TRUE;
    }

    if (bindingInfo->targetKind == ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION) {
        functionInfo = compile_statement_find_imported_compile_time_function(module, bindingInfo->targetName);
        return functionInfo != ZR_NULL
                       ? compile_statement_register_imported_compile_time_function_alias(cs,
                                                                                         aliasName,
                                                                                         functionInfo,
                                                                                         node->location)
                       : ZR_TRUE;
    }

    return ZR_TRUE;
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
                         create_instruction_2(ZR_INSTRUCTION_ENUM(SET_BY_INDEX),
                                              (TZrUInt16)valueSlot,
                                              (TZrUInt16)arraySlot,
                                              (TZrUInt16)indexSlot));
        ZrParser_Compiler_TrimStackBy(cs, 2);
    }

}

static TZrTypeId resolve_using_resource_type_id(SZrCompilerState *cs, SZrAstNode *resource) {
    SZrInferredType inferredType;
    TZrBool hasInferredType = ZR_FALSE;
    TZrTypeId typeId = ZR_SEMANTIC_ID_INVALID;
    EZrSemanticTypeKind semanticKind = ZR_SEMANTIC_TYPE_KIND_REFERENCE;

    if (cs == ZR_NULL || resource == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
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
        return ZR_SEMANTIC_ID_INVALID;
    }

    typeId = resolve_using_resource_type_id(cs, resource);
    return ZrParser_Semantic_RegisterSymbol(cs->semanticContext,
                                    resource->data.identifier.name,
                                    ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                    typeId,
                                    ZR_SEMANTIC_ID_INVALID,
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
    TZrChar buffer[ZR_PARSER_GENERATED_NAME_BUFFER_LENGTH];
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
        if (existingLocalSlot != ZR_PARSER_SLOT_NONE) {
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

    compile_statement_trace("compile_variable_declaration enter node=%p patternType=%d valueType=%d",
                            (void *)node,
                            node->data.variableDeclaration.pattern != ZR_NULL
                                    ? (int)node->data.variableDeclaration.pattern->type
                                    : -1,
                            node->data.variableDeclaration.value != ZR_NULL
                                    ? (int)node->data.variableDeclaration.value->type
                                    : -1);
    
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
        SZrInferredType initializerType;
        TZrBool resolvedTypeInitialized = ZR_FALSE;
        TZrBool initializerTypeInitialized = ZR_FALSE;
        TZrBool hasResolvedType = ZR_FALSE;
        if (varName == ZR_NULL) {
            ZrParser_Compiler_Error(cs, "Variable name is null", node->location);
            return;
        }

        compile_statement_trace("var decl identifier name=%p typeInfo=%p value=%p",
                                (void *)varName,
                                (void *)decl->typeInfo,
                                (void *)decl->value);

        if (decl->typeInfo != ZR_NULL) {
            compile_statement_trace("var decl infer from explicit type start");
            ZrParser_InferredType_Init(cs->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            resolvedTypeInitialized = ZR_TRUE;
            hasResolvedType = ZrParser_AstTypeToInferredType_Convert(cs, decl->typeInfo, &resolvedType);
            compile_statement_trace("var decl infer from explicit type done success=%d baseType=%d typeName=%p",
                                    (int)hasResolvedType,
                                    resolvedTypeInitialized ? (int)resolvedType.baseType : -1,
                                    resolvedTypeInitialized ? (void *)resolvedType.typeName : ZR_NULL);
        } else if (decl->value != ZR_NULL) {
            compile_statement_trace("var decl infer from initializer start valueType=%d", (int)decl->value->type);
            ZrParser_InferredType_Init(cs->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            resolvedTypeInitialized = ZR_TRUE;
            hasResolvedType = ZrParser_ExpressionType_Infer(cs, decl->value, &resolvedType);
            compile_statement_trace("var decl infer from initializer done success=%d baseType=%d typeName=%p hasError=%d",
                                    (int)hasResolvedType,
                                    resolvedTypeInitialized ? (int)resolvedType.baseType : -1,
                                    resolvedTypeInitialized ? (void *)resolvedType.typeName : ZR_NULL,
                                    (int)cs->hasError);
        }

        if (cs->hasError) {
            if (resolvedTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &resolvedType);
            }
            return;
        }

        if (decl->typeInfo != ZR_NULL && decl->value != ZR_NULL && hasResolvedType) {
            compile_statement_trace("var decl compatibility check start");
            ZrParser_InferredType_Init(cs->state, &initializerType, ZR_VALUE_TYPE_OBJECT);
            initializerTypeInitialized = ZR_TRUE;
            if (!ZrParser_ExpressionType_Infer(cs, decl->value, &initializerType) ||
                !ZrParser_AssignmentCompatibility_Check(cs, &resolvedType, &initializerType, decl->value->location)) {
                ZrParser_InferredType_Free(cs->state, &initializerType);
                if (resolvedTypeInitialized) {
                    ZrParser_InferredType_Free(cs->state, &resolvedType);
                }
                return;
            }
            compile_statement_trace("var decl compatibility check done baseType=%d typeName=%p",
                                    (int)initializerType.baseType,
                                    (void *)initializerType.typeName);
        }

        TZrUInt32 varIndex = 0;

        if (decl->isConst && decl->value == ZR_NULL) {
            TZrNativeString varNameText = ZrCore_String_GetNativeStringShort(varName);
            TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];

            if (varNameText != ZR_NULL) {
                snprintf(errorMsg,
                         sizeof(errorMsg),
                         "Const variable '%s' must be initialized at declaration",
                         varNameText);
            } else {
                snprintf(errorMsg, sizeof(errorMsg), "Const variable must be initialized at declaration");
            }
            ZrParser_Compiler_Error(cs, errorMsg, node->location);
            if (initializerTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &initializerType);
            }
            if (resolvedTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &resolvedType);
            }
            return;
        }

        // 如果有初始值，编译初始值表达式
        if (decl->value != ZR_NULL) {
            TZrUInt32 reservedVarSlot = allocate_stack_slot(cs);
            TZrUInt32 initializerSlot;
            TZrUInt32 activateOffset;

            compile_statement_trace("var decl initializer compile start reservedSlot=%u stackCount=%llu",
                                    (unsigned int)reservedVarSlot,
                                    (unsigned long long)cs->stackSlotCount);

            if (reservedVarSlot == ZR_PARSER_SLOT_NONE) {
                if (initializerTypeInitialized) {
                    ZrParser_InferredType_Free(cs->state, &initializerType);
                }
                if (resolvedTypeInitialized) {
                    ZrParser_InferredType_Free(cs->state, &resolvedType);
                }
                return;
            }

            ZrParser_Expression_Compile(cs, decl->value);
            compile_statement_trace("var decl initializer compile done hasError=%d stackCount=%llu",
                                    (int)cs->hasError,
                                    (unsigned long long)cs->stackSlotCount);
            if (cs->hasError || cs->stackSlotCount == 0) {
                if (initializerTypeInitialized) {
                    ZrParser_InferredType_Free(cs->state, &initializerType);
                }
                if (resolvedTypeInitialized) {
                    ZrParser_InferredType_Free(cs->state, &resolvedType);
                }
                return;
            }

            initializerSlot = (TZrUInt32)(cs->stackSlotCount - 1);
            compile_statement_trace("var decl bind initializer slot=%u reserved=%u",
                                    (unsigned int)initializerSlot,
                                    (unsigned int)reservedVarSlot);
            activateOffset = (TZrUInt32)cs->instructionCount;
            if (initializerSlot != reservedVarSlot) {
                emit_instruction(cs,
                                 create_instruction_1(ZR_INSTRUCTION_ENUM(SET_STACK),
                                                      (TZrUInt16)reservedVarSlot,
                                                      (TZrInt32)initializerSlot));
            }
            ZrParser_Compiler_TrimStackToSlot(cs, reservedVarSlot);
            varIndex = bind_existing_stack_slot_as_local_var(cs, varName, reservedVarSlot, activateOffset);
            compile_statement_trace("var decl local binding done varIndex=%u", (unsigned int)varIndex);
        } else {
            TZrSize fixedArraySize;
            varIndex = allocate_local_var(cs, varName);
            compile_statement_trace("var decl allocate local no initializer varIndex=%u", (unsigned int)varIndex);

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

        if (decl->value != ZR_NULL) {
            compiler_register_callable_value_binding(cs, varName, decl->value);
            compile_statement_register_type_value_alias(cs, varName, decl->value);
            compile_statement_trace("var decl runtime alias registration done");
        }

        if (!compile_statement_try_register_imported_compile_time_module_alias(cs, node) ||
            !compile_statement_try_register_imported_compile_time_member_alias(cs, node)) {
            if (resolvedTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &resolvedType);
            }
            if (initializerTypeInitialized) {
                ZrParser_InferredType_Free(cs->state, &initializerType);
            }
            return;
        }
        compile_statement_trace("var decl compile-time import alias registration done");

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
            exportedVar.exportKind = ZR_MODULE_EXPORT_KIND_VALUE;
            exportedVar.readiness = ZR_MODULE_EXPORT_READY_ENTRY;
            exportedVar.callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;

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
        if (initializerTypeInitialized) {
            ZrParser_InferredType_Free(cs->state, &initializerType);
        }
        compile_statement_trace("compile_variable_declaration exit success");
    } else if (decl->pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        // 处理解构对象赋值：var {key1, key2, ...} = value;
        if (!compile_statement_try_register_imported_destructured_type_aliases(cs, node)) {
            return;
        }
        compile_destructuring_object(cs, decl->pattern, decl->value);
        if (cs->hasError) {
            return;
        }
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
                (lastInst->instruction.operationCode == ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL) ||
                 lastInst->instruction.operationCode == ZR_INSTRUCTION_ENUM(DYN_TAIL_CALL) ||
                 lastInst->instruction.operationCode == ZR_INSTRUCTION_ENUM(META_TAIL_CALL))) {
                resultSlot = lastInst->instruction.operandExtra;
            } else {
                resultSlot = (TZrUInt32)(cs->stackSlotCount - 1);
            }
        } else {
            resultSlot = (TZrUInt32)(cs->stackSlotCount - 1);
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
    TZrLifetimeRegionId regionId = ZR_SEMANTIC_ID_INVALID;
    TZrSymbolId symbolId = ZR_SEMANTIC_ID_INVALID;
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
    TZrUInt32 condSlot = (TZrUInt32)(cs->stackSlotCount - 1);
    
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
    compile_statement_trace("statement compile dispatch node=%p type=%d", (void *)node, (int)node->type);
    
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
                static TZrChar errorMsg[ZR_PARSER_ERROR_BUFFER_LENGTH];
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
    compile_statement_trace("statement compile dispatch done node=%p type=%d hasError=%d",
                            (void *)node,
                            (int)node->type,
                            (int)cs->hasError);
}

static TZrBool compile_statement_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void compile_statement_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!compile_statement_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-compile-stmt] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}

// 编译解构对象赋值：var {key1, key2, ...} = value;
