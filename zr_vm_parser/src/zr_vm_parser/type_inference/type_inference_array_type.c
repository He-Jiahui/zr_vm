#include "type_inference_array_type.h"

#include "type_inference_internal.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/syntax_context.h"

static TZrBool array_type_contexts_equal(
        SZrCompilerState *cs, SZrSyntaxContext left, SZrSyntaxContext right,
        EZrSyntaxNamespace namespaceKind, SZrFileRange location,
        TZrBool *equal) {
    SZrSyntaxContext normalizedLeft;
    SZrSyntaxContext normalizedRight;

    *equal = ZR_FALSE;
    if (!ZrParser_SyntaxContext_IsValid(left) ||
        !ZrParser_SyntaxContext_IsValid(right)) {
        ZrParser_Compiler_Error(cs, "Invalid array size expression context", location);
        return ZR_FALSE;
    }
    normalizedLeft = ZrParser_SyntaxContext_Normalize(left, namespaceKind);
    normalizedRight = ZrParser_SyntaxContext_Normalize(right, namespaceKind);
    if (!ZrParser_SyntaxContext_IsValid(normalizedLeft) ||
        !ZrParser_SyntaxContext_IsValid(normalizedRight)) {
        ZrParser_Compiler_Error(cs, "Failed to normalize array size expression context", location);
        return ZR_FALSE;
    }
    *equal = ZrParser_SyntaxContext_Equal(normalizedLeft, normalizedRight);
    return ZR_TRUE;
}

static const SZrGenericDeclaration *array_type_owner_generic(
        const SZrAstNode *owner) {
    if (owner == ZR_NULL) {
        return ZR_NULL;
    }
    switch (owner->type) {
        case ZR_AST_FUNCTION_DECLARATION: return owner->data.functionDeclaration.generic;
        case ZR_AST_CLASS_METHOD: return owner->data.classMethod.generic;
        case ZR_AST_STRUCT_METHOD: return owner->data.structMethod.generic;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE: return owner->data.interfaceMethodSignature.generic;
        case ZR_AST_CLASS_DECLARATION: return owner->data.classDeclaration.generic;
        case ZR_AST_STRUCT_DECLARATION: return owner->data.structDeclaration.generic;
        case ZR_AST_INTERFACE_DECLARATION: return owner->data.interfaceDeclaration.generic;
        case ZR_AST_UNION_DECLARATION: return owner->data.unionDeclaration.generic;
        default: return ZR_NULL;
    }
}

static TZrBool array_type_can_defer_extent(
        SZrCompilerState *cs, const SZrAstNode *expression, TZrBool *canDefer) {
    const SZrAstNode *owners[2] = {cs->currentFunctionNode, cs->currentTypeNode};
    const SZrIdentifier *identifier;

    *canDefer = ZR_FALSE;
    if (expression->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_TRUE;
    }
    identifier = &expression->data.identifier;
    if (identifier->name == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!ZrParser_SyntaxContext_IsValid(identifier->syntaxContext)) {
        ZrParser_Compiler_Error(cs, "Invalid array size expression context", expression->location);
        return ZR_FALSE;
    }

    /* The legacy presence lookup merges absence and normalization failure.
     * Keep them distinct before allowing an unevaluated generic extent. */
    for (TZrSize index = 0U; index < cs->compileTimeVariables.length; index++) {
        SZrCompileTimeVariable *const *variable =
                ZrCore_Array_Get(&cs->compileTimeVariables, index);
        TZrBool equal;
        if (variable == ZR_NULL || *variable == ZR_NULL || (*variable)->name == ZR_NULL ||
            !ZrCore_String_Equal((*variable)->name, identifier->name)) {
            continue;
        }
        if (!array_type_contexts_equal(cs, (*variable)->syntaxContext,
                    identifier->syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM,
                    expression->location, &equal)) {
            return ZR_FALSE;
        }
        if (equal) {
            return ZR_TRUE;
        }
    }

    for (TZrSize ownerIndex = 0U; ownerIndex < 2U; ownerIndex++) {
        const SZrGenericDeclaration *generic = array_type_owner_generic(owners[ownerIndex]);
        if (generic == ZR_NULL || generic->params == ZR_NULL) {
            continue;
        }
        for (TZrSize index = 0U; index < generic->params->count; index++) {
            const SZrAstNode *parameterNode = generic->params->nodes[index];
            const SZrParameter *parameter;
            TZrBool equal;
            if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
                continue;
            }
            parameter = &parameterNode->data.parameter;
            if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL ||
                !ZrCore_String_Equal(parameter->name->name, identifier->name)) {
                continue;
            }
            if (!array_type_contexts_equal(cs, parameter->name->syntaxContext,
                        identifier->syntaxContext, ZR_SYNTAX_NAMESPACE_LOCAL,
                        expression->location, &equal)) {
                return ZR_FALSE;
            }
            if (equal) {
                *canDefer = parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT;
                return ZR_TRUE;
            }
        }
    }
    return ZR_TRUE;
}

TZrBool type_inference_resolve_array_type_extent(
        SZrCompilerState *cs, const SZrType *astType, SZrArrayTypeExtent *extent) {
    TZrBool canDefer;

    if (cs == ZR_NULL || astType == ZR_NULL || extent == ZR_NULL) {
        return ZR_FALSE;
    }
    extent->fixedSize = 0;
    extent->minSize = 0;
    extent->maxSize = 0;
    extent->hasFixedSize = ZR_FALSE;
    if (!astType->hasArraySizeConstraint) {
        return ZR_TRUE;
    }
    if (astType->arraySizeExpression != ZR_NULL) {
        if (!array_type_can_defer_extent(cs, astType->arraySizeExpression, &canDefer)) {
            return ZR_FALSE;
        }
        if (canDefer) {
            /* The source AST retains the expression; numeric bounds stay unresolved. */
            return ZR_TRUE;
        }
        if (!resolve_compile_time_array_size(cs, astType, &extent->fixedSize)) {
            return ZR_FALSE;
        }
        extent->minSize = extent->fixedSize;
        extent->maxSize = extent->fixedSize;
        extent->hasFixedSize = ZR_TRUE;
    } else {
        extent->fixedSize = astType->arrayFixedSize;
        extent->minSize = astType->arrayMinSize;
        extent->maxSize = astType->arrayMaxSize;
        extent->hasFixedSize = extent->fixedSize > 0 ||
                (extent->minSize > 0 && extent->minSize == extent->maxSize);
        if (!extent->hasFixedSize && extent->minSize > 0 && extent->minSize == extent->maxSize) {
            extent->fixedSize = extent->minSize;
            extent->hasFixedSize = ZR_TRUE;
        }
    }
    return ZR_TRUE;
}
