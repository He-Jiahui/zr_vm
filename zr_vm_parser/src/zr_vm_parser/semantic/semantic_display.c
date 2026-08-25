#include "zr_vm_parser/semantic_display.h"

#include <stdio.h>
#include <string.h>

#include "zr_vm_parser/canonical_type.h"

static TZrBool semantic_display_prepare_buffer(TZrChar *buffer, TZrSize bufferSize) {
    if (buffer == ZR_NULL || bufferSize == 0U) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    return ZR_TRUE;
}

static TZrBool semantic_display_copy_string(
        SZrString *value,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const TZrChar *text;
    TZrSize length;

    if (value == ZR_NULL || !semantic_display_prepare_buffer(buffer, bufferSize)) {
        return ZR_FALSE;
    }
    text = ZrCore_String_GetNativeString(value);
    length = ZrCore_String_GetByteLength(value);
    if (text == ZR_NULL || length + 1U > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return ZR_TRUE;
}

static SZrString *semantic_display_declaration_signature(
        const SZrSemanticContext *context,
        const SZrSemanticSymbolRecord *symbol) {
    TZrSize index;

    if (context == ZR_NULL || symbol == ZR_NULL || !context->referenceFacts.isValid) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->referenceFacts.length; ++index) {
        const SZrSemanticReferenceFact *fact =
                (const SZrSemanticReferenceFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->referenceFacts, index);
        if (fact != ZR_NULL && fact->kind == ZR_SEMANTIC_REFERENCE_DECLARATION &&
            fact->isResolved && fact->symbolId == symbol->id &&
            fact->typeId == symbol->typeId && fact->signatureDisplay != ZR_NULL) {
            return fact->signatureDisplay;
        }
    }
    return ZR_NULL;
}

static TZrBool semantic_display_is_function_symbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    const SZrSemanticSymbolRecord *symbol =
            ZrParser_Semantic_FindSymbolById(context, symbolId);
    const SZrCanonicalTypeNode *type;

    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_FUNCTION ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    type = ZrParser_CanonicalType_Find(context, symbol->typeId);
    return (TZrBool)(symbol != ZR_NULL &&
                     type != ZR_NULL && type->kind == ZR_CANONICAL_TYPE_FUNCTION);
}

TZrBool ZrParser_SemanticDisplay_FormatType(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    if (!semantic_display_prepare_buffer(buffer, bufferSize) || context == ZR_NULL ||
        typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    return ZrParser_CanonicalType_Format(context, typeId, buffer, bufferSize);
}

TZrBool ZrParser_SemanticDisplay_FormatSymbol(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticSymbolRecord *symbol;
    SZrString *signature;
    const TZrChar *name;
    TZrChar typeBuffer[512];
    int written;

    if (!semantic_display_prepare_buffer(buffer, bufferSize) || context == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    symbol = ZrParser_Semantic_FindSymbolById(context, symbolId);
    if (symbol == ZR_NULL || symbol->name == ZR_NULL ||
        symbol->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    signature = semantic_display_declaration_signature(context, symbol);
    if (signature != ZR_NULL) {
        return semantic_display_copy_string(signature, buffer, bufferSize);
    }
    if (!ZrParser_SemanticDisplay_FormatType(
                context, symbol->typeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }
    name = ZrCore_String_GetNativeString(symbol->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    written = snprintf(buffer, bufferSize, "%s: %s", name, typeBuffer);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}

TZrBool ZrParser_SemanticDisplay_FormatProperty(
        const SZrSemanticContext *context,
        const SZrSemanticPropertyContract *property,
        TZrChar *buffer,
        TZrSize bufferSize) {
    const SZrSemanticSymbolRecord *symbol;
    const TZrChar *name;
    const TZrChar *staticPrefix;
    const TZrChar *receiverPrefix;
    const TZrChar *referencePrefix;
    const TZrChar *getter;
    const TZrChar *setter;
    const TZrChar *initializer;
    TZrChar typeBuffer[512];
    int written;

    if (!semantic_display_prepare_buffer(buffer, bufferSize) || context == ZR_NULL ||
        property == ZR_NULL || property->propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        property->propertyTypeId == ZR_SEMANTIC_ID_INVALID ||
        (TZrInt32)property->receiverEffect < (TZrInt32)ZR_CANONICAL_RECEIVER_NONE ||
        property->receiverEffect > ZR_CANONICAL_RECEIVER_MUTABLE ||
        (TZrInt32)property->referenceAccess < (TZrInt32)ZR_REFERENCE_ACCESS_NONE ||
        property->referenceAccess > ZR_REFERENCE_ACCESS_READONLY) {
        return ZR_FALSE;
    }
    symbol = ZrParser_Semantic_FindSymbolById(context, property->propertySymbolId);
    if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_PROPERTY ||
        symbol->typeId != property->propertyTypeId ||
        (property->getterSymbolId == ZR_SEMANTIC_ID_INVALID &&
         property->setterSymbolId == ZR_SEMANTIC_ID_INVALID &&
         property->initializerSymbolId == ZR_SEMANTIC_ID_INVALID) ||
        (property->getterSymbolId != ZR_SEMANTIC_ID_INVALID &&
         !semantic_display_is_function_symbol(context, property->getterSymbolId)) ||
        (property->setterSymbolId != ZR_SEMANTIC_ID_INVALID &&
         !semantic_display_is_function_symbol(context, property->setterSymbolId)) ||
        (property->initializerSymbolId != ZR_SEMANTIC_ID_INVALID &&
         !semantic_display_is_function_symbol(context, property->initializerSymbolId))) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticDisplay_FormatType(
                context, property->propertyTypeId, typeBuffer, sizeof(typeBuffer))) {
        return ZR_FALSE;
    }
    name = ZrCore_String_GetNativeString(symbol->name);
    if (name == ZR_NULL) {
        return ZR_FALSE;
    }
    staticPrefix = property->isStatic ? "static " : "";
    receiverPrefix = property->receiverEffect == ZR_CANONICAL_RECEIVER_READONLY ? "const " : "";
    referencePrefix = property->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE
                              ? "ref "
                              : property->referenceAccess == ZR_REFERENCE_ACCESS_READONLY
                                      ? "ref readonly "
                                      : "";
    getter = property->getterSymbolId != ZR_SEMANTIC_ID_INVALID ? " get;" : "";
    setter = property->setterSymbolId != ZR_SEMANTIC_ID_INVALID ? " set;" : "";
    initializer = property->initializerSymbolId != ZR_SEMANTIC_ID_INVALID ? " init;" : "";
    written = snprintf(buffer,
                       bufferSize,
                       "%s%s%sproperty %s: %s {%s%s%s }",
                       staticPrefix,
                       receiverPrefix,
                       referencePrefix,
                       name,
                       typeBuffer,
                       getter,
                       setter,
                       initializer);
    return (TZrBool)(written >= 0 && (TZrSize)written < bufferSize);
}
