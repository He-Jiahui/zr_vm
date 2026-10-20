#include "type_inference_nominal_prototype.h"

#include "compiler_type_declaration.h"
#include "type_inference_generic_prototype.h"
#include "type_inference_internal.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/syntax_context.h"

#include <string.h>

TZrBool type_inference_prototype_is_root_visible(const SZrTypePrototypeInfo *prototype) {
    SZrSyntaxContext context;

    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }
    if (prototype->declarationNode == ZR_NULL) {
        return ZR_TRUE;
    }
    return compiler_type_declaration_item_context(prototype->declarationNode, &context) &&
           ZrParser_SyntaxContext_Equal(context, ZrParser_SyntaxContext_Root());
}

static TZrBool matches_declaration(const SZrTypePrototypeInfo *prototype,
        const SZrInferredType *type, const SZrSemanticSymbolRecord *symbol) {
    SZrSyntaxContext declarationContext;

    return prototype != ZR_NULL && prototype->name != ZR_NULL &&
           prototype->declarationNode == symbol->astNode &&
           ZrCore_String_Equal(prototype->name, type->typeName) &&
           compiler_type_declaration_item_context(symbol->astNode, &declarationContext) &&
           ZrParser_SyntaxContext_Equal(declarationContext,
                   ZrParser_SyntaxContext_Normalize(
                           symbol->syntaxContext, ZR_SYNTAX_NAMESPACE_ITEM));
}

SZrTypePrototypeInfo *type_inference_find_nominal_prototype(
        SZrCompilerState *cs, const SZrInferredType *type) {
    SZrSemanticContext *semanticContext;
    const SZrSemanticSymbolRecord *symbol;
    SZrTypePrototypeInfo *bestMatch = ZR_NULL;

    if (cs == ZR_NULL || type == ZR_NULL || type->typeName == ZR_NULL) {
        return ZR_NULL;
    }
    if (type->baseType == ZR_VALUE_TYPE_OBJECT && type->elementTypes.length > 0U &&
        strchr(ZrCore_String_GetNativeString(type->typeName), '<') != ZR_NULL) {
        return type_inference_find_generic_prototype(cs, type);
    }
    if (type->nominalDefinitionToken == 0U) {
        return find_compiler_type_prototype_inference(cs, type->typeName);
    }
    semanticContext = cs->typeEnv != ZR_NULL
            ? cs->typeEnv->semanticContext : cs->semanticContext;
    symbol = ZrParser_Semantic_FindSymbolById(semanticContext, type->nominalDefinitionToken);
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
        symbol->astNode == ZR_NULL || !ZrParser_SyntaxContext_IsValid(symbol->syntaxContext)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype = ZrCore_Array_Get(&cs->typePrototypes, index);

        if (matches_declaration(prototype, type, symbol)) {
            if (!prototype->isImportedNative) {
                return prototype;
            }
            if (bestMatch == ZR_NULL) {
                bestMatch = prototype;
            }
        }
    }
    if (matches_declaration(cs->currentTypePrototypeInfo, type, symbol) &&
        (bestMatch == ZR_NULL || !cs->currentTypePrototypeInfo->isImportedNative)) {
        return cs->currentTypePrototypeInfo;
    }
    return bestMatch;
}
