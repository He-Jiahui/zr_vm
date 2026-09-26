#include "zr_vm_parser/type_system.h"
#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"
#include "type_environment_declaration_binding.h"

#include <string.h>

static TZrSymbolId type_environment_register_variable_declaration(
        SZrSemanticContext *context,
        SZrString *name,
        TZrTypeId typeId,
        SZrAstNode *declarationNode,
        SZrFileRange declarationRange) {
    EZrSemanticSymbolKind kind = declarationNode != ZR_NULL &&
                                        declarationNode->type == ZR_AST_PARAMETER
                                ? ZR_SEMANTIC_SYMBOL_KIND_PARAMETER
                                : ZR_SEMANTIC_SYMBOL_KIND_VARIABLE;
    const SZrSemanticSymbolRecord *existing =
            type_environment_find_declaration_symbol(
                    context, declarationNode, name, kind);

    if (existing != ZR_NULL) {
        if (existing->typeId != typeId && typeId != ZR_SEMANTIC_ID_INVALID &&
            !ZrParser_Semantic_RebindSymbolType(context, existing->id, typeId)) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        return existing->id;
    }
    return ZrParser_Semantic_RegisterSymbol(
            context, name, kind, typeId, ZR_SEMANTIC_ID_INVALID,
            declarationNode, declarationRange);
}

static void type_environment_append_variable_declaration_fact(
        SZrSemanticContext *semanticContext,
        SZrAstNode *declarationNode,
        SZrFileRange declarationRange,
        SZrString *name,
        const SZrInferredType *type,
        TZrSymbolId symbolId,
        TZrTypeId typeId) {
    SZrSemanticReferenceFact declarationFact;

    if (semanticContext == ZR_NULL) {
        return;
    }
    if (declarationNode != ZR_NULL && symbolId != ZR_SEMANTIC_ID_INVALID) {
        for (TZrSize index = 0U; index < semanticContext->referenceFacts.length; index++) {
            SZrSemanticReferenceFact *existing =
                    (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                            &semanticContext->referenceFacts, index);
            if (existing != ZR_NULL &&
                existing->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
                existing->node == declarationNode && existing->symbolId == symbolId) {
                existing->typeId = typeId;
                existing->ownershipQualifier = type->ownershipQualifier;
                return;
            }
        }
    }
    memset(&declarationFact, 0, sizeof(declarationFact));
    declarationFact.node = declarationNode;
    declarationFact.range = declarationRange;
    declarationFact.declarationRange = declarationRange;
    declarationFact.definitionRange = declarationRange;
    declarationFact.hasDefinitionRange = ZR_TRUE;
    declarationFact.kind = ZR_SEMANTIC_REFERENCE_DECLARATION;
    declarationFact.symbolId = symbolId;
    declarationFact.typeId = typeId;
    declarationFact.ownershipQualifier = type->ownershipQualifier;
    declarationFact.name = name;
    declarationFact.isResolved = ZR_TRUE;
    ZrParser_SemanticFacts_AppendReference(semanticContext, &declarationFact);
}

// 注册变量类型
TZrBool ZrParser_TypeEnvironment_RegisterVariableEx(SZrState *state,
                                                    SZrTypeEnvironment *env,
                                                    SZrString *name,
                                                    const SZrInferredType *type,
                                                    SZrAstNode *declarationNode,
                                                    SZrFileRange declarationRange) {
    TZrTypeId typeId;
    TZrSymbolId symbolId;
    SZrFileRange location = {0};
    TZrBool hasDeclarationRange = declarationRange.source != ZR_NULL ||
                                  declarationRange.start.line != 0 ||
                                  declarationRange.start.column != 0 ||
                                  declarationRange.start.offset != 0 ||
                                  declarationRange.end.line != 0 ||
                                  declarationRange.end.column != 0 ||
                                  declarationRange.end.offset != 0;

    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    if (hasDeclarationRange) {
        location = declarationRange;
    }

    typeId = ZR_SEMANTIC_ID_INVALID;
    symbolId = ZR_SEMANTIC_ID_INVALID;
    if (env->semanticContext != ZR_NULL) {
        /* A source declaration can be known before its exact type. Keep its
         * identity without publishing the inference sentinel as `object`. */
        if (declarationNode == ZR_NULL || type->baseType != ZR_VALUE_TYPE_OBJECT ||
            type->typeName != ZR_NULL || type->elementTypes.length != 0U ||
            (declarationNode->type == ZR_AST_VARIABLE_DECLARATION &&
             declarationNode->data.variableDeclaration.typeInfo != ZR_NULL)) {
            typeId = ZrParser_Semantic_RegisterInferredType(
                    env->semanticContext,
                    type,
                    ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                    type->typeName,
                    declarationNode);
        }
        if (typeId == ZR_SEMANTIC_ID_INVALID && declarationNode == ZR_NULL) {
            return ZR_FALSE;
        }
    }

    // 检查是否已存在
    for (TZrSize i = 0; i < env->variableTypes.length; i++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&env->variableTypes, i);
        if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
            if (env->semanticContext != ZR_NULL) {
                if (hasDeclarationRange) {
                    symbolId = type_environment_register_variable_declaration(
                            env->semanticContext,
                            name,
                            typeId,
                            declarationNode,
                            declarationRange);
                    if (symbolId == ZR_SEMANTIC_ID_INVALID) {
                        return ZR_FALSE;
                    }
                } else if (binding->symbolId != ZR_SEMANTIC_ID_INVALID &&
                           !ZrParser_Semantic_RebindSymbolType(
                                   env->semanticContext,
                                   binding->symbolId,
                                   typeId)) {
                    return ZR_FALSE;
                }
            }
            // 已存在，更新类型
            ZrParser_InferredType_Free(state, &binding->type);
            ZrParser_InferredType_Copy(state, &binding->type, type);
            binding->typeId = typeId;
            binding->placeId = 0;
            binding->originKind = ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION;
            binding->runtimeRootKind = ZR_SEMANTIC_RUNTIME_ROOT_NONE;
            binding->originToken = 0u;
            binding->originIndex = 0u;
            if (hasDeclarationRange) {
                binding->declarationRange = declarationRange;
                binding->hasDeclarationRange = ZR_TRUE;
                binding->symbolId = symbolId;
                type_environment_append_variable_declaration_fact(
                        env->semanticContext,
                        declarationNode,
                        declarationRange,
                        name,
                        type,
                        symbolId,
                        typeId);
            }
            return ZR_TRUE;
        }
    }

    // 创建新的绑定
    SZrTypeBinding binding;
    binding.name = name;
    binding.declarationRange = declarationRange;
    binding.hasDeclarationRange = hasDeclarationRange;
    binding.typeId = ZR_SEMANTIC_ID_INVALID;
    binding.symbolId = ZR_SEMANTIC_ID_INVALID;
    binding.placeId = 0;
    binding.originKind = ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION;
    binding.runtimeRootKind = ZR_SEMANTIC_RUNTIME_ROOT_NONE;
    binding.originToken = 0u;
    binding.originIndex = 0u;
    ZrParser_InferredType_Copy(state, &binding.type, type);

    if (env->semanticContext != ZR_NULL) {
        symbolId = type_environment_register_variable_declaration(env->semanticContext,
                                            name,
                                            typeId,
                                            declarationNode,
                                            location);
        if (symbolId == ZR_SEMANTIC_ID_INVALID) {
            ZrParser_InferredType_Free(state, &binding.type);
            return ZR_FALSE;
        }
        binding.typeId = typeId;
        binding.symbolId = symbolId;

        if (hasDeclarationRange) {
            type_environment_append_variable_declaration_fact(
                    env->semanticContext,
                    declarationNode,
                    declarationRange,
                    name,
                    type,
                    symbolId,
                    typeId);
        }
    }

    ZrCore_Array_Push(state, &env->variableTypes, &binding);
    return ZR_TRUE;
}

TZrBool ZrParser_TypeEnvironment_RegisterCanonicalVariable(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *name,
        const SZrInferredType *type,
        TZrSymbolId symbolId,
        TZrTypeId typeId,
        SZrFileRange declarationRange) {
    return ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(state,
                                                                        env,
                                                                        name,
                                                                        type,
                                                                        symbolId,
                                                                        typeId,
                                                                        0,
                                                                        declarationRange);
}

TZrBool ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace(
        SZrState *state,
        SZrTypeEnvironment *env,
        SZrString *name,
        const SZrInferredType *type,
        TZrSymbolId symbolId,
        TZrTypeId typeId,
        TZrUInt32 placeId,
        SZrFileRange declarationRange) {
    SZrTypeBinding binding;
    TZrBool hasDeclarationRange;

    hasDeclarationRange = declarationRange.source != ZR_NULL ||
                          declarationRange.start.line != 0 ||
                          declarationRange.start.column != 0 ||
                          declarationRange.start.offset != 0 ||
                          declarationRange.end.line != 0 ||
                          declarationRange.end.column != 0 ||
                          declarationRange.end.offset != 0;
    if (state == ZR_NULL || env == ZR_NULL || name == ZR_NULL || type == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        !hasDeclarationRange) {
        return ZR_FALSE;
    }
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        const SZrSemanticSymbolRecord *record =
                ZrParser_Semantic_FindSymbolById(env->semanticContext, symbolId);
        if (record == ZR_NULL || record->astNode == ZR_NULL ||
            record->typeId != ZR_SEMANTIC_ID_INVALID) {
            return ZR_FALSE;
        }
    }

    for (TZrSize i = 0; i < env->variableTypes.length; i++) {
        SZrTypeBinding *existing = (SZrTypeBinding *)ZrCore_Array_Get(&env->variableTypes, i);

        if (existing == ZR_NULL || existing->name == ZR_NULL ||
            !ZrCore_String_Equal(existing->name, name)) {
            continue;
        }

        ZrParser_InferredType_Free(state, &existing->type);
        ZrParser_InferredType_Copy(state, &existing->type, type);
        existing->declarationRange = declarationRange;
        existing->hasDeclarationRange = ZR_TRUE;
        existing->symbolId = symbolId;
        existing->typeId = typeId;
        existing->placeId = placeId;
        existing->originKind = ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION;
        existing->runtimeRootKind = ZR_SEMANTIC_RUNTIME_ROOT_NONE;
        existing->originToken = 0u;
        existing->originIndex = 0u;
        return ZR_TRUE;
    }

    binding.name = name;
    binding.declarationRange = declarationRange;
    binding.hasDeclarationRange = ZR_TRUE;
    binding.typeId = typeId;
    binding.symbolId = symbolId;
    binding.placeId = placeId;
    binding.originKind = ZR_SEMANTIC_REFERENCE_ORIGIN_SOURCE_DECLARATION;
    binding.runtimeRootKind = ZR_SEMANTIC_RUNTIME_ROOT_NONE;
    binding.originToken = 0u;
    binding.originIndex = 0u;
    ZrParser_InferredType_Copy(state, &binding.type, type);
    ZrCore_Array_Push(state, &env->variableTypes, &binding);
    return ZR_TRUE;
}

