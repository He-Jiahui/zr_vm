#include "compiler_type_value_alias.h"

#include "zr_vm_core/string.h"
#include "zr_vm_parser/type_inference.h"

static TZrSize current_alias_start(SZrCompilerState *cs) {
    SZrScope *scope;

    if (cs->scopeStack.length == 0U) {
        return 0U;
    }
    scope = ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1U);
    return scope != ZR_NULL ? scope->typeValueAliasStartIndex : 0U;
}

static SZrTypeBinding *find_alias_from(SZrCompilerState *cs,
        SZrString *name, SZrSyntaxContext syntaxContext, TZrSize firstIndex) {
    SZrSyntaxContext normalized;

    if (cs == ZR_NULL || name == ZR_NULL || !cs->typeValueAliases.isValid ||
        !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return ZR_NULL;
    }
    normalized = ZrParser_SyntaxContext_Normalize(syntaxContext, ZR_SYNTAX_NAMESPACE_LOCAL);
    for (TZrSize index = cs->typeValueAliases.length; index > firstIndex; index--) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeValueAliases, index - 1U);
        if (binding != ZR_NULL && binding->name != ZR_NULL &&
            ZrCore_String_Equal(binding->name, name) &&
            ZrParser_SyntaxContext_Equal(normalized, ZrParser_SyntaxContext_Normalize(
                    binding->syntaxContext, ZR_SYNTAX_NAMESPACE_LOCAL))) {
            return binding;
        }
    }
    return ZR_NULL;
}

SZrTypeBinding *compiler_find_type_value_alias(SZrCompilerState *cs,
        SZrString *name, SZrSyntaxContext syntaxContext) {
    return find_alias_from(cs, name, syntaxContext, 0U);
}

SZrTypeBinding *compiler_find_current_type_value_alias(SZrCompilerState *cs,
        SZrString *name, SZrSyntaxContext syntaxContext) {
    return cs != ZR_NULL
                   ? find_alias_from(cs, name, syntaxContext, current_alias_start(cs))
                   : ZR_NULL;
}

void compiler_truncate_type_value_aliases(SZrCompilerState *cs, TZrSize count) {
    if (cs == ZR_NULL || !cs->typeValueAliases.isValid) {
        return;
    }
    while (cs->typeValueAliases.length > count) {
        TZrSize index = cs->typeValueAliases.length - 1U;
        SZrTypeBinding *binding = ZrCore_Array_Get(&cs->typeValueAliases, index);

        if (binding != ZR_NULL) {
            ZrParser_InferredType_Free(cs->state, &binding->type);
        }
        cs->typeValueAliases.length = index;
    }
}

void compiler_register_type_value_alias(SZrCompilerState *cs,
        SZrString *name, const SZrInferredType *type, SZrSyntaxContext syntaxContext) {
    SZrTypeBinding *binding;

    if (cs == ZR_NULL || cs->state == ZR_NULL || name == ZR_NULL || type == ZR_NULL ||
        !cs->typeValueAliases.isValid || !ZrParser_SyntaxContext_IsValid(syntaxContext)) {
        return;
    }
    binding = compiler_find_current_type_value_alias(cs, name, syntaxContext);
    if (binding != ZR_NULL) {
        ZrParser_InferredType_Free(cs->state, &binding->type);
        ZrParser_InferredType_Copy(cs->state, &binding->type, type);
    } else {
        SZrTypeBinding newBinding = {0};
        newBinding.name = name;
        newBinding.syntaxContext = syntaxContext;
        ZrParser_InferredType_Init(cs->state, &newBinding.type, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Copy(cs->state, &newBinding.type, type);
        ZrCore_Array_Push(cs->state, &cs->typeValueAliases, &newBinding);
    }
}

void compiler_register_type_value_alias_literal(SZrCompilerState *cs,
        SZrString *name, SZrAstNode *value, SZrSyntaxContext syntaxContext) {
    SZrInferredType aliasType;

    if (cs == ZR_NULL || cs->state == ZR_NULL || name == ZR_NULL || value == ZR_NULL ||
        value->type != ZR_AST_TYPE_LITERAL_EXPRESSION ||
        value->data.typeLiteralExpression.typeInfo == ZR_NULL) {
        return;
    }
    ZrParser_InferredType_Init(cs->state, &aliasType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_AstTypeToInferredType_Convert(cs, value->data.typeLiteralExpression.typeInfo, &aliasType)) {
        compiler_register_type_value_alias(cs, name, &aliasType, syntaxContext);
    }
    ZrParser_InferredType_Free(cs->state, &aliasType);
}
