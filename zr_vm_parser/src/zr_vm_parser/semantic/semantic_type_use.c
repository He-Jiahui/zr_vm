#include "zr_vm_parser/semantic_type_use.h"

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/semantic_query.h"

#include <limits.h>
#include <string.h>

static TZrBool type_use_reference_range(
        const SZrType *typeUse,
        SZrFileRange *range,
        SZrString **name) {
    const SZrAstNode *node = typeUse->name;
    TZrSize length;

    *range = node->location;
    *name = ZR_NULL;
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        *name = node->data.identifier.name;
    } else if (node->type == ZR_AST_GENERIC_TYPE &&
               node->data.genericType.name != ZR_NULL) {
        *name = node->data.genericType.name->name;
        *range = node->data.genericType.wholeRange;
        length = *name != ZR_NULL ? ZrCore_String_GetByteLength(*name) : 0U;
        if (range->source == ZR_NULL || node->location.source == ZR_NULL ||
            !ZrCore_String_Equal(range->source, node->location.source) ||
            length == 0U || range->end.offset < range->start.offset ||
            length > range->end.offset - range->start.offset ||
            range->start.column < 0 || length > (TZrSize)INT_MAX ||
            range->start.column > INT_MAX - (TZrInt32)length) {
            return ZR_FALSE;
        }
        range->end = range->start;
        range->end.offset += length;
        range->end.column += (TZrInt32)length;
    } else {
        return ZR_FALSE;
    }
    return *name != ZR_NULL && range->source != ZR_NULL &&
           range->end.offset > range->start.offset;
}

static const SZrSemanticSymbolRecord *type_use_declaration(
        const SZrSemanticContext *context,
        TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(context, typeId);
    const SZrSemanticSymbolRecord *result = ZR_NULL;

    if (type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
        typeId = type->data.genericInstance.definitionTypeId;
        type = ZrParser_CanonicalType_Find(context, typeId);
    }
    if (type == ZR_NULL || (type->kind != ZR_CANONICAL_TYPE_NOMINAL &&
                            type->kind != ZR_CANONICAL_TYPE_GENERIC_PARAMETER)) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *candidate =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (candidate == ZR_NULL || candidate->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            candidate->typeId != typeId || candidate->id == ZR_SEMANTIC_ID_INVALID ||
            candidate->astNode == ZR_NULL) {
            continue;
        }
        if (result != ZR_NULL && result->id != candidate->id) {
            return ZR_NULL;
        }
        result = candidate;
    }
    return result;
}

static void type_use_bind_declaration(
        const SZrSemanticContext *context,
        const SZrType *typeUse,
        SZrSemanticReferenceFact *fact) {
    const SZrSemanticSymbolRecord *symbol;
    const SZrSemanticReferenceFact *declaration;
    SZrFileRange range = {0};

    if (!fact->isResolved || typeUse->subType != ZR_NULL) {
        return;
    }
    symbol = type_use_declaration(context, fact->typeId);
    if (symbol == ZR_NULL) {
        return;
    }
    declaration = ZrParser_SemanticQuery_DeclarationOf(context, symbol->id, ZR_NULL);
    if (declaration != ZR_NULL) {
        range = declaration->range;
    } else if (symbol->astNode->type == ZR_AST_CLASS_DECLARATION) {
        range = symbol->astNode->data.classDeclaration.nameLocation;
    }
    if (range.source == ZR_NULL || range.end.offset <= range.start.offset) {
        return;
    }
    fact->symbolId = symbol->id;
    fact->declarationRange = range;
    fact->definitionRange = range;
    fact->hasDefinitionRange = ZR_TRUE;
}

static void type_use_publish_arguments(
        SZrSemanticContext *context,
        const SZrType *typeUse,
        TZrTypeId typeId,
        TZrBool isResolved) {
    const SZrCanonicalTypeNode *type = ZrParser_CanonicalType_Find(context, typeId);
    const SZrGenericType *generic;

    if (!isResolved || typeUse->name->type != ZR_AST_GENERIC_TYPE ||
        type == ZR_NULL || type->kind != ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
        return;
    }
    generic = &typeUse->name->data.genericType;
    if (generic->params == ZR_NULL ||
        generic->params->count != type->data.genericInstance.arguments.length) {
        return;
    }
    for (TZrSize index = 0U; index < generic->params->count; index++) {
        const SZrAstNode *argumentNode = generic->params->nodes[index];
        const SZrCanonicalGenericArgument *argument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&type->data.genericInstance.arguments, index);
        if (argumentNode != ZR_NULL && argumentNode->type == ZR_AST_TYPE &&
            argument != ZR_NULL && argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
            (void)ZrParser_SemanticTypeUse_Publish(
                    context, &argumentNode->data.type, argument->data.typeId, isResolved);
        }
    }
}

TZrBool ZrParser_SemanticTypeUse_Publish(
        SZrSemanticContext *context,
        const SZrType *typeUse,
        TZrTypeId typeId,
        TZrBool isResolved) {
    SZrSemanticReferenceFact fact;

    if (context == ZR_NULL || typeUse == ZR_NULL || typeUse->name == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&fact, 0, sizeof(fact));
    fact.node = typeUse->name;
    fact.typeId = typeId;
    fact.kind = ZR_SEMANTIC_REFERENCE_TYPE;
    fact.isResolved = isResolved;
    if (!type_use_reference_range(typeUse, &fact.range, &fact.name)) {
        return ZR_FALSE;
    }
    type_use_bind_declaration(context, typeUse, &fact);
    for (TZrSize index = 0U; index < context->referenceFacts.length; index++) {
        SZrSemanticReferenceFact *existing = (SZrSemanticReferenceFact *)ZrCore_Array_Get(
                &context->referenceFacts, index);
        if (existing == ZR_NULL || existing->node != fact.node || existing->kind != fact.kind) {
            continue;
        }
        if (existing->typeId != fact.typeId) {
            return ZR_FALSE;
        }
        existing->range = fact.range;
        existing->symbolId = fact.symbolId;
        existing->name = fact.name;
        existing->isResolved = fact.isResolved;
        existing->declarationRange = fact.declarationRange;
        existing->definitionRange = fact.definitionRange;
        existing->hasDefinitionRange = fact.hasDefinitionRange;
        type_use_publish_arguments(context, typeUse, typeId, isResolved);
        return ZR_TRUE;
    }
    if (!ZrParser_SemanticFacts_AppendReference(context, &fact)) {
        return ZR_FALSE;
    }
    type_use_publish_arguments(context, typeUse, typeId, isResolved);
    return ZR_TRUE;
}
