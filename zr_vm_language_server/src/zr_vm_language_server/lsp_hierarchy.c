#include "lsp_editor_features_internal.h"

#include <string.h>

static const TZrChar *lsp_hierarchy_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static void lsp_hierarchy_free_symbols(SZrState *state, SZrArray *symbols) {
    if (state == ZR_NULL || symbols == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *symbolPtr, sizeof(SZrLspSymbolInformation));
        }
    }
    ZrCore_Array_Free(state, symbols);
}

static TZrBool lsp_hierarchy_range_contains_position(SZrLspRange range, SZrLspPosition position) {
    if (position.line < range.start.line || position.line > range.end.line) {
        return ZR_FALSE;
    }
    if (position.line == range.start.line && position.character < range.start.character) {
        return ZR_FALSE;
    }
    if (position.line == range.end.line && position.character > range.end.character) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrInt32 lsp_hierarchy_range_score(SZrLspRange range) {
    TZrInt32 lineSpan = range.end.line - range.start.line;
    TZrInt32 charSpan = range.end.character - range.start.character;
    if (lineSpan < 0) {
        lineSpan = 0;
    }
    if (charSpan < 0) {
        charSpan = 0;
    }
    return lineSpan * 10000 + charSpan;
}

static TZrBool lsp_hierarchy_symbol_is_callable(TZrInt32 kind) {
    return kind == ZR_LSP_SYMBOL_KIND_FUNCTION || kind == ZR_LSP_SYMBOL_KIND_METHOD;
}

static TZrBool lsp_hierarchy_symbol_is_type(TZrInt32 kind) {
    return kind == ZR_LSP_SYMBOL_KIND_CLASS ||
           kind == ZR_LSP_SYMBOL_KIND_STRUCT ||
           kind == ZR_LSP_SYMBOL_KIND_INTERFACE ||
           kind == ZR_LSP_SYMBOL_KIND_ENUM;
}

static TZrBool lsp_hierarchy_item_from_symbol(SZrState *state,
                                              const SZrLspSymbolInformation *symbol,
                                              SZrLspHierarchyItem **outItem) {
    SZrLspHierarchyItem *item;

    if (outItem != ZR_NULL) {
        *outItem = ZR_NULL;
    }
    if (state == ZR_NULL || symbol == ZR_NULL || outItem == ZR_NULL) {
        return ZR_FALSE;
    }

    item = (SZrLspHierarchyItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHierarchyItem));
    if (item == ZR_NULL) {
        return ZR_FALSE;
    }
    item->name = symbol->name;
    item->detail = symbol->containerName;
    item->kind = symbol->kind;
    item->uri = symbol->location.uri;
    item->range = symbol->location.range;
    item->selectionRange = symbol->location.range;
    *outItem = item;
    return ZR_TRUE;
}

static TZrBool lsp_hierarchy_append_item_from_symbol(SZrState *state,
                                                     SZrArray *result,
                                                     const SZrLspSymbolInformation *symbol) {
    SZrLspHierarchyItem *item;

    if (state == ZR_NULL || result == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    if (!lsp_hierarchy_item_from_symbol(state, symbol, &item)) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(state, result, &item);
    return ZR_TRUE;
}

static TZrBool lsp_hierarchy_prepare(SZrState *state,
                                     SZrLspContext *context,
                                     SZrString *uri,
                                     SZrLspPosition position,
                                     TZrBool wantTypes,
                                     SZrArray *result) {
    SZrArray symbols = {0};
    SZrLspSymbolInformation *best = ZR_NULL;
    TZrInt32 bestScore = 0;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols)) {
        lsp_hierarchy_free_symbols(state, &symbols);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < symbols.length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(&symbols, index);
        TZrBool matchesKind;
        TZrInt32 score;

        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL) {
            continue;
        }
        matchesKind = wantTypes ? lsp_hierarchy_symbol_is_type((*symbolPtr)->kind)
                                : lsp_hierarchy_symbol_is_callable((*symbolPtr)->kind);
        if (!matchesKind || !lsp_hierarchy_range_contains_position((*symbolPtr)->location.range, position)) {
            continue;
        }

        score = lsp_hierarchy_range_score((*symbolPtr)->location.range);
        if (best == ZR_NULL || score < bestScore) {
            best = *symbolPtr;
            bestScore = score;
        }
    }

    if (best != ZR_NULL && !lsp_hierarchy_append_item_from_symbol(state, result, best)) {
        lsp_hierarchy_free_symbols(state, &symbols);
        return ZR_FALSE;
    }

    lsp_hierarchy_free_symbols(state, &symbols);
    return ZR_TRUE;
}

static TZrBool lsp_hierarchy_identifier_start(TZrChar ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static TZrBool lsp_hierarchy_identifier_part(TZrChar ch) {
    return lsp_hierarchy_identifier_start(ch) || (ch >= '0' && ch <= '9');
}

static const SZrLspSymbolInformation *lsp_hierarchy_find_callable_symbol(SZrArray *symbols,
                                                                         const TZrChar *name,
                                                                         TZrSize nameLength,
                                                                         const SZrLspHierarchyItem *currentItem) {
    const TZrChar *currentName = lsp_hierarchy_string_text(currentItem != ZR_NULL ? currentItem->name : ZR_NULL);

    if (symbols == ZR_NULL || name == ZR_NULL || nameLength == 0) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);
        const TZrChar *symbolName;

        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL ||
            !lsp_hierarchy_symbol_is_callable((*symbolPtr)->kind)) {
            continue;
        }

        symbolName = lsp_hierarchy_string_text((*symbolPtr)->name);
        if (symbolName != ZR_NULL &&
            strlen(symbolName) == nameLength &&
            memcmp(symbolName, name, nameLength) == 0 &&
            (currentName == ZR_NULL || strcmp(symbolName, currentName) != 0)) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool lsp_hierarchy_append_outgoing_call(SZrState *state,
                                                  SZrArray *result,
                                                  const SZrLspSymbolInformation *targetSymbol,
                                                  SZrLspRange fromRange) {
    SZrLspHierarchyCall *call;
    SZrLspHierarchyItem *item;

    if (state == ZR_NULL || result == ZR_NULL || targetSymbol == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }
    if (!lsp_hierarchy_item_from_symbol(state, targetSymbol, &item)) {
        return ZR_FALSE;
    }

    call = (SZrLspHierarchyCall *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspHierarchyCall));
    if (call == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, item, sizeof(SZrLspHierarchyItem));
        return ZR_FALSE;
    }

    call->item = item;
    ZrCore_Array_Init(state, &call->fromRanges, sizeof(SZrLspRange), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    ZrCore_Array_Push(state, &call->fromRanges, &fromRange);
    ZrCore_Array_Push(state, result, &call);
    return ZR_TRUE;
}

static TZrBool lsp_hierarchy_symbol_name_matches(const SZrLspSymbolInformation *symbol,
                                                 const TZrChar *name,
                                                 TZrSize nameLength) {
    const TZrChar *symbolName;

    if (symbol == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    symbolName = lsp_hierarchy_string_text(symbol->name);
    return symbolName != ZR_NULL &&
           strlen(symbolName) == nameLength &&
           memcmp(symbolName, name, nameLength) == 0;
}

static TZrBool lsp_hierarchy_scan_symbol_for_named_calls(SZrState *state,
                                                         SZrFileVersion *fileVersion,
                                                         const SZrLspSymbolInformation *callerSymbol,
                                                         const TZrChar *targetName,
                                                         TZrSize targetNameLength,
                                                         SZrArray *result) {
    TZrSize startOffset;
    TZrSize endOffset;
    TZrSize cursor;

    if (state == ZR_NULL ||
        fileVersion == ZR_NULL ||
        fileVersion->content == ZR_NULL ||
        callerSymbol == ZR_NULL ||
        targetName == ZR_NULL ||
        targetNameLength == 0 ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    startOffset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                                     fileVersion->contentLength,
                                                                     callerSymbol->location.range.start.line,
                                                                     callerSymbol->location.range.start.character);
    endOffset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                                   fileVersion->contentLength,
                                                                   callerSymbol->location.range.end.line,
                                                                   callerSymbol->location.range.end.character);
    cursor = startOffset;
    while (cursor + targetNameLength < endOffset && cursor + targetNameLength < fileVersion->contentLength) {
        TZrSize callCursor;

        if ((cursor > startOffset && lsp_hierarchy_identifier_part(fileVersion->content[cursor - 1])) ||
            memcmp(fileVersion->content + cursor, targetName, targetNameLength) != 0 ||
            (cursor + targetNameLength < fileVersion->contentLength &&
             lsp_hierarchy_identifier_part(fileVersion->content[cursor + targetNameLength]))) {
            cursor++;
            continue;
        }

        callCursor = cursor + targetNameLength;
        while (callCursor < endOffset &&
               callCursor < fileVersion->contentLength &&
               (fileVersion->content[callCursor] == ' ' || fileVersion->content[callCursor] == '\t')) {
            callCursor++;
        }
        if (callCursor < endOffset &&
            callCursor < fileVersion->contentLength &&
            fileVersion->content[callCursor] == '(') {
            SZrLspRange fromRange =
                    lsp_editor_range_from_offsets(fileVersion->content,
                                                  fileVersion->contentLength,
                                                  cursor,
                                                  cursor + targetNameLength);
            if (!lsp_hierarchy_append_outgoing_call(state, result, callerSymbol, fromRange)) {
                return ZR_FALSE;
            }
        }

        cursor = callCursor + 1;
    }

    return ZR_TRUE;
}

static TZrSize lsp_hierarchy_symbol_start_offset(SZrFileVersion *fileVersion,
                                                 const SZrLspSymbolInformation *symbol) {
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || symbol == ZR_NULL) {
        return 0;
    }
    return ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                              fileVersion->contentLength,
                                                              symbol->location.range.start.line,
                                                              symbol->location.range.start.character);
}

static TZrSize lsp_hierarchy_symbol_end_offset(SZrFileVersion *fileVersion,
                                               const SZrLspSymbolInformation *symbol) {
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || symbol == ZR_NULL) {
        return 0;
    }
    return ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                              fileVersion->contentLength,
                                                              symbol->location.range.end.line,
                                                              symbol->location.range.end.character);
}

static TZrSize lsp_hierarchy_type_header_end(SZrFileVersion *fileVersion,
                                             const SZrLspSymbolInformation *symbol) {
    TZrSize cursor;
    TZrSize endOffset;

    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || symbol == ZR_NULL) {
        return 0;
    }

    cursor = lsp_hierarchy_symbol_start_offset(fileVersion, symbol);
    endOffset = lsp_hierarchy_symbol_end_offset(fileVersion, symbol);
    while (cursor < endOffset && cursor < fileVersion->contentLength) {
        if (fileVersion->content[cursor] == '{' || fileVersion->content[cursor] == '\n') {
            return cursor;
        }
        cursor++;
    }
    return cursor;
}

static const SZrLspSymbolInformation *lsp_hierarchy_find_type_symbol(SZrArray *symbols,
                                                                     const TZrChar *name,
                                                                     TZrSize nameLength) {
    if (symbols == ZR_NULL || name == ZR_NULL || nameLength == 0) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);

        if (symbolPtr != ZR_NULL &&
            *symbolPtr != ZR_NULL &&
            lsp_hierarchy_symbol_is_type((*symbolPtr)->kind) &&
            lsp_hierarchy_symbol_name_matches(*symbolPtr, name, nameLength)) {
            return *symbolPtr;
        }
    }

    return ZR_NULL;
}

static TZrSize lsp_hierarchy_find_inheritance_colon(SZrFileVersion *fileVersion,
                                                    const SZrLspSymbolInformation *symbol,
                                                    TZrSize headerEnd) {
    TZrSize cursor;
    TZrSize angleDepth = 0;

    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || symbol == ZR_NULL) {
        return headerEnd;
    }

    cursor = lsp_hierarchy_symbol_start_offset(fileVersion, symbol);
    while (cursor < headerEnd && cursor < fileVersion->contentLength) {
        if (fileVersion->content[cursor] == '<') {
            angleDepth++;
            cursor++;
            continue;
        }
        if (fileVersion->content[cursor] == '>' && angleDepth > 0) {
            angleDepth--;
            cursor++;
            continue;
        }
        if (angleDepth == 0 &&
            cursor + 5 <= headerEnd &&
            memcmp(fileVersion->content + cursor, "where", 5) == 0 &&
            (cursor == 0 || !lsp_hierarchy_identifier_part(fileVersion->content[cursor - 1])) &&
            (cursor + 5 == headerEnd || !lsp_hierarchy_identifier_part(fileVersion->content[cursor + 5]))) {
            return headerEnd;
        }
        if (angleDepth == 0 && fileVersion->content[cursor] == ':') {
            return cursor;
        }
        cursor++;
    }

    return headerEnd;
}

static TZrBool lsp_hierarchy_type_header_contains_base(SZrFileVersion *fileVersion,
                                                       const SZrLspSymbolInformation *symbol,
                                                       const TZrChar *baseName,
                                                       TZrSize baseNameLength) {
    TZrSize cursor;
    TZrSize headerEnd;

    if (fileVersion == ZR_NULL ||
        fileVersion->content == ZR_NULL ||
        symbol == ZR_NULL ||
        baseName == ZR_NULL ||
        baseNameLength == 0) {
        return ZR_FALSE;
    }

    headerEnd = lsp_hierarchy_type_header_end(fileVersion, symbol);
    cursor = lsp_hierarchy_find_inheritance_colon(fileVersion, symbol, headerEnd);
    if (cursor >= headerEnd) {
        return ZR_FALSE;
    }
    cursor++;
    while (cursor < headerEnd && cursor < fileVersion->contentLength) {
        TZrSize nameStart;
        TZrSize nameEnd;

        if (!lsp_hierarchy_identifier_start(fileVersion->content[cursor])) {
            cursor++;
            continue;
        }

        nameStart = cursor;
        nameEnd = cursor + 1;
        while (nameEnd < headerEnd &&
               nameEnd < fileVersion->contentLength &&
               lsp_hierarchy_identifier_part(fileVersion->content[nameEnd])) {
            nameEnd++;
        }

        if (nameEnd - nameStart == baseNameLength &&
            memcmp(fileVersion->content + nameStart, baseName, baseNameLength) == 0) {
            return ZR_TRUE;
        }
        cursor = nameEnd;
    }

    return ZR_FALSE;
}

static TZrBool lsp_hierarchy_append_direct_supertypes(SZrState *state,
                                                      SZrFileVersion *fileVersion,
                                                      SZrArray *symbols,
                                                      const SZrLspHierarchyItem *item,
                                                      SZrArray *result) {
    const TZrChar *itemName = lsp_hierarchy_string_text(item != ZR_NULL ? item->name : ZR_NULL);
    const SZrLspSymbolInformation *itemSymbol;
    TZrSize cursor;
    TZrSize headerEnd;

    if (state == ZR_NULL || fileVersion == ZR_NULL || symbols == ZR_NULL || item == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    itemSymbol = itemName != ZR_NULL ? lsp_hierarchy_find_type_symbol(symbols, itemName, strlen(itemName)) : ZR_NULL;
    if (itemSymbol == ZR_NULL) {
        return ZR_TRUE;
    }

    headerEnd = lsp_hierarchy_type_header_end(fileVersion, itemSymbol);
    cursor = lsp_hierarchy_find_inheritance_colon(fileVersion, itemSymbol, headerEnd);
    if (cursor >= headerEnd) {
        return ZR_TRUE;
    }
    cursor++;
    while (cursor < headerEnd && cursor < fileVersion->contentLength) {
        TZrSize nameStart;
        TZrSize nameEnd;
        const SZrLspSymbolInformation *baseSymbol;

        if (!lsp_hierarchy_identifier_start(fileVersion->content[cursor])) {
            cursor++;
            continue;
        }

        nameStart = cursor;
        nameEnd = cursor + 1;
        while (nameEnd < headerEnd &&
               nameEnd < fileVersion->contentLength &&
               lsp_hierarchy_identifier_part(fileVersion->content[nameEnd])) {
            nameEnd++;
        }

        baseSymbol = lsp_hierarchy_find_type_symbol(symbols,
                                                    fileVersion->content + nameStart,
                                                    nameEnd - nameStart);
        if (baseSymbol != ZR_NULL && !lsp_hierarchy_append_item_from_symbol(state, result, baseSymbol)) {
            return ZR_FALSE;
        }
        cursor = nameEnd;
    }

    return ZR_TRUE;
}

static TZrBool lsp_hierarchy_append_direct_subtypes(SZrState *state,
                                                    SZrFileVersion *fileVersion,
                                                    SZrArray *symbols,
                                                    const SZrLspHierarchyItem *item,
                                                    SZrArray *result) {
    const TZrChar *itemName;
    TZrSize itemNameLength;

    if (state == ZR_NULL || fileVersion == ZR_NULL || symbols == ZR_NULL || item == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    itemName = lsp_hierarchy_string_text(item->name);
    if (itemName == ZR_NULL) {
        return ZR_TRUE;
    }
    itemNameLength = strlen(itemName);

    for (TZrSize index = 0; index < symbols->length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(symbols, index);

        if (symbolPtr == ZR_NULL ||
            *symbolPtr == ZR_NULL ||
            !lsp_hierarchy_symbol_is_type((*symbolPtr)->kind) ||
            lsp_hierarchy_symbol_name_matches(*symbolPtr, itemName, itemNameLength)) {
            continue;
        }

        if (lsp_hierarchy_type_header_contains_base(fileVersion, *symbolPtr, itemName, itemNameLength) &&
            !lsp_hierarchy_append_item_from_symbol(state, result, *symbolPtr)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_PrepareCallHierarchy(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  SZrArray *result) {
    return lsp_hierarchy_prepare(state, context, uri, position, ZR_FALSE, result);
}

TZrBool ZrLanguageServer_Lsp_GetCallHierarchyIncomingCalls(SZrState *state,
                                                           SZrLspContext *context,
                                                           const SZrLspHierarchyItem *item,
                                                           SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrArray symbols = {0};
    const TZrChar *targetName;
    TZrSize targetNameLength;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL || item->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, item->uri);
    targetName = lsp_hierarchy_string_text(item->name);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL || targetName == ZR_NULL) {
        return ZR_TRUE;
    }

    targetNameLength = strlen(targetName);
    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, item->uri, &symbols)) {
        lsp_hierarchy_free_symbols(state, &symbols);
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < symbols.length; index++) {
        SZrLspSymbolInformation **symbolPtr = (SZrLspSymbolInformation **)ZrCore_Array_Get(&symbols, index);

        if (symbolPtr == ZR_NULL ||
            *symbolPtr == ZR_NULL ||
            !lsp_hierarchy_symbol_is_callable((*symbolPtr)->kind) ||
            lsp_hierarchy_symbol_name_matches(*symbolPtr, targetName, targetNameLength)) {
            continue;
        }

        if (!lsp_hierarchy_scan_symbol_for_named_calls(state,
                                                       fileVersion,
                                                       *symbolPtr,
                                                       targetName,
                                                       targetNameLength,
                                                       result)) {
            lsp_hierarchy_free_symbols(state, &symbols);
            return ZR_FALSE;
        }
    }

    lsp_hierarchy_free_symbols(state, &symbols);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetCallHierarchyOutgoingCalls(SZrState *state,
                                                           SZrLspContext *context,
                                                           const SZrLspHierarchyItem *item,
                                                           SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrArray symbols = {0};
    TZrSize startOffset;
    TZrSize endOffset;
    TZrSize cursor;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL || item->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyCall *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, item->uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, item->uri, &symbols)) {
        lsp_hierarchy_free_symbols(state, &symbols);
        return ZR_FALSE;
    }

    startOffset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                                     fileVersion->contentLength,
                                                                     item->range.start.line,
                                                                     item->range.start.character);
    endOffset = ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn(fileVersion->content,
                                                                   fileVersion->contentLength,
                                                                   item->range.end.line,
                                                                   item->range.end.character);
    cursor = startOffset;
    while (cursor < endOffset && cursor < fileVersion->contentLength) {
        TZrSize nameStart;
        TZrSize nameEnd;
        TZrSize callCursor;
        const SZrLspSymbolInformation *targetSymbol;

        if (!lsp_hierarchy_identifier_start(fileVersion->content[cursor])) {
            cursor++;
            continue;
        }

        nameStart = cursor;
        nameEnd = cursor + 1;
        while (nameEnd < endOffset &&
               nameEnd < fileVersion->contentLength &&
               lsp_hierarchy_identifier_part(fileVersion->content[nameEnd])) {
            nameEnd++;
        }

        callCursor = nameEnd;
        while (callCursor < endOffset &&
               callCursor < fileVersion->contentLength &&
               (fileVersion->content[callCursor] == ' ' || fileVersion->content[callCursor] == '\t')) {
            callCursor++;
        }
        if (callCursor >= endOffset ||
            callCursor >= fileVersion->contentLength ||
            fileVersion->content[callCursor] != '(') {
            cursor = nameEnd;
            continue;
        }

        targetSymbol = lsp_hierarchy_find_callable_symbol(&symbols,
                                                          fileVersion->content + nameStart,
                                                          nameEnd - nameStart,
                                                          item);
        if (targetSymbol != ZR_NULL) {
            SZrLspRange fromRange =
                    lsp_editor_range_from_offsets(fileVersion->content, fileVersion->contentLength, nameStart, nameEnd);
            if (!lsp_hierarchy_append_outgoing_call(state, result, targetSymbol, fromRange)) {
                lsp_hierarchy_free_symbols(state, &symbols);
                return ZR_FALSE;
            }
        }

        cursor = callCursor + 1;
    }

    lsp_hierarchy_free_symbols(state, &symbols);
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_PrepareTypeHierarchy(SZrState *state,
                                                  SZrLspContext *context,
                                                  SZrString *uri,
                                                  SZrLspPosition position,
                                                  SZrArray *result) {
    return lsp_hierarchy_prepare(state, context, uri, position, ZR_TRUE, result);
}

TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySupertypes(SZrState *state,
                                                        SZrLspContext *context,
                                                        const SZrLspHierarchyItem *item,
                                                        SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrArray symbols = {0};
    TZrBool ok;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL || item->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, item->uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, item->uri, &symbols)) {
        lsp_hierarchy_free_symbols(state, &symbols);
        return ZR_FALSE;
    }

    ok = lsp_hierarchy_append_direct_supertypes(state, fileVersion, &symbols, item, result);
    lsp_hierarchy_free_symbols(state, &symbols);
    return ok;
}

TZrBool ZrLanguageServer_Lsp_GetTypeHierarchySubtypes(SZrState *state,
                                                      SZrLspContext *context,
                                                      const SZrLspHierarchyItem *item,
                                                      SZrArray *result) {
    SZrFileVersion *fileVersion;
    SZrArray symbols = {0};
    TZrBool ok;

    if (state == ZR_NULL || context == ZR_NULL || item == ZR_NULL || item->uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspHierarchyItem *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    }

    fileVersion = lsp_editor_get_file_version(context, item->uri);
    if (fileVersion == ZR_NULL || fileVersion->content == ZR_NULL) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, &symbols, sizeof(SZrLspSymbolInformation *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    if (!ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, item->uri, &symbols)) {
        lsp_hierarchy_free_symbols(state, &symbols);
        return ZR_FALSE;
    }

    ok = lsp_hierarchy_append_direct_subtypes(state, fileVersion, &symbols, item, result);
    lsp_hierarchy_free_symbols(state, &symbols);
    return ok;
}
