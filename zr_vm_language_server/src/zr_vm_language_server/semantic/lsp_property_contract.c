#include "semantic/lsp_property_contract.h"

#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *lsp_property_access_text(EZrAccessModifier access) {
    switch (access) {
        case ZR_ACCESS_PUBLIC: return "public";
        case ZR_ACCESS_PROTECTED: return "protected";
        case ZR_ACCESS_PRIVATE: return "private";
        default: return "unavailable";
    }
}

static const TZrChar *lsp_property_receiver_text(
        EZrCanonicalReceiverEffect effect) {
    switch (effect) {
        case ZR_CANONICAL_RECEIVER_READONLY: return "readonly";
        case ZR_CANONICAL_RECEIVER_MUTABLE: return "mutable";
        case ZR_CANONICAL_RECEIVER_NONE: return "none";
        default: return "unavailable";
    }
}

TZrBool ZrLanguageServer_LspPropertyContract_RegisterSourceSymbol(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *propertyNode,
        SZrSymbol **outSymbol) {
    SZrPropertyDeclaration *property;
    SZrParserSemanticPropertyQuery query;
    SZrInferredType inferredType;
    SZrInferredType *symbolType = ZR_NULL;
    SZrSymbol *symbol = ZR_NULL;

    if (outSymbol != ZR_NULL) {
        *outSymbol = ZR_NULL;
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || propertyNode == ZR_NULL ||
        propertyNode->type != ZR_AST_PROPERTY_DECLARATION) {
        return ZR_FALSE;
    }

    property = &propertyNode->data.propertyDeclaration;
    if (property->name == ZR_NULL || property->name->name == ZR_NULL ||
        !ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                property->nameLocation,
                ZR_NULL,
                &query)) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (property->typeInfo != ZR_NULL &&
        ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
                analyzer,
                ownerTypeNode,
                propertyNode,
                property->typeInfo,
                &inferredType)) {
        symbolType = &inferredType;
    }

    ZrLanguageServer_SymbolTable_AddSymbolEx(
            state,
            analyzer->symbolTable,
            ZR_SYMBOL_PROPERTY,
            property->name->name,
            query.declarationRange,
            symbolType,
            query.access,
            propertyNode,
            &symbol);
    ZrParser_InferredType_Free(state, &inferredType);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    symbol->location = query.declarationRange;
    symbol->selectionRange = query.selectionRange;
    symbol->semanticId = query.propertySymbolId;
    symbol->semanticTypeId = query.propertyTypeId;
    symbol->hasPropertyContract = ZR_TRUE;
    symbol->propertyContract = query;
    ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForRange(
            state,
            analyzer,
            symbol,
            query.selectionRange);

    if (outSymbol != ZR_NULL) {
        *outSymbol = symbol;
    }
    return ZR_TRUE;
}

SZrString *ZrLanguageServer_LspPropertyContract_FormatQuery(
        SZrState *state,
        SZrString *name,
        const TZrChar *typeText,
        const SZrParserSemanticPropertyQuery *query) {
    TZrChar signatureBuffer[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    const TZrChar *nameText;
    const TZrChar *referencePrefix = "";
    TZrChar accessorBuffer[192];
    TZrSize accessorUsed = 0U;
    TZrInt32 written;

    if (state == ZR_NULL || name == ZR_NULL || typeText == ZR_NULL ||
        typeText[0] == '\0' || query == ZR_NULL ||
        query->propertySymbolId == ZR_SEMANTIC_ID_INVALID ||
        query->propertyTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    nameText = semantic_string_native(name);
    if (nameText == ZR_NULL) {
        return ZR_NULL;
    }
    if (query->referenceAccess == ZR_REFERENCE_ACCESS_WRITABLE) {
        referencePrefix = "ref ";
    } else if (query->referenceAccess == ZR_REFERENCE_ACCESS_READONLY) {
        referencePrefix = "ref readonly ";
    }
    accessorBuffer[0] = '\0';
    if (query->getterSymbolId != ZR_SEMANTIC_ID_INVALID) {
        accessorUsed += (TZrSize)snprintf(
                accessorBuffer + accessorUsed,
                sizeof(accessorBuffer) - accessorUsed,
                "get %s",
                lsp_property_access_text(query->getterAccess));
    }
    if (query->setterSymbolId != ZR_SEMANTIC_ID_INVALID &&
        accessorUsed < sizeof(accessorBuffer)) {
        accessorUsed += (TZrSize)snprintf(
                accessorBuffer + accessorUsed,
                sizeof(accessorBuffer) - accessorUsed,
                "%sset %s",
                accessorUsed > 0U ? ", " : "",
                lsp_property_access_text(query->setterAccess));
    }
    if (query->initializerSymbolId != ZR_SEMANTIC_ID_INVALID &&
        accessorUsed < sizeof(accessorBuffer)) {
        accessorUsed += (TZrSize)snprintf(
                accessorBuffer + accessorUsed,
                sizeof(accessorBuffer) - accessorUsed,
                "%sinit %s",
                accessorUsed > 0U ? ", " : "",
                lsp_property_access_text(query->initializerAccess));
    }
    if (accessorUsed >= sizeof(accessorBuffer)) {
        return ZR_NULL;
    }
    written = snprintf(
            signatureBuffer,
            sizeof(signatureBuffer),
            "property %s: %s%s [access %s; %s; receiver %s]",
            nameText,
            referencePrefix,
            typeText,
            lsp_property_access_text(query->access),
            accessorBuffer,
            lsp_property_receiver_text(query->receiverEffect));
    if (written <= 0 || (TZrSize)written >= sizeof(signatureBuffer)) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, signatureBuffer, (TZrSize)written);
}

SZrString *ZrLanguageServer_LspPropertyContract_FormatSignature(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSymbol *symbol) {
    SZrParserSemanticPropertyQuery query;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL ||
        symbol->type != ZR_SYMBOL_PROPERTY || symbol->name == ZR_NULL ||
        symbol->semanticId == ZR_SEMANTIC_ID_INVALID ||
        symbol->semanticTypeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrParser_SemanticQuery_PropertyBySymbolId(
                analyzer->semanticContext,
                symbol->semanticId,
                &query) ||
        query.propertySymbolId != symbol->semanticId ||
        query.propertyTypeId != symbol->semanticTypeId ||
        !ZrParser_CanonicalType_Format(
                analyzer->semanticContext,
                query.propertyTypeId,
                typeBuffer,
                sizeof(typeBuffer))) {
        return ZR_NULL;
    }
    return ZrLanguageServer_LspPropertyContract_FormatQuery(
            state,
            symbol->name,
            typeBuffer,
            &query);
}

SZrSymbol *ZrLanguageServer_LspPropertyContract_FindSourceSymbolAt(
        SZrSemanticAnalyzer *analyzer,
        SZrFileRange position) {
    SZrParserSemanticPropertyQuery query;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->symbolTable == ZR_NULL ||
        !ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                position,
                ZR_NULL,
                &query)) {
        return ZR_NULL;
    }
    for (TZrSize scopeIndex = 0U;
         scopeIndex < analyzer->symbolTable->allScopes.length;
         scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(
                &analyzer->symbolTable->allScopes,
                scopeIndex);
        SZrSymbolScope *scope = scopePtr != ZR_NULL ? *scopePtr : ZR_NULL;

        for (TZrSize symbolIndex = 0U;
             scope != ZR_NULL && symbolIndex < scope->symbols.length;
             symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(
                    &scope->symbols,
                    symbolIndex);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL &&
                (*symbolPtr)->hasPropertyContract &&
                (*symbolPtr)->semanticId == query.propertySymbolId) {
                return *symbolPtr;
            }
        }
    }
    return ZR_NULL;
}
