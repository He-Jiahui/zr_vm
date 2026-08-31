#include "semantic/semantic_analyzer_internal.h"

static TZrBool callable_has_static_receiver(const SZrAstNode *functionNode) {
    if (functionNode == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (functionNode->type) {
        case ZR_AST_CLASS_METHOD:
            return functionNode->data.classMethod.isStatic;
        case ZR_AST_CLASS_META_FUNCTION:
            return functionNode->data.classMetaFunction.isStatic;
        case ZR_AST_STRUCT_METHOD:
            return functionNode->data.structMethod.isStatic;
        case ZR_AST_STRUCT_META_FUNCTION:
            return functionNode->data.structMetaFunction.isStatic;
        default:
            return ZR_TRUE;
    }
}

static TZrBool register_canonical_receiver_binding(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *functionNode,
        const TZrChar *literalName,
        SZrString *typeName) {
    SZrCompilerState *compilerState;
    SZrString *name;
    SZrSymbol *symbol;
    SZrInferredType typeInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || functionNode == ZR_NULL ||
        literalName == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL || compilerState->typeEnv == ZR_NULL ||
        analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    name = ZrCore_String_Create(
            state, (TZrNativeString)literalName, strlen(literalName));
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }

    symbol = ZrLanguageServer_SymbolTable_LookupAtPosition(
            analyzer->symbolTable, name, functionNode->location);
    if (symbol == ZR_NULL ||
        symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        symbol->selectionRange.source == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_InitFull(
            state, &typeInfo, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, typeName);
    if (!ZrParser_TypeEnvironment_RegisterCanonicalVariable(
                state,
                compilerState->typeEnv,
                name,
                &typeInfo,
                symbol->semanticId,
                symbol->semanticTypeId,
                symbol->selectionRange)) {
        ZrParser_InferredType_Free(state, &typeInfo);
        return ZR_FALSE;
    }

    ZrParser_InferredType_Free(state, &typeInfo);
    return ZR_TRUE;
}

void ZrLanguageServer_SemanticAnalyzer_RegisterTypecheckReceiverBindings(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *functionNode) {
    SZrCompilerState *compilerState;
    SZrTypePrototypeInfo *prototype;

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        callable_has_static_receiver(functionNode)) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL ||
        compilerState->currentTypeNode == ZR_NULL ||
        compilerState->currentTypeName == ZR_NULL) {
        return;
    }

    (void)register_canonical_receiver_binding(
            state, analyzer, functionNode, "this", compilerState->currentTypeName);

    prototype = compilerState->currentTypePrototypeInfo;
    if (compilerState->currentTypeNode->type == ZR_AST_CLASS_DECLARATION &&
        prototype != ZR_NULL && prototype->extendsTypeName != ZR_NULL) {
        (void)register_canonical_receiver_binding(
                state, analyzer, functionNode, "super", prototype->extendsTypeName);
    }
}
