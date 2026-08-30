#include "type_inference_type_display_alias.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic_display.h"
#include "type_inference_internal.h"

static SZrTypeBinding *type_inference_find_type_value_alias_binding(
        SZrCompilerState *cs,
        SZrString *name) {
    TZrSize index;

    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < cs->typeValueAliases.length; index++) {
        SZrTypeBinding *binding = (SZrTypeBinding *)ZrCore_Array_Get(
                &cs->typeValueAliases, index);
        if (binding != ZR_NULL && binding->name != ZR_NULL &&
            ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }
    return ZR_NULL;
}

TZrBool type_inference_resolve_type_value_alias(
        SZrCompilerState *cs,
        SZrString *name,
        SZrInferredType *result) {
    SZrTypeBinding *binding;

    if (result == ZR_NULL) {
        return ZR_FALSE;
    }
    binding = type_inference_find_type_value_alias_binding(cs, name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Copy(cs->state, result, &binding->type);
    return ZR_TRUE;
}

static TZrBool type_inference_type_use_name_range(
        const SZrAstNode *nameNode,
        SZrFileRange *outRange) {
    if (nameNode == ZR_NULL || outRange == ZR_NULL) {
        return ZR_FALSE;
    }
    if (nameNode->type == ZR_AST_GENERIC_TYPE) {
        const SZrFileRange *wholeRange = &nameNode->data.genericType.wholeRange;
        if (wholeRange->source == ZR_NULL ||
            wholeRange->end.offset <= wholeRange->start.offset) {
            return ZR_FALSE;
        }
        *outRange = *wholeRange;
        return ZR_TRUE;
    }
    *outRange = nameNode->location;
    return outRange->source != ZR_NULL &&
           outRange->end.offset > outRange->start.offset;
}

static TZrBool type_inference_alias_append_text(
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset,
        const TZrChar *text) {
    TZrSize length;

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    length = strlen(text);
    if (*offset + length + 1U > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer + *offset, text, length);
    *offset += length;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

static TZrBool type_inference_alias_append_type(
        const SZrType *type,
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset);

static TZrBool type_inference_alias_append_argument(
        const SZrAstNode *node,
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset) {
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->type) {
        case ZR_AST_TYPE:
            return type_inference_alias_append_type(
                    &node->data.type, buffer, bufferSize, offset);
        case ZR_AST_IDENTIFIER_LITERAL:
            return node->data.identifier.name != ZR_NULL &&
                   type_inference_alias_append_text(
                           buffer,
                           bufferSize,
                           offset,
                           ZrCore_String_GetNativeString(
                                   node->data.identifier.name));
        case ZR_AST_INTEGER_LITERAL:
            if (node->data.integerLiteral.literal != ZR_NULL) {
                return type_inference_alias_append_text(
                        buffer,
                        bufferSize,
                        offset,
                        ZrCore_String_GetNativeString(
                                node->data.integerLiteral.literal));
            }
            {
                TZrChar integerBuffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];
                snprintf(integerBuffer,
                         sizeof(integerBuffer),
                         "%lld",
                         (long long)node->data.integerLiteral.value);
                return type_inference_alias_append_text(
                        buffer, bufferSize, offset, integerBuffer);
            }
        case ZR_AST_UNARY_EXPRESSION:
            return node->data.unaryExpression.op.op != ZR_NULL &&
                   type_inference_alias_append_text(
                           buffer,
                           bufferSize,
                           offset,
                           node->data.unaryExpression.op.op) &&
                   type_inference_alias_append_argument(
                           node->data.unaryExpression.argument,
                           buffer,
                           bufferSize,
                           offset);
        case ZR_AST_BINARY_EXPRESSION:
            return node->data.binaryExpression.op.op != ZR_NULL &&
                   type_inference_alias_append_argument(
                           node->data.binaryExpression.left,
                           buffer,
                           bufferSize,
                           offset) &&
                   type_inference_alias_append_text(
                           buffer, bufferSize, offset, " ") &&
                   type_inference_alias_append_text(
                           buffer,
                           bufferSize,
                           offset,
                           node->data.binaryExpression.op.op) &&
                   type_inference_alias_append_text(
                           buffer, bufferSize, offset, " ") &&
                   type_inference_alias_append_argument(
                           node->data.binaryExpression.right,
                           buffer,
                           bufferSize,
                           offset);
        default:
            return ZR_FALSE;
    }
}

static TZrBool type_inference_alias_append_type_name(
        const SZrAstNode *nameNode,
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset) {
    TZrSize index;

    if (nameNode == ZR_NULL) {
        return ZR_FALSE;
    }
    if (nameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        return nameNode->data.identifier.name != ZR_NULL &&
               type_inference_alias_append_text(
                       buffer,
                       bufferSize,
                       offset,
                       ZrCore_String_GetNativeString(
                               nameNode->data.identifier.name));
    }
    if (nameNode->type == ZR_AST_GENERIC_TYPE) {
        const SZrGenericType *generic = &nameNode->data.genericType;
        if (generic->name == ZR_NULL || generic->name->name == ZR_NULL ||
            generic->params == ZR_NULL || generic->params->count == 0U ||
            !type_inference_alias_append_text(
                    buffer,
                    bufferSize,
                    offset,
                    ZrCore_String_GetNativeString(generic->name->name)) ||
            !type_inference_alias_append_text(
                    buffer, bufferSize, offset, "<")) {
            return ZR_FALSE;
        }
        for (index = 0; index < generic->params->count; index++) {
            if ((index > 0U &&
                 !type_inference_alias_append_text(
                         buffer, bufferSize, offset, ", ")) ||
                !type_inference_alias_append_argument(
                        generic->params->nodes[index],
                        buffer,
                        bufferSize,
                        offset)) {
                return ZR_FALSE;
            }
        }
        return type_inference_alias_append_text(
                buffer, bufferSize, offset, ">");
    }
    if (nameNode->type == ZR_AST_TUPLE_TYPE) {
        const SZrAstNodeArray *elements = nameNode->data.tupleType.elements;
        if (elements == ZR_NULL || elements->count == 0U ||
            !type_inference_alias_append_text(
                    buffer, bufferSize, offset, "(")) {
            return ZR_FALSE;
        }
        for (index = 0; index < elements->count; index++) {
            if ((index > 0U &&
                 !type_inference_alias_append_text(
                         buffer, bufferSize, offset, ", ")) ||
                !type_inference_alias_append_argument(
                        elements->nodes[index],
                        buffer,
                        bufferSize,
                        offset)) {
                return ZR_FALSE;
            }
        }
        return type_inference_alias_append_text(
                buffer, bufferSize, offset, ")");
    }
    return ZR_FALSE;
}

static TZrBool type_inference_alias_append_type(
        const SZrType *type,
        TZrChar *buffer,
        TZrSize bufferSize,
        TZrSize *offset) {
    TZrInt32 dimension;

    if (type == ZR_NULL || type->name == ZR_NULL ||
        type->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        type->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        type->isReadonlyView ||
        !type_inference_alias_append_type_name(
                type->name, buffer, bufferSize, offset)) {
        return ZR_FALSE;
    }
    if (type->subType != ZR_NULL &&
        (!type_inference_alias_append_text(
                 buffer, bufferSize, offset, ".") ||
         !type_inference_alias_append_type(
                 type->subType, buffer, bufferSize, offset))) {
        return ZR_FALSE;
    }
    for (dimension = 0; dimension < type->dimensions; dimension++) {
        if (!type_inference_alias_append_text(
                    buffer, bufferSize, offset, "[]")) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

void type_inference_publish_explicit_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        SZrString *alias,
        const SZrType *typeUse) {
    const SZrType *terminalType;
    SZrFileRange useRange;
    TZrTypeId typeId;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || type == ZR_NULL ||
        alias == ZR_NULL || typeUse == ZR_NULL || typeUse->name == ZR_NULL ||
        typeUse->dimensions != 0 ||
        typeUse->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        typeUse->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        typeUse->isReadonlyView) {
        return;
    }
    terminalType = typeUse;
    while (terminalType->subType != ZR_NULL) {
        terminalType = terminalType->subType;
    }
    if (terminalType->name == ZR_NULL) {
        return;
    }
    if (!type_inference_type_use_name_range(typeUse->name, &useRange)) {
        return;
    }
    {
        SZrFileRange terminalRange;
        if (!type_inference_type_use_name_range(
                    terminalType->name, &terminalRange)) {
            return;
        }
        useRange.end = terminalRange.end;
    }
    typeId = ZrParser_CanonicalType_FromInferred(cs->semanticContext, type);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    (void)ZrParser_SemanticTypeDisplayAlias_Publish(
            cs->semanticContext, typeId, &useRange, alias);
}

void type_inference_publish_generic_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;
    SZrString *alias;

    if (cs == ZR_NULL || type == ZR_NULL || typeUse == ZR_NULL ||
        typeUse->name == ZR_NULL ||
        typeUse->name->type != ZR_AST_GENERIC_TYPE) {
        return;
    }
    buffer[0] = '\0';
    if (!type_inference_alias_append_type(
                typeUse, buffer, sizeof(buffer), &offset)) {
        return;
    }
    alias = ZrCore_String_Create(cs->state, buffer, offset);
    if (alias == ZR_NULL) {
        return;
    }
    type_inference_publish_explicit_type_display_alias(
            cs, type, alias, typeUse);
}

void type_inference_publish_primitive_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse) {
    const SZrAstNode *typeUseNode;
    SZrString *alias;
    TZrNativeString aliasText;
    TZrSize aliasLength;
    EZrValueType primitiveType;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL || type == ZR_NULL ||
        typeUse == ZR_NULL || typeUse->name == ZR_NULL ||
        typeUse->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return;
    }
    typeUseNode = typeUse->name;
    alias = typeUseNode->data.identifier.name;
    if (alias == ZR_NULL) {
        return;
    }
    aliasText = ZrCore_String_GetNativeString(alias);
    aliasLength = alias->shortStringLength < ZR_VM_LONG_STRING_FLAG
                          ? alias->shortStringLength
                          : alias->longStringLength;
    if (aliasText == ZR_NULL ||
        !inferred_type_try_map_primitive_name(
                aliasText, aliasLength, &primitiveType)) {
        return;
    }
    type_inference_publish_explicit_type_display_alias(
            cs, type, alias, typeUse);
}

void type_inference_publish_type_value_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse) {
    SZrTypeBinding *binding;
    SZrType innerTypeUse;

    if (typeUse == ZR_NULL || typeUse->name == ZR_NULL ||
        typeUse->name->type != ZR_AST_IDENTIFIER_LITERAL) {
        return;
    }
    binding = type_inference_find_type_value_alias_binding(
            cs, typeUse->name->data.identifier.name);
    if (binding == ZR_NULL) {
        return;
    }
    innerTypeUse = *typeUse;
    innerTypeUse.dimensions = 0;
    innerTypeUse.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    innerTypeUse.referenceAccess = ZR_REFERENCE_ACCESS_NONE;
    innerTypeUse.isReadonlyView = ZR_FALSE;
    type_inference_publish_explicit_type_display_alias(
            cs, type, binding->name, &innerTypeUse);
}
