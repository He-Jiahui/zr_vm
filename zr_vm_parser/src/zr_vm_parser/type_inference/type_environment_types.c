#include "zr_vm_parser/type_system.h"

#include "semantic/semantic_type_publication.h"
#include "semantic/type_declaration_context.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

static TZrBool find_current_binding(
        const SZrTypeEnvironment *env, SZrString *typeName, SZrSyntaxContext normalizedContext,
        const SZrNamedTypeBinding **outBinding) {
    *outBinding = ZR_NULL;
    for (TZrSize index = env->namedTypeBindings.length; index > 0U; index--) {
        const SZrNamedTypeBinding *binding =
                ZrCore_Array_Get((SZrArray *)&env->namedTypeBindings, index - 1U);
        SZrSyntaxContext bindingContext;

        if (binding == ZR_NULL || binding->name == ZR_NULL ||
            !ZrCore_String_Equal(binding->name, typeName)) {
            continue;
        }
        if (!type_declaration_normalize_context(binding->syntaxContext, &bindingContext)) {
            return ZR_FALSE;
        }
        if (ZrParser_SyntaxContext_Equal(bindingContext, normalizedContext)) {
            *outBinding = binding;
            break;
        }
    }
    return ZR_TRUE;
}

const SZrNamedTypeBinding *ZrParser_TypeEnvironment_FindTypeBindingWithContext(
        const SZrTypeEnvironment *env, SZrString *typeName, SZrSyntaxContext syntaxContext) {
    SZrSyntaxContext normalizedContext;

    if (typeName == ZR_NULL ||
        !type_declaration_normalize_context(syntaxContext, &normalizedContext)) {
        return ZR_NULL;
    }
    for (; env != ZR_NULL; env = env->parent) {
        const SZrNamedTypeBinding *binding;

        if (!find_current_binding(env, typeName, normalizedContext, &binding)) {
            return ZR_NULL;
        }
        if (binding != ZR_NULL) {
            return binding;
        }
    }
    return ZR_NULL;
}

static TZrBool find_published_declaration_symbol(
        const SZrSemanticContext *context, SZrAstNode *declarationNode, SZrString *typeName,
        SZrSyntaxContext normalizedContext, const SZrSemanticSymbolRecord **outSymbol) {
    *outSymbol = ZR_NULL;
    if (declarationNode == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                ZrCore_Array_Get((SZrArray *)&context->symbols, index);
        SZrSyntaxContext symbolContext;

        if (symbol == ZR_NULL || symbol->astNode != declarationNode ||
            symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE || symbol->name == ZR_NULL ||
            !ZrCore_String_Equal(symbol->name, typeName)) {
            continue;
        }
        if (!type_declaration_normalize_context(symbol->syntaxContext, &symbolContext)) {
            return ZR_FALSE;
        }
        if (ZrParser_SyntaxContext_Equal(symbolContext, normalizedContext)) {
            *outSymbol = symbol;
            break;
        }
    }
    return ZR_TRUE;
}

static TZrBool register_type_with_context_and_kind(
        SZrState *state, SZrTypeEnvironment *env, SZrString *typeName,
        SZrAstNode *declarationNode, SZrSyntaxContext syntaxContext,
        EZrSemanticTypeKind typeKind) {
    SZrNamedTypeBinding binding = {0};
    SZrFileRange location = {0};
    SZrSyntaxContext normalizedContext;
    const SZrNamedTypeBinding *existingBinding;
    TZrBool isRoot;

    if (state == ZR_NULL || env == ZR_NULL || typeName == ZR_NULL ||
        !type_declaration_normalize_context(syntaxContext, &normalizedContext) ||
        !find_current_binding(env, typeName, normalizedContext, &existingBinding)) {
        return ZR_FALSE;
    }
    if (existingBinding != ZR_NULL) {
        return ZR_TRUE;
    }
    isRoot = ZrParser_SyntaxContext_Equal(normalizedContext, ZrParser_SyntaxContext_Root());
    binding.name = typeName;
    binding.syntaxContext = syntaxContext;
    binding.declarationNode = declarationNode;
    if (env->semanticContext != ZR_NULL) {
        const SZrSemanticSymbolRecord *publishedSymbol;

        if (declarationNode != ZR_NULL) {
            location = declarationNode->location;
        }
        if (!find_published_declaration_symbol(env->semanticContext, declarationNode,
                    typeName, normalizedContext, &publishedSymbol)) {
            return ZR_FALSE;
        }
        if (publishedSymbol != ZR_NULL) {
            const SZrCanonicalTypeNode *definition = semantic_type_nominal_definition(
                    env->semanticContext, publishedSymbol->typeId);

            if (definition == ZR_NULL) {
                return ZR_FALSE;
            }
            /* The symbol keeps its projected type; the named binding supplies
             * the nominal root consumed by annotation identity inference. */
            binding.symbolId = publishedSymbol->id;
            binding.typeId = definition->id;
        } else {
            binding.symbolId = ZrParser_Semantic_ReserveSymbolId(env->semanticContext);
            if (binding.symbolId == ZR_SEMANTIC_ID_INVALID) {
                return ZR_FALSE;
            }
            if (isRoot) {
                binding.typeId = ZrParser_Semantic_RegisterNamedType(
                        env->semanticContext, typeName, typeKind,
                        declarationNode);
            } else {
                binding.typeId = ZrParser_CanonicalType_InternNominal(
                        env->semanticContext, ZR_NULL, typeName, binding.symbolId);
                if (binding.typeId == ZR_SEMANTIC_ID_INVALID ||
                    !ZrParser_Semantic_RegisterCanonicalType(env->semanticContext,
                            binding.typeId, typeKind,
                            typeName, declarationNode)) {
                    return ZR_FALSE;
                }
            }
            if (binding.typeId == ZR_SEMANTIC_ID_INVALID ||
                ZrParser_Semantic_RegisterSymbolWithIdAndContext(env->semanticContext,
                        binding.symbolId, typeName, ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                        binding.typeId, ZR_SEMANTIC_ID_INVALID, declarationNode,
                        location, syntaxContext) == ZR_SEMANTIC_ID_INVALID) {
                return ZR_FALSE;
            }
        }
    }
    ZrCore_Array_Push(state, &env->namedTypeBindings, &binding);
    if (isRoot) {
        ZrCore_Array_Push(state, &env->typeNames, &typeName);
    }
    return ZR_TRUE;
}

TZrBool ZrParser_TypeEnvironment_RegisterTypeWithContext(
        SZrState *state, SZrTypeEnvironment *env, SZrString *typeName,
        SZrAstNode *declarationNode, SZrSyntaxContext syntaxContext) {
    return register_type_with_context_and_kind(
            state, env, typeName, declarationNode, syntaxContext,
            ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
}

TZrBool ZrParser_TypeEnvironment_RegisterType(
        SZrState *state, SZrTypeEnvironment *env, SZrString *typeName) {
    return ZrParser_TypeEnvironment_RegisterTypeWithContext(
            state, env, typeName, ZR_NULL, ZrParser_SyntaxContext_Root());
}

TZrBool ZrParser_TypeEnvironment_RegisterTypeDeclaration(
        SZrState *state, SZrTypeEnvironment *env, SZrString *typeName,
        SZrAstNode *declarationNode) {
    return ZrParser_TypeEnvironment_RegisterTypeDeclarationWithKind(
            state, env, typeName, declarationNode, ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
}

TZrBool ZrParser_TypeEnvironment_RegisterTypeDeclarationWithKind(
        SZrState *state, SZrTypeEnvironment *env, SZrString *typeName,
        SZrAstNode *declarationNode, EZrSemanticTypeKind typeKind) {
    SZrSyntaxContext syntaxContext;

    if (!type_declaration_raw_context(declarationNode, &syntaxContext)) {
        return ZR_FALSE;
    }
    return register_type_with_context_and_kind(
            state, env, typeName, declarationNode, syntaxContext, typeKind);
}

TZrBool ZrParser_TypeEnvironment_LookupTypeWithContext(
        SZrTypeEnvironment *env, SZrString *typeName, SZrSyntaxContext syntaxContext) {
    SZrSyntaxContext normalizedContext;
    TZrBool isRoot;

    if (env == ZR_NULL || typeName == ZR_NULL ||
        !type_declaration_normalize_context(syntaxContext, &normalizedContext)) {
        return ZR_FALSE;
    }
    isRoot = ZrParser_SyntaxContext_Equal(normalizedContext, ZrParser_SyntaxContext_Root());
    for (; env != ZR_NULL; env = env->parent) {
        const SZrNamedTypeBinding *binding;

        if (!find_current_binding(env, typeName, normalizedContext, &binding)) {
            return ZR_FALSE;
        }
        if (binding != ZR_NULL) {
            return ZR_TRUE;
        }
        if (isRoot) {
            for (TZrSize index = 0U; index < env->typeNames.length; index++) {
                SZrString **name = ZrCore_Array_Get(&env->typeNames, index);

                if (name != ZR_NULL && *name != ZR_NULL && ZrCore_String_Equal(*name, typeName)) {
                    return ZR_TRUE;
                }
            }
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_TypeEnvironment_LookupType(SZrTypeEnvironment *env, SZrString *typeName) {
    return ZrParser_TypeEnvironment_LookupTypeWithContext(
            env, typeName, ZrParser_SyntaxContext_Root());
}
