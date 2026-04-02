//
// Strongly-typed function metadata builder for compiled script entry functions.
//

#include "compiler_internal.h"

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

static TZrBool build_metadata_parameters_from_ast(SZrCompilerState *cs,
                                                  SZrAstNodeArray *params,
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
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameters[index].name =
                paramNode->data.parameter.name != ZR_NULL ? paramNode->data.parameter.name->name : ZR_NULL;
        if (!typed_type_ref_from_ast_type(cs, paramNode->data.parameter.typeInfo, &parameters[index].type)) {
            free_metadata_parameters(cs->state, parameters, parameterCount);
            return ZR_FALSE;
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

    ZrCore_Memory_RawSet(infos, 0, sizeof(SZrFunctionCompileTimeVariableInfo) * infoCount);
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
    }

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

        if (declaration != ZR_NULL &&
            !build_metadata_parameters_from_ast(cs,
                                                declaration->params,
                                                &infos[index].parameters,
                                                &infos[index].parameterCount)) {
            free_compile_time_function_infos(cs->state, infos, infoCount);
            return ZR_FALSE;
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
        if (!build_metadata_parameters_from_ast(cs,
                                                declaration->params,
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
    TZrUInt32 parameterCount = 0;
    TZrBool hasCompleteParameterTypes = ZR_TRUE;

    if (cs == ZR_NULL || exportedVar == ZR_NULL || outSymbol == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(outSymbol, 0, sizeof(*outSymbol));
    outSymbol->name = exportedVar->name;
    outSymbol->stackSlot = exportedVar->stackSlot;
    outSymbol->accessModifier = (TZrUInt8)exportedVar->accessModifier;
    outSymbol->symbolKind = ZR_FUNCTION_TYPED_SYMBOL_FUNCTION;

    if (!typed_type_ref_from_ast_type(cs, returnType, &outSymbol->valueType)) {
        return ZR_FALSE;
    }

    parameterCount = params != ZR_NULL ? (TZrUInt32)params->count : 0;
    outSymbol->parameterCount = parameterCount;
    if (parameterCount == 0) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        SZrAstNode *paramNode = params->nodes[index];

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER || paramNode->data.parameter.typeInfo == ZR_NULL) {
            hasCompleteParameterTypes = ZR_FALSE;
            break;
        }
    }

    if (!hasCompleteParameterTypes) {
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
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *parameter;

        typed_type_ref_init_unknown(&outSymbol->parameterTypes[index]);
        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        if (!typed_type_ref_from_ast_type(cs, parameter->typeInfo, &outSymbol->parameterTypes[index])) {
            return ZR_FALSE;
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

        ZrParser_InferredType_Init(cs->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        if (localVar->name != ZR_NULL &&
            cs->typeEnv != ZR_NULL &&
            ZrParser_TypeEnvironment_LookupVariable(cs->state, cs->typeEnv, localVar->name, &inferredType)) {
            typed_type_ref_from_inferred(&bindings[index].type, &inferredType);
            ZrParser_InferredType_Free(cs->state, &inferredType);
            continue;
        }

        ZrParser_InferredType_Free(cs->state, &inferredType);
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

        functionDeclNode = find_script_function_declaration_by_name(cs, exportedVar->name);
        if (functionDeclNode != ZR_NULL && functionDeclNode->type == ZR_AST_FUNCTION_DECLARATION) {
            if (!build_function_export_symbol(cs,
                                              exportedVar,
                                               &functionDeclNode->data.functionDeclaration,
                                               &symbols[index])) {
                free_typed_export_symbols(cs->state, symbols, exportCount);
                return ZR_FALSE;
            }
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
                    continue;
                }
            }

            build_variable_export_symbol(cs, exportedVar, &symbols[index]);
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
    return ZR_TRUE;
}
