#include "semantic_type_publication.h"

#include "semantic_context_ownership.h"
#include "semantic/type_declaration_context.h"
#include "zr_vm_core/string.h"

const SZrCanonicalTypeNode *semantic_type_nominal_definition(
        const SZrSemanticContext *context, TZrTypeId typeId) {
    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize depth = 0U; depth < context->canonicalTypes.length; depth++) {
        const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(context, typeId);

        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        if (type->kind == ZR_CANONICAL_TYPE_NOMINAL) {
            return type;
        }
        if (type->kind == ZR_CANONICAL_TYPE_UNION) {
            typeId = type->data.unionType.definitionTypeId;
        } else if (type->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
            typeId = type->data.genericInstance.definitionTypeId;
        } else {
            return ZR_NULL;
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_Semantic_PublishCanonicalTypeSymbol(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrSemanticTypeKind typeKind,
        SZrString *name,
        SZrAstNode *astNode,
        TZrSymbolId symbolId,
        SZrFileRange location) {
    SZrSemanticTypeRecord typeRecord = {0};
    SZrSemanticSymbolRecord symbolRecord = {0};
    SZrSyntaxContext syntaxContext;
    SZrSyntaxContext normalizedContext;
    const SZrCanonicalTypeNode *definition;

    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID ||
        symbolId == ZR_SEMANTIC_ID_INVALID || name == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL ||
        !type_declaration_raw_context(astNode, &syntaxContext) ||
        !type_declaration_normalize_context(syntaxContext, &normalizedContext)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < context->types.length; index++) {
        const SZrSemanticTypeRecord *existing = ZrCore_Array_Get(&context->types, index);

        if (existing != ZR_NULL && existing->id == typeId) {
            return ZR_FALSE;
        }
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *existing = ZrCore_Array_Get(&context->symbols, index);
        SZrSyntaxContext existingContext;

        if (existing == ZR_NULL) {
            continue;
        }
        if (existing->id == symbolId) {
            return ZR_FALSE;
        }
        if (existing->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE || existing->name == ZR_NULL ||
            !ZrCore_String_Equal(existing->name, name)) {
            continue;
        }
        if (!type_declaration_normalize_context(existing->syntaxContext, &existingContext) ||
            ZrParser_SyntaxContext_Equal(existingContext, normalizedContext)) {
            return ZR_FALSE;
        }
    }
    if (!semantic_context_retain_syntax_table(context, syntaxContext)) {
        return ZR_FALSE;
    }

    typeRecord.id = typeId;
    typeRecord.kind = typeKind;
    typeRecord.baseType = ZR_VALUE_TYPE_OBJECT;
    typeRecord.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    typeRecord.name = name;
    typeRecord.astNode = astNode;
    ZrParser_InferredType_InitFull(
            context->state, &typeRecord.inferredType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, name);
    definition = semantic_type_nominal_definition(context, typeId);
    if (definition != ZR_NULL) {
        typeRecord.inferredType.nominalDefinitionToken = definition->data.nominal.definitionToken;
    }

    symbolRecord.id = symbolId;
    symbolRecord.kind = ZR_SEMANTIC_SYMBOL_KIND_TYPE;
    symbolRecord.name = name;
    symbolRecord.typeId = typeId;
    symbolRecord.overloadSetId = ZR_SEMANTIC_ID_INVALID;
    symbolRecord.astNode = astNode;
    symbolRecord.location = location;
    symbolRecord.syntaxContext = syntaxContext;

    ZrCore_Array_Push(context->state, &context->types, &typeRecord);
    ZrCore_Array_Push(context->state, &context->symbols, &symbolRecord);
    return ZR_TRUE;
}
