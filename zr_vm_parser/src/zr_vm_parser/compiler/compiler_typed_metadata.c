//
// Strongly-typed function metadata builder for compiled script entry functions.
//

#include "compiler_internal.h"
#include "type_inference_internal.h"
#include "compile_time_executor_internal.h"
#include "compile_time_binding_metadata.h"
#include "zr_vm_core/stack.h"

static void typed_type_ref_init_unknown(SZrFunctionTypedTypeRef *typeRef) {
    if (typeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(typeRef, 0, sizeof(*typeRef));
    typeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    typeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
}

static void typed_type_ref_from_inferred(SZrFunctionTypedTypeRef *dest, const SZrInferredType *src) {
    if (dest == ZR_NULL) {
        return;
    }

    typed_type_ref_init_unknown(dest);
    if (src == ZR_NULL) {
        return;
    }

    dest->baseType = src->baseType;
    dest->isNullable = src->isNullable;
    dest->ownershipQualifier = src->ownershipQualifier;
    dest->typeName = src->typeName;
    if (src->baseType == ZR_VALUE_TYPE_ARRAY) {
        dest->isArray = ZR_TRUE;
        if (src->elementTypes.length > 0) {
            const SZrInferredType *elementType =
                    (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&src->elementTypes, 0);
            if (elementType != ZR_NULL) {
                dest->elementBaseType = elementType->baseType;
                dest->elementTypeName = elementType->typeName;
            }
        }
    }
}

static TZrBool typed_type_ref_from_ast_type(SZrCompilerState *cs,
                                            SZrType *typeNode,
                                            SZrFunctionTypedTypeRef *outType) {
    SZrInferredType inferredType;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (typeNode != ZR_NULL) {
        success = ZrParser_AstTypeToInferredType_Convert(cs, typeNode, &inferredType);
    } else {
        success = ZR_TRUE;
    }

    if (!success) {
        ZrParser_InferredType_Free(cs->state, &inferredType);
        return ZR_FALSE;
    }

    typed_type_ref_from_inferred(outType, &inferredType);
    ZrParser_InferredType_Free(cs->state, &inferredType);
    return ZR_TRUE;
}

static SZrAstNodeArray *typed_metadata_current_parameter_list(SZrCompilerState *cs) {
    SZrAstNode *node;

    if (cs == ZR_NULL || cs->currentFunctionNode == ZR_NULL) {
        return ZR_NULL;
    }

    node = cs->currentFunctionNode;
    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return node->data.functionDeclaration.params;
        case ZR_AST_TEST_DECLARATION:
            return node->data.testDeclaration.params;
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.params;
        case ZR_AST_STRUCT_META_FUNCTION:
            return node->data.structMetaFunction.params;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.params;
        case ZR_AST_CLASS_META_FUNCTION:
            return node->data.classMetaFunction.params;
        case ZR_AST_LAMBDA_EXPRESSION:
            return node->data.lambdaExpression.params;
        default:
            return ZR_NULL;
    }
}

static const SZrParameter *typed_metadata_find_parameter_by_name(const SZrAstNodeArray *params,
                                                                 const SZrString *name) {
    if (params == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        const SZrAstNode *paramNode = params->nodes[index];
        if (paramNode == ZR_NULL ||
            paramNode->type != ZR_AST_PARAMETER ||
            paramNode->data.parameter.name == ZR_NULL ||
            paramNode->data.parameter.name->name == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(paramNode->data.parameter.name->name, (SZrString *)name)) {
            return &paramNode->data.parameter;
        }
    }

    return ZR_NULL;
}

static TZrBool typed_type_ref_from_current_parameter(SZrCompilerState *cs,
                                                     SZrString *name,
                                                     SZrFunctionTypedTypeRef *outType) {
    const SZrParameter *parameter;

    if (cs == ZR_NULL || name == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    parameter = typed_metadata_find_parameter_by_name(typed_metadata_current_parameter_list(cs), name);
    if (parameter == ZR_NULL) {
        return ZR_FALSE;
    }

    return typed_type_ref_from_ast_type(cs, parameter->typeInfo, outType);
}

static TZrBool typed_type_ref_from_injected_this(SZrCompilerState *cs,
                                                 SZrString *name,
                                                 SZrFunctionTypedTypeRef *outType) {
    const TZrChar *nameText;

    if (cs == ZR_NULL || name == ZR_NULL || outType == ZR_NULL || cs->currentTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = ZrCore_String_GetNativeStringShort(name);
    if (nameText == ZR_NULL || strcmp(nameText, "this") != 0) {
        return ZR_FALSE;
    }

    typed_type_ref_init_unknown(outType);
    outType->baseType = ZR_VALUE_TYPE_OBJECT;
    outType->typeName = cs->currentTypeName;
    outType->ownershipQualifier =
            get_implicit_this_ownership_qualifier(get_member_receiver_qualifier(cs->currentFunctionNode));
    return ZR_TRUE;
}

static SZrFunctionTypeInfo *find_callable_binding_info(SZrCompilerState *cs, SZrString *name) {
    SZrFunctionTypeInfo *functionInfo = ZR_NULL;

    if (cs == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (ZrParser_TypeEnvironment_LookupFunction(cs->typeEnv, name, &functionInfo)) {
        return functionInfo;
    }

    return ZR_NULL;
}

static TZrBool typed_type_ref_from_callable_binding(SZrCompilerState *cs,
                                                   SZrString *name,
                                                   SZrFunctionTypedTypeRef *outType) {
    SZrFunctionTypeInfo *functionInfo;

    if (cs == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    functionInfo = find_callable_binding_info(cs, name);
    if (functionInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    typed_type_ref_from_inferred(outType, &functionInfo->returnType);
    return ZR_TRUE;
}

static void typed_type_ref_from_type_name(SZrCompilerState *cs,
                                          SZrString *typeName,
                                          SZrFunctionTypedTypeRef *outType) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || outType == ZR_NULL) {
        return;
    }

    typed_type_ref_init_unknown(outType);
    if (typeName == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (inferred_type_from_type_name(cs, typeName, &inferredType)) {
        typed_type_ref_from_inferred(outType, &inferredType);
    } else {
        outType->typeName = typeName;
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

typedef struct SZrCompileTimeVariableBindingBuildContext {
    SZrCompilerState *cs;
    SZrCompileTimeBindingSourceVariable *variables;
    TZrSize variableCount;
} SZrCompileTimeVariableBindingBuildContext;

static SZrCompileTimeBindingSourceVariable *find_compile_time_binding_variable_source(
        TZrPtr userData,
        SZrString *name) {
    SZrCompileTimeVariableBindingBuildContext *context =
            (SZrCompileTimeVariableBindingBuildContext *)userData;

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

static SZrCompileTimeFunction *find_compile_time_binding_function(TZrPtr userData, SZrString *name) {
    SZrCompileTimeVariableBindingBuildContext *context =
            (SZrCompileTimeVariableBindingBuildContext *)userData;

    if (context == ZR_NULL || context->cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->cs->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&context->cs->compileTimeFunctions, index);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return *funcPtr;
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeDecoratorClass *find_compile_time_binding_decorator_class(TZrPtr userData, SZrString *name) {
    SZrCompileTimeVariableBindingBuildContext *context =
            (SZrCompileTimeVariableBindingBuildContext *)userData;

    if (context == ZR_NULL || context->cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < context->cs->compileTimeDecoratorClasses.length; index++) {
        SZrCompileTimeDecoratorClass **classPtr =
                (SZrCompileTimeDecoratorClass **)ZrCore_Array_Get(&context->cs->compileTimeDecoratorClasses, index);
        if (classPtr != ZR_NULL && *classPtr != ZR_NULL && (*classPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*classPtr)->name, name)) {
            return *classPtr;
        }
    }

    return ZR_NULL;
}

static void typed_export_symbol_set_declaration_range(SZrFunctionTypedExportSymbol *symbol, SZrFileRange location) {
    if (symbol == ZR_NULL) {
        return;
    }

    symbol->lineInSourceStart = location.start.line > 0 ? (TZrUInt32)location.start.line : 0;
    symbol->columnInSourceStart = location.start.column > 0 ? (TZrUInt32)location.start.column : 0;
    symbol->lineInSourceEnd = location.end.line > 0 ? (TZrUInt32)location.end.line : 0;
    symbol->columnInSourceEnd = location.end.column > 0 ? (TZrUInt32)location.end.column : 0;
}

static void typed_export_symbol_set_declaration_from_function(SZrFunctionTypedExportSymbol *symbol,
                                                              SZrAstNode *functionDeclNode) {
    SZrFunctionDeclaration *declaration;

    if (symbol == ZR_NULL || functionDeclNode == ZR_NULL || functionDeclNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return;
    }

    declaration = &functionDeclNode->data.functionDeclaration;
    if (declaration->name != ZR_NULL) {
        typed_export_symbol_set_declaration_range(symbol, declaration->nameLocation);
    } else {
        typed_export_symbol_set_declaration_range(symbol, functionDeclNode->location);
    }
}

static void typed_export_symbol_set_declaration_from_variable(SZrFunctionTypedExportSymbol *symbol,
                                                              SZrAstNode *variableDeclNode) {
    SZrVariableDeclaration *declaration;

    if (symbol == ZR_NULL || variableDeclNode == ZR_NULL || variableDeclNode->type != ZR_AST_VARIABLE_DECLARATION) {
        return;
    }

    declaration = &variableDeclNode->data.variableDeclaration;
    if (declaration->pattern != ZR_NULL) {
        typed_export_symbol_set_declaration_range(symbol, declaration->pattern->location);
    } else {
        typed_export_symbol_set_declaration_range(symbol, variableDeclNode->location);
    }
}

static SZrAstNode *find_script_function_declaration_by_name(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || name == ZR_NULL || cs->scriptAst->type != ZR_AST_SCRIPT ||
        cs->scriptAst->data.script.statements == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->scriptAst->data.script.statements->count; index++) {
        SZrAstNode *statement = cs->scriptAst->data.script.statements->nodes[index];
        SZrFunctionDeclaration *declaration;

        if (statement == ZR_NULL || statement->type != ZR_AST_FUNCTION_DECLARATION) {
            continue;
        }

        declaration = &statement->data.functionDeclaration;
        if (declaration->name != ZR_NULL && declaration->name->name != ZR_NULL &&
            ZrCore_String_Equal(declaration->name->name, name)) {
            return statement;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *find_script_function_declaration_for_export(SZrCompilerState *cs,
                                                               const SZrExportedVariable *exportedVar) {
    if (cs == ZR_NULL || exportedVar == ZR_NULL) {
        return ZR_NULL;
    }

    if (exportedVar->callableChildIndex != ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE &&
        exportedVar->callableChildIndex < cs->childFunctions.length) {
        SZrFunction **childFunctionPtr =
                (SZrFunction **)ZrCore_Array_Get(&cs->childFunctions, exportedVar->callableChildIndex);
        if (childFunctionPtr != ZR_NULL &&
            *childFunctionPtr != ZR_NULL &&
            (*childFunctionPtr)->functionName != ZR_NULL) {
            SZrAstNode *candidate = ZR_NULL;
            TZrUInt32 childLineStart = (*childFunctionPtr)->lineInSourceStart;
            TZrUInt32 childLineEnd = (*childFunctionPtr)->lineInSourceEnd;

            for (TZrSize index = 0;
                 cs->scriptAst != ZR_NULL &&
                 cs->scriptAst->type == ZR_AST_SCRIPT &&
                 cs->scriptAst->data.script.statements != ZR_NULL &&
                 index < cs->scriptAst->data.script.statements->count;
                 index++) {
                SZrAstNode *statement = cs->scriptAst->data.script.statements->nodes[index];
                SZrFunctionDeclaration *declaration;

                if (statement == ZR_NULL || statement->type != ZR_AST_FUNCTION_DECLARATION) {
                    continue;
                }

                declaration = &statement->data.functionDeclaration;
                if (declaration->name == ZR_NULL ||
                    declaration->name->name == ZR_NULL ||
                    !ZrCore_String_Equal(declaration->name->name, (*childFunctionPtr)->functionName)) {
                    continue;
                }

                if ((childLineStart == 0 ||
                     (TZrUInt32)statement->location.start.line == childLineStart) &&
                    (childLineEnd == 0 ||
                     (TZrUInt32)statement->location.end.line == childLineEnd)) {
                    return statement;
                }
                if (candidate == ZR_NULL) {
                    candidate = statement;
                }
            }

            if (candidate != ZR_NULL) {
                return candidate;
            }
        }
    }

    return find_script_function_declaration_by_name(cs, exportedVar->name);
}

static SZrAstNode *find_script_variable_declaration_by_name(SZrCompilerState *cs, SZrString *name) {
    if (cs == ZR_NULL || cs->scriptAst == ZR_NULL || name == ZR_NULL || cs->scriptAst->type != ZR_AST_SCRIPT ||
        cs->scriptAst->data.script.statements == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->scriptAst->data.script.statements->count; index++) {
        SZrAstNode *statement = cs->scriptAst->data.script.statements->nodes[index];
        SZrVariableDeclaration *declaration;

        if (statement == ZR_NULL || statement->type != ZR_AST_VARIABLE_DECLARATION) {
            continue;
        }

        declaration = &statement->data.variableDeclaration;
        if (declaration->pattern != ZR_NULL &&
            declaration->pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
            declaration->pattern->data.identifier.name != ZR_NULL &&
            ZrCore_String_Equal(declaration->pattern->data.identifier.name, name)) {
            return statement;
        }
    }

    return ZR_NULL;
}

static void free_typed_export_symbols(SZrState *state,
                                      SZrFunctionTypedExportSymbol *symbols,
                                      TZrUInt32 count) {
    if (state == ZR_NULL || state->global == ZR_NULL || symbols == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < count; index++) {
        SZrFunctionTypedExportSymbol *symbol = &symbols[index];
        if (symbol->parameterTypes != ZR_NULL && symbol->parameterCount > 0) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          symbol->parameterTypes,
                                          sizeof(SZrFunctionTypedTypeRef) * symbol->parameterCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  symbols,
                                  sizeof(SZrFunctionTypedExportSymbol) * count,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

static void free_metadata_parameters(SZrState *state,
                                     SZrFunctionMetadataParameter *parameters,
                                     TZrUInt32 parameterCount) {
    if (state == ZR_NULL || state->global == ZR_NULL || parameters == ZR_NULL || parameterCount == 0) {
        return;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        if (parameters[index].decoratorNames != ZR_NULL && parameters[index].decoratorCount > 0) {
            ZrCore_Memory_RawFreeWithType(state->global,
                                          parameters[index].decoratorNames,
                                          sizeof(SZrString *) * parameters[index].decoratorCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  parameters,
                                  sizeof(SZrFunctionMetadataParameter) * parameterCount,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

static void free_compile_time_function_infos(SZrState *state,
                                             SZrFunctionCompileTimeFunctionInfo *infos,
                                             TZrUInt32 count) {
    if (state == ZR_NULL || state->global == ZR_NULL || infos == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < count; index++) {
        free_metadata_parameters(state, infos[index].parameters, infos[index].parameterCount);
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  infos,
                                  sizeof(SZrFunctionCompileTimeFunctionInfo) * count,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

static void free_test_infos(SZrState *state,
                            SZrFunctionTestInfo *infos,
                            TZrUInt32 count) {
    if (state == ZR_NULL || state->global == ZR_NULL || infos == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < count; index++) {
        free_metadata_parameters(state, infos[index].parameters, infos[index].parameterCount);
    }

    ZrCore_Memory_RawFreeWithType(state->global,
                                  infos,
                                  sizeof(SZrFunctionTestInfo) * count,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
}

TZrBool compiler_build_function_parameter_metadata(SZrCompilerState *cs,
                                                   SZrAstNodeArray *params,
                                                   TZrBool includeDefaultValues,
                                                   SZrFunctionMetadataParameter **outParameters,
                                                   TZrUInt32 *outParameterCount) {
    TZrUInt32 parameterCount;
    SZrFunctionMetadataParameter *parameters;

    if (outParameters == ZR_NULL || outParameterCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outParameters = ZR_NULL;
    *outParameterCount = 0;
    if (cs == ZR_NULL || params == ZR_NULL || params->count == 0) {
        return ZR_TRUE;
    }

    parameterCount = (TZrUInt32)params->count;
    parameters = (SZrFunctionMetadataParameter *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionMetadataParameter) * parameterCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (parameters == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(parameters, 0, sizeof(SZrFunctionMetadataParameter) * parameterCount);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        SZrAstNode *paramNode = params->nodes[index];

        typed_type_ref_init_unknown(&parameters[index].type);
        parameters[index].hasDefaultValue = ZR_FALSE;
        ZrCore_Value_ResetAsNull(&parameters[index].defaultValue);
        parameters[index].hasDecoratorMetadata = ZR_FALSE;
        ZrCore_Value_ResetAsNull(&parameters[index].decoratorMetadataValue);
        parameters[index].decoratorNames = ZR_NULL;
        parameters[index].decoratorCount = 0;
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameters[index].name =
                paramNode->data.parameter.name != ZR_NULL ? paramNode->data.parameter.name->name : ZR_NULL;
        if (!typed_type_ref_from_ast_type(cs, paramNode->data.parameter.typeInfo, &parameters[index].type)) {
            free_metadata_parameters(cs->state, parameters, parameterCount);
            return ZR_FALSE;
        }

        if (includeDefaultValues && paramNode->data.parameter.defaultValue != ZR_NULL) {
            if (!ZrParser_Compiler_EvaluateCompileTimeExpression(cs,
                                                                 paramNode->data.parameter.defaultValue,
                                                                 &parameters[index].defaultValue)) {
                free_metadata_parameters(cs->state, parameters, parameterCount);
                return ZR_FALSE;
            }
            parameters[index].hasDefaultValue = ZR_TRUE;
        }

        if (!ZrParser_CompileTime_ApplyParameterDecorators(cs, paramNode, index, &parameters[index])) {
            free_metadata_parameters(cs->state, parameters, parameterCount);
            return ZR_FALSE;
        }
    }

    *outParameters = parameters;
    *outParameterCount = parameterCount;
    return ZR_TRUE;
}

static TZrBool compiler_build_compile_time_function_parameter_metadata_from_record(
        SZrCompilerState *cs,
        const SZrCompileTimeFunction *record,
        SZrFunctionMetadataParameter **outParameters,
        TZrUInt32 *outParameterCount) {
    TZrUInt32 parameterCount;
    SZrFunctionMetadataParameter *parameters;

    if (outParameters == ZR_NULL || outParameterCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outParameters = ZR_NULL;
    *outParameterCount = 0;
    if (cs == ZR_NULL || record == ZR_NULL || record->paramTypes.length == 0) {
        return ZR_TRUE;
    }

    parameterCount = (TZrUInt32)record->paramTypes.length;
    parameters = (SZrFunctionMetadataParameter *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionMetadataParameter) * parameterCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (parameters == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(parameters, 0, sizeof(SZrFunctionMetadataParameter) * parameterCount);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrInferredType *paramType =
                (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&record->paramTypes, index);
        SZrString **paramNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&record->paramNames, index);
        TZrBool *hasDefaultValuePtr =
                (TZrBool *)ZrCore_Array_Get((SZrArray *)&record->paramHasDefaultValues, index);
        SZrTypeValue *defaultValue =
                (SZrTypeValue *)ZrCore_Array_Get((SZrArray *)&record->paramDefaultValues, index);

        typed_type_ref_init_unknown(&parameters[index].type);
        parameters[index].name = paramNamePtr != ZR_NULL ? *paramNamePtr : ZR_NULL;
        parameters[index].hasDefaultValue = ZR_FALSE;
        ZrCore_Value_ResetAsNull(&parameters[index].defaultValue);
        parameters[index].hasDecoratorMetadata = ZR_FALSE;
        ZrCore_Value_ResetAsNull(&parameters[index].decoratorMetadataValue);
        parameters[index].decoratorNames = ZR_NULL;
        parameters[index].decoratorCount = 0;

        if (paramType != ZR_NULL) {
            typed_type_ref_from_inferred(&parameters[index].type, paramType);
        }
        if (hasDefaultValuePtr != ZR_NULL && *hasDefaultValuePtr && defaultValue != ZR_NULL) {
            ZrCore_Value_Copy(cs->state, &parameters[index].defaultValue, defaultValue);
            parameters[index].hasDefaultValue = ZR_TRUE;
        }
    }

    *outParameters = parameters;
    *outParameterCount = parameterCount;
    return ZR_TRUE;
}

static TZrBool build_compile_time_variable_infos(SZrCompilerState *cs,
                                                 SZrFunctionCompileTimeVariableInfo **outInfos,
                                                 TZrUInt32 *outCount) {
    TZrUInt32 infoCount;
    SZrFunctionCompileTimeVariableInfo *infos;
    SZrCompileTimeBindingSourceVariable *bindingSources = ZR_NULL;
    SZrCompileTimeVariableBindingBuildContext bindingContext;
    SZrCompileTimeBindingResolver resolver;

    if (outInfos == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outInfos = ZR_NULL;
    *outCount = 0;
    if (cs == ZR_NULL || cs->compileTimeVariables.length == 0) {
        return ZR_TRUE;
    }

    infoCount = (TZrUInt32)cs->compileTimeVariables.length;
    infos = (SZrFunctionCompileTimeVariableInfo *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionCompileTimeVariableInfo) * infoCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (infos == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingSources = (SZrCompileTimeBindingSourceVariable *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrCompileTimeBindingSourceVariable) * infoCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (bindingSources == ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      infos,
                                      sizeof(SZrFunctionCompileTimeVariableInfo) * infoCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(infos, 0, sizeof(SZrFunctionCompileTimeVariableInfo) * infoCount);
    ZrCore_Memory_RawSet(bindingSources, 0, sizeof(SZrCompileTimeBindingSourceVariable) * infoCount);
    for (TZrUInt32 index = 0; index < infoCount; index++) {
        SZrCompileTimeVariable **recordPtr =
                (SZrCompileTimeVariable **)ZrCore_Array_Get(&cs->compileTimeVariables, index);
        SZrCompileTimeVariable *record = recordPtr != ZR_NULL ? *recordPtr : ZR_NULL;

        typed_type_ref_init_unknown(&infos[index].type);
        if (record == ZR_NULL) {
            continue;
        }

        infos[index].name = record->name;
        infos[index].lineInSourceStart =
                record->location.start.line > 0 ? (TZrUInt32)record->location.start.line : 0;
        infos[index].lineInSourceEnd =
                record->location.end.line > 0 ? (TZrUInt32)record->location.end.line : 0;
        typed_type_ref_from_inferred(&infos[index].type, &record->type);
        bindingSources[index].name = record->name;
        bindingSources[index].value = record->value;
        bindingSources[index].info = &infos[index];
    }

    bindingContext.cs = cs;
    bindingContext.variables = bindingSources;
    bindingContext.variableCount = infoCount;
    resolver.state = cs->state;
    resolver.userData = &bindingContext;
    resolver.findVariable = find_compile_time_binding_variable_source;
    resolver.findFunction = find_compile_time_binding_function;
    resolver.findDecoratorClass = find_compile_time_binding_decorator_class;
    if (!ZrParser_CompileTimeBinding_ResolveAll(&resolver, bindingSources, infoCount)) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      bindingSources,
                                      sizeof(SZrCompileTimeBindingSourceVariable) * infoCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        for (TZrUInt32 index = 0; index < infoCount; index++) {
            if (infos[index].pathBindings != ZR_NULL && infos[index].pathBindingCount > 0) {
                ZrCore_Memory_RawFreeWithType(cs->state->global,
                                              infos[index].pathBindings,
                                              sizeof(SZrFunctionCompileTimePathBinding) * infos[index].pathBindingCount,
                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            }
        }
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      infos,
                                      sizeof(SZrFunctionCompileTimeVariableInfo) * infoCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        return ZR_FALSE;
    }

    ZrCore_Memory_RawFreeWithType(cs->state->global,
                                  bindingSources,
                                  sizeof(SZrCompileTimeBindingSourceVariable) * infoCount,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);

    *outInfos = infos;
    *outCount = infoCount;
    return ZR_TRUE;
}

static TZrBool build_compile_time_function_infos(SZrCompilerState *cs,
                                                 SZrFunctionCompileTimeFunctionInfo **outInfos,
                                                 TZrUInt32 *outCount) {
    TZrUInt32 infoCount;
    SZrFunctionCompileTimeFunctionInfo *infos;

    if (outInfos == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outInfos = ZR_NULL;
    *outCount = 0;
    if (cs == ZR_NULL || cs->compileTimeFunctions.length == 0) {
        return ZR_TRUE;
    }

    infoCount = (TZrUInt32)cs->compileTimeFunctions.length;
    infos = (SZrFunctionCompileTimeFunctionInfo *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionCompileTimeFunctionInfo) * infoCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (infos == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(infos, 0, sizeof(SZrFunctionCompileTimeFunctionInfo) * infoCount);
    for (TZrUInt32 index = 0; index < infoCount; index++) {
        SZrCompileTimeFunction **recordPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, index);
        SZrCompileTimeFunction *record = recordPtr != ZR_NULL ? *recordPtr : ZR_NULL;
        SZrFunctionDeclaration *declaration = ZR_NULL;

        typed_type_ref_init_unknown(&infos[index].returnType);
        if (record == ZR_NULL) {
            continue;
        }

        infos[index].name = record->name;
        infos[index].lineInSourceStart =
                record->location.start.line > 0 ? (TZrUInt32)record->location.start.line : 0;
        infos[index].lineInSourceEnd =
                record->location.end.line > 0 ? (TZrUInt32)record->location.end.line : 0;
        typed_type_ref_from_inferred(&infos[index].returnType, &record->returnType);

        if (record->declaration != ZR_NULL && record->declaration->type == ZR_AST_FUNCTION_DECLARATION) {
            declaration = &record->declaration->data.functionDeclaration;
        }

        if (declaration != ZR_NULL) {
            if (!compiler_build_function_parameter_metadata(cs,
                                                            declaration->params,
                                                            ZR_TRUE,
                                                            &infos[index].parameters,
                                                            &infos[index].parameterCount)) {
                free_compile_time_function_infos(cs->state, infos, infoCount);
                return ZR_FALSE;
            }
        } else if (record->paramTypes.length > 0) {
            if (!compiler_build_compile_time_function_parameter_metadata_from_record(cs,
                                                                                     record,
                                                                                     &infos[index].parameters,
                                                                                     &infos[index].parameterCount)) {
                free_compile_time_function_infos(cs->state, infos, infoCount);
                return ZR_FALSE;
            }
        }
    }

    *outInfos = infos;
    *outCount = infoCount;
    return ZR_TRUE;
}

static TZrBool build_test_infos(SZrCompilerState *cs,
                                SZrFunctionTestInfo **outInfos,
                                TZrUInt32 *outCount) {
    TZrUInt32 infoCount = 0;
    SZrFunctionTestInfo *infos;
    TZrUInt32 writeIndex = 0;

    if (outInfos == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outInfos = ZR_NULL;
    *outCount = 0;
    if (cs == ZR_NULL ||
        cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT ||
        cs->scriptAst->data.script.statements == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < cs->scriptAst->data.script.statements->count; index++) {
        SZrAstNode *statement = cs->scriptAst->data.script.statements->nodes[index];
        if (statement != ZR_NULL && statement->type == ZR_AST_TEST_DECLARATION) {
            infoCount++;
        }
    }

    if (infoCount == 0) {
        return ZR_TRUE;
    }

    infos = (SZrFunctionTestInfo *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionTestInfo) * infoCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (infos == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(infos, 0, sizeof(SZrFunctionTestInfo) * infoCount);
    for (TZrSize index = 0; index < cs->scriptAst->data.script.statements->count; index++) {
        SZrAstNode *statement = cs->scriptAst->data.script.statements->nodes[index];
        SZrTestDeclaration *declaration;

        if (statement == ZR_NULL || statement->type != ZR_AST_TEST_DECLARATION) {
            continue;
        }

        declaration = &statement->data.testDeclaration;
        infos[writeIndex].name = declaration->name != ZR_NULL ? declaration->name->name : ZR_NULL;
        infos[writeIndex].hasVariableArguments = declaration->args != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        infos[writeIndex].lineInSourceStart =
                statement->location.start.line > 0 ? (TZrUInt32)statement->location.start.line : 0;
        infos[writeIndex].lineInSourceEnd =
                statement->location.end.line > 0 ? (TZrUInt32)statement->location.end.line : 0;
        if (!compiler_build_function_parameter_metadata(cs,
                                                        declaration->params,
                                                        ZR_FALSE,
                                                        &infos[writeIndex].parameters,
                                                        &infos[writeIndex].parameterCount)) {
            free_test_infos(cs->state, infos, infoCount);
            return ZR_FALSE;
        }
        writeIndex++;
    }

    *outInfos = infos;
    *outCount = infoCount;
    return ZR_TRUE;
}

static TZrBool build_function_like_export_symbol(SZrCompilerState *cs,
                                                 const SZrExportedVariable *exportedVar,
                                                 SZrAstNodeArray *params,
                                                 SZrType *returnType,
                                                 SZrFunctionTypedExportSymbol *outSymbol) {
    SZrFunctionTypeInfo *functionInfo;
    TZrUInt32 parameterCount = 0;

    if (cs == ZR_NULL || exportedVar == ZR_NULL || outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outSymbol, 0, sizeof(*outSymbol));
    outSymbol->name = exportedVar->name;
    outSymbol->stackSlot = exportedVar->stackSlot;
    outSymbol->accessModifier = (TZrUInt8)exportedVar->accessModifier;
    outSymbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    outSymbol->exportKind = (TZrUInt8)exportedVar->exportKind;
    outSymbol->readiness = (TZrUInt8)exportedVar->readiness;
    outSymbol->reserved0 = 0;
    outSymbol->callableChildIndex = exportedVar->callableChildIndex;
    functionInfo = find_callable_binding_info(cs, exportedVar->name);

    if (returnType != ZR_NULL) {
        if (!typed_type_ref_from_ast_type(cs, returnType, &outSymbol->valueType)) {
            return ZR_FALSE;
        }
    } else if (!typed_type_ref_from_callable_binding(cs, exportedVar->name, &outSymbol->valueType) &&
               !typed_type_ref_from_ast_type(cs, ZR_NULL, &outSymbol->valueType)) {
        return ZR_FALSE;
    }

    if (params != ZR_NULL) {
        parameterCount = (TZrUInt32)params->count;
    } else if (functionInfo != ZR_NULL) {
        parameterCount = (TZrUInt32)functionInfo->paramTypes.length;
    }
    outSymbol->parameterCount = parameterCount;
    if (parameterCount == 0) {
        return ZR_TRUE;
    }

    outSymbol->parameterTypes = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionTypedTypeRef) * parameterCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outSymbol->parameterTypes == ZR_NULL) {
        outSymbol->parameterCount = 0;
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        SZrAstNode *paramNode =
                (params != ZR_NULL && index < params->count) ? params->nodes[index] : ZR_NULL;
        SZrParameter *parameter;

        typed_type_ref_init_unknown(&outSymbol->parameterTypes[index]);
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        if (parameter->typeInfo == ZR_NULL) {
            if (functionInfo != ZR_NULL && index < functionInfo->paramTypes.length) {
                const SZrInferredType *paramType =
                        (const SZrInferredType *)ZrCore_Array_Get(&functionInfo->paramTypes, index);
                if (paramType != ZR_NULL) {
                    typed_type_ref_from_inferred(&outSymbol->parameterTypes[index], paramType);
                }
            }
            continue;
        }

        if (!typed_type_ref_from_ast_type(cs, parameter->typeInfo, &outSymbol->parameterTypes[index])) {
            return ZR_FALSE;
        }
    }

    if ((params == ZR_NULL || params->count == 0) &&
        functionInfo != ZR_NULL &&
        functionInfo->paramTypes.length > 0) {
        for (TZrUInt32 index = 0;
             index < parameterCount && index < functionInfo->paramTypes.length;
             index++) {
            const SZrInferredType *paramType =
                    (const SZrInferredType *)ZrCore_Array_Get(&functionInfo->paramTypes, index);
            if (paramType != ZR_NULL) {
                typed_type_ref_from_inferred(&outSymbol->parameterTypes[index], paramType);
            }
        }
    }

    return ZR_TRUE;
}

static TZrBool build_function_export_symbol(SZrCompilerState *cs,
                                            const SZrExportedVariable *exportedVar,
                                            SZrFunctionDeclaration *declaration,
                                            SZrFunctionTypedExportSymbol *outSymbol) {
    if (cs == ZR_NULL || exportedVar == ZR_NULL || declaration == ZR_NULL || outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    return build_function_like_export_symbol(cs,
                                             exportedVar,
                                             declaration->params,
                                             declaration->returnType,
                                             outSymbol);
}

static SZrTypeMemberInfo *find_imported_callable_member_alias_info(SZrCompilerState *cs,
                                                                   SZrAstNode *valueNode) {
    SZrPrimaryExpression *primary;
    SZrAstNode *memberNode;
    SZrString *moduleTypeName = ZR_NULL;
    SZrString *memberName;
    SZrInferredType baseType;
    TZrBool baseTypeInitialized = ZR_FALSE;
    SZrTypeMemberInfo *memberInfo = ZR_NULL;

    if (cs == ZR_NULL || valueNode == ZR_NULL ||
        valueNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_NULL;
    }

    primary = &valueNode->data.primaryExpression;
    if (primary->property == ZR_NULL ||
        primary->members == ZR_NULL ||
        primary->members->count != 1) {
        return ZR_NULL;
    }

    memberNode = primary->members->nodes[0];
    if (memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        memberNode->data.memberExpression.property->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }
    memberName = memberNode->data.memberExpression.property->data.identifier.name;

    if (primary->property->type == ZR_AST_IMPORT_EXPRESSION &&
        primary->property->data.importExpression.modulePath != ZR_NULL &&
        primary->property->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
        primary->property->data.importExpression.modulePath->data.stringLiteral.value != ZR_NULL) {
        moduleTypeName = primary->property->data.importExpression.modulePath->data.stringLiteral.value;
        (void)ensure_import_module_compile_info(cs, moduleTypeName);
    } else {
        ZrParser_InferredType_Init(cs->state, &baseType, ZR_VALUE_TYPE_OBJECT);
        baseTypeInitialized = ZR_TRUE;
        if (ZrParser_ExpressionType_Infer(cs, primary->property, &baseType)) {
            moduleTypeName = baseType.typeName;
        }
    }

    if (moduleTypeName != ZR_NULL) {
        memberInfo = find_compiler_type_member_inference(cs, moduleTypeName, memberName);
    }

    if (baseTypeInitialized) {
        ZrParser_InferredType_Free(cs->state, &baseType);
    }

    if (memberInfo == ZR_NULL ||
        memberInfo->memberType != ZR_AST_CLASS_METHOD ||
        memberInfo->moduleExportKind == ZR_MODULE_EXPORT_KIND_TYPE) {
        return ZR_NULL;
    }

    return memberInfo;
}

static TZrBool build_imported_callable_member_alias_export_symbol(SZrCompilerState *cs,
                                                                  const SZrExportedVariable *exportedVar,
                                                                  const SZrTypeMemberInfo *memberInfo,
                                                                  SZrFunctionTypedExportSymbol *outSymbol) {
    TZrUInt32 parameterCount;

    if (cs == ZR_NULL || exportedVar == ZR_NULL || memberInfo == ZR_NULL || outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outSymbol, 0, sizeof(*outSymbol));
    outSymbol->name = exportedVar->name;
    outSymbol->stackSlot = exportedVar->stackSlot;
    outSymbol->accessModifier = (TZrUInt8)exportedVar->accessModifier;
    outSymbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;
    outSymbol->exportKind = (TZrUInt8)exportedVar->exportKind;
    outSymbol->readiness = (TZrUInt8)exportedVar->readiness;
    outSymbol->reserved0 = 0;
    outSymbol->callableChildIndex = exportedVar->callableChildIndex;
    typed_type_ref_from_type_name(cs, memberInfo->returnTypeName, &outSymbol->valueType);

    if (memberInfo->parameterCount == ZR_MEMBER_PARAMETER_COUNT_UNKNOWN ||
        memberInfo->parameterTypes.length == 0) {
        outSymbol->parameterCount = 0;
        return ZR_TRUE;
    }

    parameterCount = memberInfo->parameterCount;
    if (parameterCount > memberInfo->parameterTypes.length) {
        parameterCount = (TZrUInt32)memberInfo->parameterTypes.length;
    }
    outSymbol->parameterCount = parameterCount;
    if (parameterCount == 0) {
        return ZR_TRUE;
    }

    outSymbol->parameterTypes =
            (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                       sizeof(SZrFunctionTypedTypeRef) * parameterCount,
                                                                       ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (outSymbol->parameterTypes == ZR_NULL) {
        outSymbol->parameterCount = 0;
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrInferredType *parameterType =
                (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
        if (parameterType != ZR_NULL) {
            typed_type_ref_from_inferred(&outSymbol->parameterTypes[index], parameterType);
        } else {
            typed_type_ref_init_unknown(&outSymbol->parameterTypes[index]);
        }
    }

    return ZR_TRUE;
}

static void build_variable_export_symbol(SZrCompilerState *cs,
                                         const SZrExportedVariable *exportedVar,
                                         SZrFunctionTypedExportSymbol *outSymbol) {
    SZrInferredType inferredType;

    if (cs == ZR_NULL || exportedVar == ZR_NULL || outSymbol == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(outSymbol, 0, sizeof(*outSymbol));
    outSymbol->name = exportedVar->name;
    outSymbol->stackSlot = exportedVar->stackSlot;
    outSymbol->accessModifier = (TZrUInt8)exportedVar->accessModifier;
    outSymbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_VARIABLE;
    outSymbol->exportKind = (TZrUInt8)exportedVar->exportKind;
    outSymbol->readiness = (TZrUInt8)exportedVar->readiness;
    outSymbol->reserved0 = 0;
    outSymbol->callableChildIndex = exportedVar->callableChildIndex;

    ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (exportedVar->name != ZR_NULL &&
        cs->typeEnv != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, exportedVar->name, &inferredType)) {
        typed_type_ref_from_inferred(&outSymbol->valueType, &inferredType);
    } else {
        typed_type_ref_init_unknown(&outSymbol->valueType);
    }
    ZrParser_InferredType_Free(cs->state, &inferredType);
}

TZrBool compiler_build_typed_local_bindings(SZrCompilerState *cs,
                                            SZrFunctionTypedLocalBinding **outBindings,
                                            TZrUInt32 *outCount) {
    SZrFunctionTypedLocalBinding *bindings;
    TZrUInt32 localCount;

    if (outBindings == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outBindings = ZR_NULL;
    *outCount = 0;
    if (cs == ZR_NULL || cs->localVars.length == 0) {
        return ZR_TRUE;
    }

    localCount = (TZrUInt32)cs->localVars.length;
    bindings = (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionTypedLocalBinding) * localCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < localCount; index++) {
        SZrFunctionLocalVariable *localVar =
                (SZrFunctionLocalVariable *)ZrCore_Array_Get(&cs->localVars, index);
        SZrInferredType inferredType;
        SZrAstNode *functionDeclNode;

        ZrCore_Memory_RawSet(&bindings[index], 0, sizeof(bindings[index]));
        if (localVar == ZR_NULL) {
            typed_type_ref_init_unknown(&bindings[index].type);
            continue;
        }

        bindings[index].name = localVar->name;
        bindings[index].stackSlot = localVar->stackSlot;

        if (typed_type_ref_from_injected_this(cs, localVar->name, &bindings[index].type)) {
            continue;
        }

        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (localVar->name != ZR_NULL &&
            cs->typeEnv != ZR_NULL &&
            ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, localVar->name, &inferredType)) {
            typed_type_ref_from_inferred(&bindings[index].type, &inferredType);
            ZrParser_InferredType_Free(cs->state, &inferredType);
            continue;
        }

        ZrParser_InferredType_Free(cs->state, &inferredType);
        if (typed_type_ref_from_current_parameter(cs, localVar->name, &bindings[index].type)) {
            continue;
        }

        functionDeclNode = find_script_function_declaration_by_name(cs, localVar->name);
        if (functionDeclNode != ZR_NULL) {
            typed_type_ref_init_unknown(&bindings[index].type);
            bindings[index].type.baseType = ZR_VALUE_TYPE_CLOSURE;
        } else {
            typed_type_ref_init_unknown(&bindings[index].type);
        }
    }

    *outBindings = bindings;
    *outCount = localCount;
    return ZR_TRUE;
}

static TZrUInt32 frame_layout_align_offset(TZrUInt32 offset, TZrUInt32 align) {
    if (align <= 1) {
        return offset;
    }

    return ((offset + align - 1) / align) * align;
}

void compiler_clear_stack_slot_type_hints_from(SZrCompilerState *cs, TZrSize startIndex) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || !cs->stackSlotTypeHints.isValid) {
        return;
    }

    if (startIndex > cs->stackSlotTypeHints.length) {
        startIndex = cs->stackSlotTypeHints.length;
    }

    for (TZrSize index = startIndex; index < cs->stackSlotTypeHints.length; index++) {
        SZrCompilerStackSlotTypeHint *hint =
                (SZrCompilerStackSlotTypeHint *)ZrCore_Array_Get(&cs->stackSlotTypeHints, index);
        if (hint != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &hint->type);
        }
    }
    cs->stackSlotTypeHints.length = startIndex;
    if (cs->stackSlotTypeHintScopeStart > cs->stackSlotTypeHints.length) {
        cs->stackSlotTypeHintScopeStart = cs->stackSlotTypeHints.length;
    }
}

TZrSize compiler_enter_stack_slot_type_hint_scope(SZrCompilerState *cs) {
    TZrSize previousScopeStart;

    if (cs == ZR_NULL) {
        return 0;
    }

    previousScopeStart = cs->stackSlotTypeHintScopeStart;
    cs->stackSlotTypeHintScopeStart = cs->stackSlotTypeHints.length;
    return previousScopeStart;
}

void compiler_restore_stack_slot_type_hint_scope(SZrCompilerState *cs, TZrSize previousScopeStart) {
    if (cs == ZR_NULL) {
        return;
    }

    compiler_clear_stack_slot_type_hints_from(cs, cs->stackSlotTypeHintScopeStart);
    cs->stackSlotTypeHintScopeStart = previousScopeStart;
}

static const SZrCompilerStackSlotTypeHint *find_stack_slot_type_hint_for_slot(const SZrCompilerState *cs,
                                                                              TZrUInt32 stackSlot) {
    if (cs == ZR_NULL || !cs->stackSlotTypeHints.isValid) {
        return ZR_NULL;
    }

    for (TZrSize index = cs->stackSlotTypeHintScopeStart; index < cs->stackSlotTypeHints.length; index++) {
        const SZrCompilerStackSlotTypeHint *hint =
                (const SZrCompilerStackSlotTypeHint *)ZrCore_Array_Get((SZrArray *)&cs->stackSlotTypeHints, index);
        if (hint != ZR_NULL && hint->stackSlot == stackSlot) {
            return hint;
        }
    }

    return ZR_NULL;
}

static const SZrFunctionTypedLocalBinding *find_typed_local_binding_for_slot(const SZrFunction *function,
                                                                             TZrUInt32 stackSlot) {
    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        if (function->typedLocalBindings[index].stackSlot == stackSlot) {
            return &function->typedLocalBindings[index];
        }
    }

    return ZR_NULL;
}

static TZrBool typed_local_binding_slot_is_parameter(const SZrFunction *function, TZrUInt32 stackSlot) {
    TZrUInt32 parameterBindingCount = 0;

    if (function == ZR_NULL || function->parameterCount == 0) {
        return ZR_FALSE;
    }

    if (function->typedLocalBindings == ZR_NULL || function->typedLocalBindingLength == 0) {
        return (TZrBool)(stackSlot < function->parameterCount);
    }

    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];

        if (binding->name == ZR_NULL) {
            continue;
        }
        if (parameterBindingCount >= function->parameterCount) {
            break;
        }
        if (binding->stackSlot == stackSlot) {
            return ZR_TRUE;
        }
        parameterBindingCount++;
    }

    return ZR_FALSE;
}

static void compiler_finalize_current_struct_inline_layout(SZrCompilerState *cs, SZrTypePrototypeInfo *prototypeInfo) {
    TZrUInt32 currentOffset = 0;
    TZrUInt32 maxAlign = 0;

    if (cs == ZR_NULL ||
        prototypeInfo == ZR_NULL ||
        prototypeInfo->type != ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
        prototypeInfo->layoutByteAlign != 0) {
        return;
    }

    for (TZrSize index = 0; index < prototypeInfo->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototypeInfo->members, index);
        TZrUInt32 align;
        TZrUInt32 fieldSize;

        if (memberInfo == ZR_NULL ||
            memberInfo->memberType != ZR_AST_STRUCT_FIELD ||
            memberInfo->isStatic) {
            continue;
        }

        align = memberInfo->fieldType != ZR_NULL ? get_type_alignment(cs, memberInfo->fieldType) : ZR_ALIGN_SIZE;
        if (align > maxAlign) {
            maxAlign = align;
        }

        currentOffset = align_offset(currentOffset, align);
        memberInfo->fieldOffset = currentOffset;
        fieldSize = memberInfo->fieldSize != 0u ? memberInfo->fieldSize : ZR_ALIGN_SIZE;
        currentOffset += fieldSize;
    }

    prototypeInfo->layoutByteAlign = maxAlign;
    prototypeInfo->layoutByteSize = maxAlign > 0u ? align_offset(currentOffset, maxAlign) : 0u;
}

static TZrBool compiler_prototype_has_inline_frame_layout(const SZrTypePrototypeInfo *prototypeInfo) {
    return (TZrBool)(prototypeInfo != ZR_NULL &&
                     (prototypeInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ||
                      prototypeInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_UNION) &&
                     prototypeInfo->layoutByteSize > 0u &&
                     prototypeInfo->layoutByteAlign > 0u);
}

static TZrBool compiler_type_prototype_serializes_to_runtime(const SZrTypePrototypeInfo *prototypeInfo) {
    return (TZrBool)(prototypeInfo != ZR_NULL &&
                     prototypeInfo->name != ZR_NULL &&
                     !prototypeInfo->isImportedNative);
}

static TZrUInt32 compiler_serialized_type_prototype_count(SZrCompilerState *cs) {
    TZrUInt32 serializedCount = 0u;

    if (cs == ZR_NULL) {
        return 0u;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototypeInfo =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);

        if (compiler_type_prototype_serializes_to_runtime(prototypeInfo)) {
            serializedCount++;
        }
    }

    return serializedCount;
}

static TZrBool compiler_find_inline_type_layout(SZrCompilerState *cs,
                                                const SZrFunctionTypedTypeRef *typeRef,
                                                TZrUInt32 *outLayoutId,
                                                TZrUInt32 *outSize,
                                                TZrUInt32 *outAlign) {
    SZrTypePrototypeInfo *currentPrototypeInfo;
    TZrUInt32 serializedIndex = 0u;

    if (cs == ZR_NULL || typeRef == ZR_NULL || typeRef->typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    currentPrototypeInfo = cs->currentTypePrototypeInfo;
    if (currentPrototypeInfo != ZR_NULL &&
        currentPrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(currentPrototypeInfo->name, typeRef->typeName)) {
        if (currentPrototypeInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
            compiler_finalize_current_struct_inline_layout(cs, currentPrototypeInfo);
        }
        if (!compiler_prototype_has_inline_frame_layout(currentPrototypeInfo)) {
            return ZR_FALSE;
        }
        if (outLayoutId != ZR_NULL) {
            if (!compiler_type_prototype_serializes_to_runtime(currentPrototypeInfo)) {
                return ZR_FALSE;
            }
            *outLayoutId = compiler_serialized_type_prototype_count(cs);
        }
        if (outSize != ZR_NULL) {
            *outSize = currentPrototypeInfo->layoutByteSize;
        }
        if (outAlign != ZR_NULL) {
            *outAlign = currentPrototypeInfo->layoutByteAlign;
        }
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototypeInfo =
                (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (!compiler_type_prototype_serializes_to_runtime(prototypeInfo)) {
            continue;
        }
        if (prototypeInfo == ZR_NULL ||
            prototypeInfo->name == ZR_NULL ||
            !ZrCore_String_Equal(prototypeInfo->name, typeRef->typeName)) {
            serializedIndex++;
            continue;
        }

        if (prototypeInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
            compiler_finalize_current_struct_inline_layout(cs, prototypeInfo);
        }
        if (!compiler_prototype_has_inline_frame_layout(prototypeInfo)) {
            serializedIndex++;
            continue;
        }

        if (outLayoutId != ZR_NULL) {
            *outLayoutId = serializedIndex;
        }
        if (outSize != ZR_NULL) {
            *outSize = prototypeInfo->layoutByteSize;
        }
        if (outAlign != ZR_NULL) {
            *outAlign = prototypeInfo->layoutByteAlign;
        }
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

TZrBool compiler_register_stack_slot_type_hint(SZrCompilerState *cs,
                                               TZrUInt32 stackSlot,
                                               const SZrInferredType *type) {
    SZrFunctionTypedTypeRef typeRef;

    if (cs == ZR_NULL || type == ZR_NULL || !cs->stackSlotTypeHints.isValid) {
        return ZR_FALSE;
    }

    typed_type_ref_from_inferred(&typeRef, type);
    if (!compiler_find_inline_type_layout(cs, &typeRef, ZR_NULL, ZR_NULL, ZR_NULL)) {
        return ZR_TRUE;
    }

    if (cs->stackSlotCount <= (TZrSize)stackSlot) {
        cs->stackSlotCount = (TZrSize)stackSlot + 1u;
    }
    if (cs->maxStackSlotCount < cs->stackSlotCount) {
        cs->maxStackSlotCount = cs->stackSlotCount;
    }

    for (TZrSize index = cs->stackSlotTypeHintScopeStart; index < cs->stackSlotTypeHints.length; index++) {
        SZrCompilerStackSlotTypeHint *hint =
                (SZrCompilerStackSlotTypeHint *)ZrCore_Array_Get(&cs->stackSlotTypeHints, index);
        if (hint == ZR_NULL || hint->stackSlot != stackSlot) {
            continue;
        }

        ZrParser_InferredType_Free(cs->state, &hint->type);
        ZrParser_InferredType_Copy(cs->state, &hint->type, type);
        return ZR_TRUE;
    }

    {
        SZrCompilerStackSlotTypeHint hint;
        hint.stackSlot = stackSlot;
        ZrParser_InferredType_Copy(cs->state, &hint.type, type);
        ZrCore_Array_Push(cs->state, &cs->stackSlotTypeHints, &hint);
    }
    return ZR_TRUE;
}

static TZrBool compiler_instruction_extra_matches_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    return instruction != ZR_NULL &&
           instruction->instruction.operandExtra != ZR_INSTRUCTION_USE_RET_FLAG &&
           (TZrUInt32)instruction->instruction.operandExtra == slot;
}

static TZrBool compiler_instruction_binary_operands_match_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    return instruction != ZR_NULL &&
           ((TZrUInt32)instruction->instruction.operand.operand1[0] == slot ||
            (TZrUInt32)instruction->instruction.operand.operand1[1] == slot);
}

static TZrBool compiler_instruction_const_binary_operand_matches_slot(const TZrInstruction *instruction,
                                                                      TZrUInt32 slot) {
    return instruction != ZR_NULL && (TZrUInt32)instruction->instruction.operand.operand1[0] == slot;
}

static TZrBool compiler_instruction_operand0_pair_matches_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    return instruction != ZR_NULL &&
           ((TZrUInt32)instruction->instruction.operand.operand0[0] == slot ||
            (TZrUInt32)instruction->instruction.operand.operand0[1] == slot);
}

static TZrBool compiler_instruction_operand0_triple_matches_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    return compiler_instruction_operand0_pair_matches_slot(instruction, slot) ||
           (instruction != ZR_NULL && (TZrUInt32)instruction->instruction.operand.operand0[2] == slot);
}

static TZrBool compiler_instruction_requires_plain_value_slot(const TZrInstruction *instruction, TZrUInt32 slot) {
    EZrInstructionCode opcode;

    if (instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
        case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
        case ZR_INSTRUCTION_ENUM(GETUPVAL):
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL):
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
        case ZR_INSTRUCTION_ENUM(GET_MEMBER):
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_BIND_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUPER_ARRAY_GET_INT_ITEMS_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ITER_INIT):
        case ZR_INSTRUCTION_ENUM(ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(ITER_CURRENT):
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
        case ZR_INSTRUCTION_ENUM(TO_INT):
        case ZR_INSTRUCTION_ENUM(TO_UINT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
        case ZR_INSTRUCTION_ENUM(TO_STRING):
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
        case ZR_INSTRUCTION_ENUM(CATCH):
        case ZR_INSTRUCTION_ENUM(TYPEOF):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_INIT):
        case ZR_INSTRUCTION_ENUM(DYN_ITER_MOVE_NEXT):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_CACHED):
        case ZR_INSTRUCTION_ENUM(META_GET):
        case ZR_INSTRUCTION_ENUM(SUPER_META_GET_STATIC_CACHED):
            return compiler_instruction_extra_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(NEG):
        case ZR_INSTRUCTION_ENUM(NEG_SIGNED):
        case ZR_INSTRUCTION_ENUM(NEG_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE):
        case ZR_INSTRUCTION_ENUM(OWN_BORROW):
        case ZR_INSTRUCTION_ENUM(OWN_LOAN):
        case ZR_INSTRUCTION_ENUM(OWN_SHARE):
        case ZR_INSTRUCTION_ENUM(OWN_WEAK):
        case ZR_INSTRUCTION_ENUM(OWN_DETACH):
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE):
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE):
        case ZR_INSTRUCTION_ENUM(OWN_RETURN_LOAN):
            return compiler_instruction_extra_matches_slot(instruction, slot) ||
                   compiler_instruction_const_binary_operand_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB):
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
        case ZR_INSTRUCTION_ENUM(DIV):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
        case ZR_INSTRUCTION_ENUM(POW):
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
            return compiler_instruction_extra_matches_slot(instruction, slot) ||
                   compiler_instruction_binary_operands_match_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED_CONST):
            return compiler_instruction_extra_matches_slot(instruction, slot) ||
                   compiler_instruction_const_binary_operand_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_LOAD_STACK_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_LOAD_STACK):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_MOD_CONST):
            return compiler_instruction_extra_matches_slot(instruction, slot) ||
                   compiler_instruction_operand0_pair_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_LOAD_STACK_LOAD_CONST):
            return compiler_instruction_extra_matches_slot(instruction, slot) ||
                   compiler_instruction_operand0_triple_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(JUMP_IF):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_BOOL_FALSE):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NULL):
            return compiler_instruction_extra_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(JUMP_IF_GREATER_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_LESS_EQUAL_SIGNED):
        case ZR_INSTRUCTION_ENUM(JUMP_IF_NOT_EQUAL_SIGNED):
            return compiler_instruction_extra_matches_slot(instruction, slot) ||
                   compiler_instruction_const_binary_operand_matches_slot(instruction, slot);
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
        case ZR_INSTRUCTION_ENUM(SET_PENDING_RETURN):
            return compiler_instruction_const_binary_operand_matches_slot(instruction, slot);
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_stack_slot_requires_plain_value_layout(const SZrFunction *function, TZrUInt32 slot) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        if (compiler_instruction_requires_plain_value_slot(&function->instructionsList[index], slot)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool compiler_build_function_frame_layout_metadata(SZrCompilerState *cs, SZrFunction *function) {
    SZrFunctionFrameSlotLayout *layouts;
    TZrUInt32 slotCount;
    TZrUInt32 cursor = 0;
    TZrUInt32 frameAlign = 0;

    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->frameSlotLayouts != ZR_NULL && function->frameSlotLayoutLength > 0) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      function->frameSlotLayouts,
                                      sizeof(SZrFunctionFrameSlotLayout) * function->frameSlotLayoutLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        function->frameSlotLayouts = ZR_NULL;
        function->frameSlotLayoutLength = 0;
    }
    function->frameByteSize = 0;
    function->frameByteAlign = 0;

    if (function->stackSize == 0) {
        return ZR_TRUE;
    }

    slotCount = function->stackSize;
    layouts = (SZrFunctionFrameSlotLayout *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionFrameSlotLayout) * slotCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (layouts == ZR_NULL) {
        return ZR_FALSE;
    }

    cursor = frame_layout_align_offset((TZrUInt32)(slotCount * sizeof(SZrTypeValueOnStack)), ZR_ALIGN_SIZE);

    for (TZrUInt32 slot = 0; slot < slotCount; slot++) {
        const SZrFunctionTypedLocalBinding *binding = find_typed_local_binding_for_slot(function, slot);
        const SZrCompilerStackSlotTypeHint *hint =
                (binding == ZR_NULL && !compiler_stack_slot_requires_plain_value_layout(function, slot))
                        ? find_stack_slot_type_hint_for_slot(cs, slot)
                        : ZR_NULL;
        SZrFunctionTypedTypeRef hintTypeRef;
        const SZrFunctionTypedTypeRef *slotTypeRef = binding != ZR_NULL ? &binding->type : ZR_NULL;
        TZrUInt32 byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        TZrUInt32 byteAlign = ZR_ALIGN_SIZE;
        TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
        TZrUInt8 slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_VALUE;

        if (hint != ZR_NULL) {
            typed_type_ref_from_inferred(&hintTypeRef, &hint->type);
            slotTypeRef = &hintTypeRef;
        }

        if (compiler_find_inline_type_layout(cs, slotTypeRef,
                                             &typeLayoutId,
                                             &byteSize,
                                             &byteAlign)) {
            slotKind = ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT;
        }

        if (byteAlign == 0) {
            byteAlign = ZR_ALIGN_SIZE;
        }
        if (byteAlign > frameAlign) {
            frameAlign = byteAlign;
        }

        cursor = frame_layout_align_offset(cursor, byteAlign);
        ZrCore_Memory_RawSet(&layouts[slot], 0, sizeof(layouts[slot]));
        layouts[slot].stackSlot = slot;
        layouts[slot].byteOffset = cursor;
        layouts[slot].byteSize = byteSize;
        layouts[slot].byteAlign = byteAlign;
        layouts[slot].typeLayoutId = typeLayoutId;
        layouts[slot].slotKind = slotKind;
        layouts[slot].isParameter = typed_local_binding_slot_is_parameter(function, slot) ? 1 : 0;
        cursor += byteSize;
    }

    if (frameAlign == 0) {
        frameAlign = ZR_ALIGN_SIZE;
    }

    function->frameSlotLayouts = layouts;
    function->frameSlotLayoutLength = slotCount;
    function->frameByteAlign = frameAlign;
    function->frameByteSize = frame_layout_align_offset(cursor, frameAlign);
    return ZR_TRUE;
}

static TZrBool build_typed_export_symbols(SZrCompilerState *cs,
                                          SZrFunctionTypedExportSymbol **outSymbols,
                                          TZrUInt32 *outCount) {
    SZrFunctionTypedExportSymbol *symbols;
    TZrUInt32 exportCount;

    if (outSymbols == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outSymbols = ZR_NULL;
    *outCount = 0;
    if (cs == ZR_NULL || cs->proVariables.length == 0) {
        return ZR_TRUE;
    }

    exportCount = (TZrUInt32)cs->proVariables.length;
    symbols = (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(SZrFunctionTypedExportSymbol) * exportCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (symbols == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(symbols, 0, sizeof(SZrFunctionTypedExportSymbol) * exportCount);

    for (TZrUInt32 index = 0; index < exportCount; index++) {
        SZrExportedVariable *exportedVar =
                (SZrExportedVariable *)ZrCore_Array_Get(&cs->proVariables, index);
        SZrAstNode *functionDeclNode;
        SZrAstNode *variableDeclNode;

        if (exportedVar == ZR_NULL) {
            typed_type_ref_init_unknown(&symbols[index].valueType);
            continue;
        }

        functionDeclNode = find_script_function_declaration_for_export(cs, exportedVar);
        if (functionDeclNode != ZR_NULL && functionDeclNode->type == ZR_AST_FUNCTION_DECLARATION) {
            if (!build_function_export_symbol(cs,
                                              exportedVar,
                                               &functionDeclNode->data.functionDeclaration,
                                               &symbols[index])) {
                free_typed_export_symbols(cs->state, symbols, exportCount);
                return ZR_FALSE;
            }
            typed_export_symbol_set_declaration_from_function(&symbols[index], functionDeclNode);
        } else {
            variableDeclNode = find_script_variable_declaration_by_name(cs, exportedVar->name);
            if (variableDeclNode != ZR_NULL && variableDeclNode->type == ZR_AST_VARIABLE_DECLARATION) {
                SZrVariableDeclaration *declaration = &variableDeclNode->data.variableDeclaration;

                if (declaration->value != ZR_NULL &&
                    declaration->value->type == ZR_AST_IDENTIFIER_LITERAL &&
                    declaration->value->data.identifier.name != ZR_NULL) {
                    functionDeclNode =
                            find_script_function_declaration_by_name(cs, declaration->value->data.identifier.name);
                    if (functionDeclNode != ZR_NULL && functionDeclNode->type == ZR_AST_FUNCTION_DECLARATION) {
                        if (!build_function_export_symbol(cs,
                                                          exportedVar,
                                                          &functionDeclNode->data.functionDeclaration,
                                                          &symbols[index])) {
                            free_typed_export_symbols(cs->state, symbols, exportCount);
                            return ZR_FALSE;
                        }
                        typed_export_symbol_set_declaration_from_variable(&symbols[index], variableDeclNode);
                        continue;
                    }
                }

                if (declaration->value != ZR_NULL && declaration->value->type == ZR_AST_LAMBDA_EXPRESSION) {
                    if (!build_function_like_export_symbol(cs,
                                                           exportedVar,
                                                           declaration->value->data.lambdaExpression.params,
                                                           ZR_NULL,
                                                           &symbols[index])) {
                        free_typed_export_symbols(cs->state, symbols, exportCount);
                        return ZR_FALSE;
                    }
                    typed_export_symbol_set_declaration_from_variable(&symbols[index], variableDeclNode);
                    continue;
                }

                {
                    SZrTypeMemberInfo *memberInfo =
                            find_imported_callable_member_alias_info(cs, declaration->value);
                    if (memberInfo != ZR_NULL) {
                        if (!build_imported_callable_member_alias_export_symbol(cs,
                                                                                exportedVar,
                                                                                memberInfo,
                                                                                &symbols[index])) {
                            free_typed_export_symbols(cs->state, symbols, exportCount);
                            return ZR_FALSE;
                        }
                        typed_export_symbol_set_declaration_from_variable(&symbols[index], variableDeclNode);
                        continue;
                    }
                }
            }

            build_variable_export_symbol(cs, exportedVar, &symbols[index]);
            if (variableDeclNode != ZR_NULL) {
                typed_export_symbol_set_declaration_from_variable(&symbols[index], variableDeclNode);
            }
        }
    }

    *outSymbols = symbols;
    *outCount = exportCount;
    return ZR_TRUE;
}

TZrBool compiler_build_script_typed_metadata(SZrCompilerState *cs) {
    SZrFunctionTypedLocalBinding *localBindings = ZR_NULL;
    TZrUInt32 localBindingCount = 0;
    SZrFunctionTypedExportSymbol *exportSymbols = ZR_NULL;
    TZrUInt32 exportSymbolCount = 0;
    SZrFunctionCompileTimeVariableInfo *compileTimeVariableInfos = ZR_NULL;
    TZrUInt32 compileTimeVariableInfoCount = 0;
    SZrFunctionCompileTimeFunctionInfo *compileTimeFunctionInfos = ZR_NULL;
    TZrUInt32 compileTimeFunctionInfoCount = 0;
    SZrFunctionTestInfo *testInfos = ZR_NULL;
    TZrUInt32 testInfoCount = 0;

    if (cs == ZR_NULL || cs->currentFunction == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!compiler_build_typed_local_bindings(cs, &localBindings, &localBindingCount)) {
        return ZR_FALSE;
    }

    if (!build_typed_export_symbols(cs, &exportSymbols, &exportSymbolCount)) {
        if (localBindings != ZR_NULL && localBindingCount > 0) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          localBindings,
                                          sizeof(SZrFunctionTypedLocalBinding) * localBindingCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    if (!build_compile_time_variable_infos(cs, &compileTimeVariableInfos, &compileTimeVariableInfoCount)) {
        if (localBindings != ZR_NULL && localBindingCount > 0) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          localBindings,
                                          sizeof(SZrFunctionTypedLocalBinding) * localBindingCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (exportSymbols != ZR_NULL && exportSymbolCount > 0) {
            free_typed_export_symbols(cs->state, exportSymbols, exportSymbolCount);
        }
        return ZR_FALSE;
    }

    if (!build_compile_time_function_infos(cs, &compileTimeFunctionInfos, &compileTimeFunctionInfoCount)) {
        if (localBindings != ZR_NULL && localBindingCount > 0) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          localBindings,
                                          sizeof(SZrFunctionTypedLocalBinding) * localBindingCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (exportSymbols != ZR_NULL && exportSymbolCount > 0) {
            free_typed_export_symbols(cs->state, exportSymbols, exportSymbolCount);
        }
        if (compileTimeVariableInfos != ZR_NULL && compileTimeVariableInfoCount > 0) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          compileTimeVariableInfos,
                                          sizeof(SZrFunctionCompileTimeVariableInfo) * compileTimeVariableInfoCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        return ZR_FALSE;
    }

    if (!build_test_infos(cs, &testInfos, &testInfoCount)) {
        if (localBindings != ZR_NULL && localBindingCount > 0) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          localBindings,
                                          sizeof(SZrFunctionTypedLocalBinding) * localBindingCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (exportSymbols != ZR_NULL && exportSymbolCount > 0) {
            free_typed_export_symbols(cs->state, exportSymbols, exportSymbolCount);
        }
        if (compileTimeVariableInfos != ZR_NULL && compileTimeVariableInfoCount > 0) {
            ZrCore_Memory_RawFreeWithType(cs->state->global,
                                          compileTimeVariableInfos,
                                          sizeof(SZrFunctionCompileTimeVariableInfo) * compileTimeVariableInfoCount,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        }
        if (compileTimeFunctionInfos != ZR_NULL && compileTimeFunctionInfoCount > 0) {
            free_compile_time_function_infos(cs->state, compileTimeFunctionInfos, compileTimeFunctionInfoCount);
        }
        return ZR_FALSE;
    }

    cs->currentFunction->typedLocalBindings = localBindings;
    cs->currentFunction->typedLocalBindingLength = localBindingCount;
    cs->currentFunction->typedExportedSymbols = exportSymbols;
    cs->currentFunction->typedExportedSymbolLength = exportSymbolCount;
    cs->currentFunction->compileTimeVariableInfos = compileTimeVariableInfos;
    cs->currentFunction->compileTimeVariableInfoLength = compileTimeVariableInfoCount;
    cs->currentFunction->compileTimeFunctionInfos = compileTimeFunctionInfos;
    cs->currentFunction->compileTimeFunctionInfoLength = compileTimeFunctionInfoCount;
    cs->currentFunction->testInfos = testInfos;
    cs->currentFunction->testInfoLength = testInfoCount;
    return compiler_build_function_metadata_tokens(cs, cs->currentFunction);
}
