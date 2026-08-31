#include "semantic/semantic_analyzer_internal.h"

static SZrFileRange extern_parameter_diagnostic_location(
        const SZrAstNode *parameterNode) {
    if (parameterNode != ZR_NULL &&
        parameterNode->type == ZR_AST_PARAMETER &&
        parameterNode->data.parameter.name != ZR_NULL) {
        return parameterNode->data.parameter.nameLocation;
    }

    return parameterNode != ZR_NULL
           ? parameterNode->location
           : ZrParser_FileRange_Create(
                     ZrParser_FilePosition_Create(0U, 0, 0),
                     ZrParser_FilePosition_Create(0U, 0, 0),
                     ZR_NULL);
}

static SZrFunctionTypeInfo *find_function_type_binding_for_declaration(
        SZrTypeEnvironment *typeEnv,
        SZrString *name,
        SZrAstNode *declarationNode) {
    if (typeEnv == ZR_NULL || name == ZR_NULL || declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < typeEnv->functionReturnTypes.length; index++) {
        SZrFunctionTypeInfo **candidate = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
                &typeEnv->functionReturnTypes, index);

        if (candidate != ZR_NULL && *candidate != ZR_NULL &&
            (*candidate)->declarationNode == declarationNode &&
            (*candidate)->name != ZR_NULL &&
            ZrCore_String_Equal((*candidate)->name, name)) {
            return *candidate;
        }
    }

    return ZR_NULL;
}

static SZrFunctionTypeInfo *find_function_type_binding_by_identity(
        SZrTypeEnvironment *typeEnv,
        TZrSymbolId symbolId,
        TZrTypeId typeId) {
    if (typeEnv == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < typeEnv->functionReturnTypes.length; index++) {
        SZrFunctionTypeInfo **candidate = (SZrFunctionTypeInfo **)ZrCore_Array_Get(
                &typeEnv->functionReturnTypes, index);

        if (candidate != ZR_NULL && *candidate != ZR_NULL &&
            (*candidate)->symbolId == symbolId &&
            (*candidate)->typeId == typeId) {
            return *candidate;
        }
    }

    return ZR_NULL;
}

static SZrFunctionTypeInfo *register_extern_function_type_binding_in_env(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrTypeEnvironment *typeEnv,
        SZrAstNode *declarationNode,
        SZrExternFunctionDeclaration *funcDecl) {
    SZrCompilerState *compilerState;
    SZrInferredType returnType;
    SZrArray paramTypes;
    SZrArray parameterPassingModes;

    if (state == ZR_NULL || analyzer == ZR_NULL || declarationNode == ZR_NULL ||
        funcDecl == ZR_NULL) {
        return ZR_NULL;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL ||
        typeEnv == ZR_NULL ||
        funcDecl->name == ZR_NULL ||
        funcDecl->name->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (funcDecl->returnType != ZR_NULL) {
        ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_OBJECT);
        if (!ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
                    analyzer,
                    ZR_NULL,
                    declarationNode,
                    funcDecl->returnType,
                    &returnType)) {
            ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
                    state,
                    analyzer,
                    funcDecl->returnType->name != ZR_NULL
                        ? funcDecl->returnType->name->location
                        : declarationNode->location);
            ZrParser_InferredType_Free(state, &returnType);
            return ZR_NULL;
        }
    } else {
        ZrParser_InferredType_Init(state, &returnType, ZR_VALUE_TYPE_NULL);
    }

    ZrCore_Array_Init(state,
                      &paramTypes,
                      sizeof(SZrInferredType),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    ZrCore_Array_Init(state,
                      &parameterPassingModes,
                      sizeof(EZrParameterPassingMode),
                      funcDecl->params != ZR_NULL ? funcDecl->params->count : 0);
    if (funcDecl->params != ZR_NULL) {
        for (TZrSize index = 0; index < funcDecl->params->count; index++) {
            SZrAstNode *paramNode = funcDecl->params->nodes[index];
            SZrInferredType paramType;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            ZrParser_InferredType_Init(state, &paramType, ZR_VALUE_TYPE_OBJECT);
            if (paramNode->data.parameter.typeInfo != ZR_NULL) {
                if (!ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
                            analyzer,
                            ZR_NULL,
                            declarationNode,
                            paramNode->data.parameter.typeInfo,
                            &paramType)) {
                    ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType(
                            state,
                            analyzer,
                            extern_parameter_diagnostic_location(paramNode));
                    ZrParser_InferredType_Free(state, &paramType);
                    continue;
                }
            }

            passingMode = paramNode->data.parameter.passingMode;
            ZrCore_Array_Push(state, &paramTypes, &paramType);
            ZrCore_Array_Push(state, &parameterPassingModes, &passingMode);
        }
    }

    (void)ZrParser_TypeEnvironment_RegisterFunctionEx(state,
                                                      typeEnv,
                                                      funcDecl->name->name,
                                                      &returnType,
                                                      &paramTypes,
                                                      ZR_NULL,
                                                      &parameterPassingModes,
                                                      declarationNode);

    ZrParser_InferredType_Free(state, &returnType);
    for (TZrSize index = 0; index < paramTypes.length; index++) {
        SZrInferredType *paramType = (SZrInferredType *)ZrCore_Array_Get(
                &paramTypes, index);
        if (paramType != ZR_NULL) {
            ZrParser_InferredType_Free(state, paramType);
        }
    }
    ZrCore_Array_Free(state, &paramTypes);
    ZrCore_Array_Free(state, &parameterPassingModes);
    return find_function_type_binding_for_declaration(
            typeEnv, funcDecl->name->name, declarationNode);
}

SZrFunctionTypeInfo *
ZrLanguageServer_SemanticAnalyzer_RegisterCanonicalExternFunctionBindings(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *declarationNode,
        SZrExternFunctionDeclaration *funcDecl) {
    SZrTypeEnvironment *runtimeTypeEnv;
    SZrTypeEnvironment *compileTimeTypeEnv;
    SZrFunctionTypeInfo *canonicalFunction;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        declarationNode == ZR_NULL || funcDecl == ZR_NULL ||
        funcDecl->name == ZR_NULL || funcDecl->name->name == ZR_NULL) {
        return ZR_NULL;
    }

    runtimeTypeEnv = analyzer->compilerState->typeEnv;
    compileTimeTypeEnv = analyzer->compilerState->compileTimeTypeEnv;
    canonicalFunction = register_extern_function_type_binding_in_env(
            state, analyzer, runtimeTypeEnv, declarationNode, funcDecl);
    if (canonicalFunction == ZR_NULL) {
        return register_extern_function_type_binding_in_env(
                state, analyzer, compileTimeTypeEnv, declarationNode, funcDecl);
    }

    if (compileTimeTypeEnv != ZR_NULL &&
        compileTimeTypeEnv != runtimeTypeEnv &&
        canonicalFunction->symbolId != ZR_SEMANTIC_ID_INVALID &&
        canonicalFunction->typeId != ZR_SEMANTIC_ID_INVALID &&
        canonicalFunction->hasDeclarationRange &&
        ZrParser_TypeEnvironment_RegisterCanonicalFunction(
                state,
                compileTimeTypeEnv,
                funcDecl->name->name,
                &canonicalFunction->returnType,
                &canonicalFunction->paramTypes,
                &canonicalFunction->parameterPassingModes,
                canonicalFunction->symbolId,
                canonicalFunction->typeId,
                canonicalFunction->declarationRange)) {
        SZrFunctionTypeInfo *compileTimeFunction = find_function_type_binding_by_identity(
                compileTimeTypeEnv,
                canonicalFunction->symbolId,
                canonicalFunction->typeId);
        if (compileTimeFunction != ZR_NULL) {
            compileTimeFunction->declarationNode = declarationNode;
        }
    }

    return canonicalFunction;
}
