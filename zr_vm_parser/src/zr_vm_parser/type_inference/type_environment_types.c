#include "zr_vm_parser/type_system.h"

#include "zr_vm_parser/semantic.h"

static TZrBool type_environment_register_type(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *typeName,
        SZrAstNode *declarationNode) {
    TZrSize index;
    TZrTypeId typeId;
    TZrSymbolId symbolId;
    SZrFileRange location = {0};

    if (state == ZR_NULL || env == ZR_NULL || typeName == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0U; index < env->typeNames.length; index++) {
        SZrString **storedTypeName = (SZrString **)ZrCore_Array_Get(&env->typeNames, index);

        if (storedTypeName != ZR_NULL && *storedTypeName != ZR_NULL &&
            ZrCore_String_Equal(*storedTypeName, typeName)) {
            return ZR_TRUE;
        }
    }

    ZrCore_Array_Push(state, &env->typeNames, &typeName);
    if (env->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    if (declarationNode != ZR_NULL) {
        location = declarationNode->location;
    }
    typeId = ZrParser_Semantic_RegisterNamedType(
            env->semanticContext,
            typeName,
            ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
            declarationNode);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    symbolId = ZrParser_Semantic_RegisterSymbol(
            env->semanticContext,
            typeName,
            ZR_SEMANTIC_SYMBOL_KIND_TYPE,
            typeId,
            ZR_SEMANTIC_ID_INVALID,
            declarationNode,
            location);
    return symbolId != ZR_SEMANTIC_ID_INVALID;
}

TZrBool ZrParser_TypeEnvironment_RegisterType(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *typeName) {
    return type_environment_register_type(state, env, typeName, ZR_NULL);
}

TZrBool ZrParser_TypeEnvironment_RegisterTypeDeclaration(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *typeName,
        SZrAstNode *declarationNode) {
    return type_environment_register_type(state, env, typeName, declarationNode);
}
