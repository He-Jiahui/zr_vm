#include "type_inference_internal.h"

#include "compiler_internal.h"
#include "compiler_type_declaration.h"
#include "zr_vm_parser/canonical_type.h"

TZrBool type_inference_bind_nominal_identity(SZrCompilerState *cs,
        SZrString *typeName, SZrSyntaxContext syntaxContext, SZrInferredType *result) {
    const SZrNamedTypeBinding *binding;
    const SZrCanonicalTypeNode *type;
    SZrSemanticContext *semanticContext;
    SZrAstNode *declaration;

    if (cs == ZR_NULL || typeName == ZR_NULL || result == ZR_NULL ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_FALSE;
    }
    if (ZrParser_SyntaxContext_Equal(
                ZrParser_SyntaxContext_Normalize(syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM),
                ZrParser_SyntaxContext_Root())) {
        return ZR_TRUE;
    }

    binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
            cs->typeEnv, typeName, syntaxContext);
    if (binding == ZR_NULL) {
        declaration = find_type_declaration_with_context(cs, typeName, syntaxContext);
        if (declaration == ZR_NULL || cs->typeEnv == ZR_NULL) {
            return ZR_TRUE;
        }
        if (!ZrParser_TypeEnvironment_RegisterTypeDeclaration(
                    cs->state, cs->typeEnv, typeName, declaration)) {
            return ZR_FALSE;
        }
        binding = ZrParser_TypeEnvironment_FindTypeBindingWithContext(
                cs->typeEnv, typeName, syntaxContext);
    }

    semanticContext = cs->typeEnv != ZR_NULL ? cs->typeEnv->semanticContext : ZR_NULL;
    if (semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }
    type = binding != ZR_NULL
            ? ZrParser_CanonicalType_Find(semanticContext, binding->typeId) : ZR_NULL;
    if (type == ZR_NULL || type->kind != ZR_CANONICAL_TYPE_NOMINAL) {
        ZrParser_Compiler_Error(cs, "Cannot preserve nominal declaration identity",
                cs->scriptAst != ZR_NULL ? cs->scriptAst->location : (SZrFileRange){0});
        return ZR_FALSE;
    }
    result->nominalDefinitionToken = type->data.nominal.definitionToken;
    return ZR_TRUE;
}
