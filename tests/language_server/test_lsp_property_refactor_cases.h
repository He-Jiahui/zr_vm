#ifndef ZR_VM_TEST_LSP_PROPERTY_REFACTOR_CASES_H
#define ZR_VM_TEST_LSP_PROPERTY_REFACTOR_CASES_H

static SZrLspCodeAction *property_refactor_find_action(
        SZrArray *actions,
        const TZrChar *title) {
    for (TZrSize index = 0U;
         actions != ZR_NULL && title != ZR_NULL && index < actions->length;
         index++) {
        SZrLspCodeAction **actionPtr =
                (SZrLspCodeAction **)ZrCore_Array_Get(actions, index);
        if (actionPtr != ZR_NULL && *actionPtr != ZR_NULL &&
            (*actionPtr)->title != ZR_NULL &&
            strcmp(test_string_ptr((*actionPtr)->title), title) == 0) {
            return *actionPtr;
        }
    }
    return ZR_NULL;
}

static TZrSize property_refactor_position_offset(
        const TZrChar *content,
        SZrLspPosition position) {
    TZrSize offset = 0U;
    TZrInt32 line = 0;

    while (content != ZR_NULL && content[offset] != '\0' &&
           line < position.line) {
        if (content[offset++] == '\n') {
            line++;
        }
    }
    return offset + (position.character > 0
                             ? (TZrSize)position.character
                             : 0U);
}

static TZrChar *property_refactor_apply_single_edit(
        const TZrChar *content,
        const SZrLspCodeAction *action) {
    SZrLspTextEdit **editPtr;
    const SZrLspTextEdit *edit;
    const TZrChar *newText;
    TZrSize contentLength;
    TZrSize startOffset;
    TZrSize endOffset;
    TZrSize newTextLength;
    TZrChar *updated;

    if (content == ZR_NULL || action == ZR_NULL ||
        action->edits.length != 1U) {
        return ZR_NULL;
    }
    editPtr = (SZrLspTextEdit **)ZrCore_Array_Get(
            (SZrArray *)&action->edits,
            0U);
    edit = editPtr != ZR_NULL ? *editPtr : ZR_NULL;
    newText = edit != ZR_NULL && edit->newText != ZR_NULL
                      ? test_string_ptr(edit->newText)
                      : ZR_NULL;
    if (edit == ZR_NULL || newText == ZR_NULL) {
        return ZR_NULL;
    }
    contentLength = strlen(content);
    startOffset = property_refactor_position_offset(content, edit->range.start);
    endOffset = property_refactor_position_offset(content, edit->range.end);
    if (startOffset > endOffset || endOffset > contentLength) {
        return ZR_NULL;
    }
    newTextLength = strlen(newText);
    updated = (TZrChar *)malloc(
            contentLength - (endOffset - startOffset) + newTextLength + 1U);
    if (updated == ZR_NULL) {
        return ZR_NULL;
    }
    memcpy(updated, content, startOffset);
    memcpy(updated + startOffset, newText, newTextLength);
    memcpy(
            updated + startOffset + newTextLength,
            content + endOffset,
            contentLength - endOffset + 1U);
    return updated;
}

static SZrSymbol *property_refactor_find_symbol(
        SZrSemanticAnalyzer *analyzer,
        EZrSymbolType type,
        const TZrChar *name) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        name == ZR_NULL) {
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
            SZrSymbol *symbol = symbolPtr != ZR_NULL ? *symbolPtr : ZR_NULL;
            if (symbol != ZR_NULL && symbol->type == type &&
                symbol->name != ZR_NULL &&
                strcmp(test_string_ptr(symbol->name), name) == 0) {
                return symbol;
            }
        }
    }
    return ZR_NULL;
}

static void test_lsp_property_refactor_uses_canonical_query(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Property Refactor Uses Canonical Query";
    const TZrChar *uriText = "file:///property_refactor_contract.zr";
    const TZrChar *content =
            "interface ValueContract {\n"
            "    pub property value: int { get; set; }\n"
            "}\n"
            "interface MutableValue : ValueContract { }\n"
            "class Meter : MutableValue {\n"
            "    pri var stored: int = 7;\n"
            "    pub property value: int {\n"
            "        get { return this.stored; }\n"
            "    }\n"
            "}\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition position;
    SZrLspRange range;
    SZrArray actions = {0};
    SZrLspCodeAction *missingAccessorAction;
    SZrLspCodeAction *fieldAction;
    SZrLspTextEdit **missingEditPtr;
    const TZrChar *missingText = ZR_NULL;
    const TZrChar *fieldText = ZR_NULL;
    TZrChar *updated = ZR_NULL;
    SZrSemanticAnalyzer *analyzer;
    SZrSymbol *fieldSymbol;
    SZrSymbol *propertySymbol;
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "PropertyQuery refactors",
            "Missing interface accessors and explicit field proxies must derive from canonical property identity");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            (TZrNativeString)uriText,
            strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                content,
                strlen(content),
                1U) ||
        !lsp_find_position_for_substring(
                content,
                "property value",
                1U,
                9,
                &position)) {
        goto cleanup;
    }
    range.start = position;
    range.end = position;
    if (!ZrLanguageServer_Lsp_GetCodeActions(
                state,
                context,
                uri,
                range,
                &actions)) {
        goto cleanup;
    }
    missingAccessorAction = property_refactor_find_action(
            &actions,
            "Implement required set accessor");
    fieldAction = property_refactor_find_action(
            &actions,
            "Introduce explicit field for property value");
    if (missingAccessorAction == ZR_NULL || fieldAction == ZR_NULL ||
        missingAccessorAction->kind == ZR_NULL ||
        strcmp(test_string_ptr(missingAccessorAction->kind),
               "refactor.rewrite") != 0 ||
        missingAccessorAction->edits.length != 1U ||
        fieldAction->edits.length != 1U) {
        goto cleanup;
    }
    missingEditPtr = (SZrLspTextEdit **)ZrCore_Array_Get(
            &missingAccessorAction->edits,
            0U);
    if (missingEditPtr != ZR_NULL && *missingEditPtr != ZR_NULL &&
        (*missingEditPtr)->newText != ZR_NULL) {
        missingText = test_string_ptr((*missingEditPtr)->newText);
    }
    {
        SZrLspTextEdit **fieldEditPtr =
                (SZrLspTextEdit **)ZrCore_Array_Get(&fieldAction->edits, 0U);
        if (fieldEditPtr != ZR_NULL && *fieldEditPtr != ZR_NULL &&
            (*fieldEditPtr)->newText != ZR_NULL) {
            fieldText = test_string_ptr((*fieldEditPtr)->newText);
        }
    }
    if (missingText == ZR_NULL || strstr(missingText, "set { }") == ZR_NULL ||
        strstr(missingText, "__set_") != ZR_NULL || fieldText == ZR_NULL ||
        strstr(fieldText, "pri var _value: int;") == ZR_NULL ||
        strstr(fieldText, "return this._value;") == ZR_NULL ||
        strstr(fieldText, "this._value = value;") == ZR_NULL ||
        strstr(fieldText, "__get_") != ZR_NULL ||
        strstr(fieldText, "__set_") != ZR_NULL) {
        goto cleanup;
    }
    updated = property_refactor_apply_single_edit(content, fieldAction);
    if (updated == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                updated,
                strlen(updated),
                2U)) {
        goto cleanup;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    fieldSymbol = property_refactor_find_symbol(
            analyzer,
            ZR_SYMBOL_FIELD,
            "_value");
    propertySymbol = property_refactor_find_symbol(
            analyzer,
            ZR_SYMBOL_PROPERTY,
            "value");
    if (fieldSymbol != ZR_NULL && propertySymbol != ZR_NULL &&
        fieldSymbol->semanticId != ZR_SEMANTIC_ID_INVALID &&
        propertySymbol->semanticId != ZR_SEMANTIC_ID_INVALID &&
        fieldSymbol->semanticId != propertySymbol->semanticId) {
        valid = ZR_TRUE;
    }

cleanup:
    if (!valid) {
        TEST_FAIL(
                timer,
                summary,
                "Canonical property refactor actions or independent field identity were unavailable");
    } else {
        TEST_PASS(timer, summary);
    }
    free(updated);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &actions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_property_refactor_rejects_ambiguous_and_ref_contracts(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Property Refactor Rejects Ambiguous And Ref Contracts";
    const TZrChar *uriText = "file:///property_refactor_negative.zr";
    const TZrChar *ambiguousContent =
            "interface Writable { property value: int { get; set; } }\n"
            "interface Initializable { property value: int { get; init; } }\n"
            "class Meter : Writable, Initializable {\n"
            "    property value: int { get { return 7; } }\n"
            "}\n";
    const TZrChar *refContent =
            "class Box {\n"
            "    pri var stored: int = 7;\n"
            "    pub property value: ref int {\n"
            "        get { return ref this.stored; }\n"
            "    }\n"
            "}\n";
    const TZrChar *invalidContent =
            "class Box {\n"
            "    pub property value: int {\n"
            "        get { return 7; }\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrLspPosition position;
    SZrLspRange range;
    SZrArray ambiguousActions = {0};
    SZrArray refActions = {0};
    SZrArray invalidActions = {0};
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Refactor negative boundaries",
            "Ambiguous interface requirements and ref-return properties must not receive storage-producing actions");
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZrCore_String_Create(
            state,
            (TZrNativeString)uriText,
            strlen(uriText));
    if (context == ZR_NULL || uri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                ambiguousContent,
                strlen(ambiguousContent),
                1U) ||
        !lsp_find_position_for_substring(
                ambiguousContent,
                "property value",
                2U,
                9,
                &position)) {
        goto cleanup;
    }
    range.start = position;
    range.end = position;
    if (!ZrLanguageServer_Lsp_GetCodeActions(
                state,
                context,
                uri,
                range,
                &ambiguousActions) ||
        property_refactor_find_action(
                &ambiguousActions,
                "Implement required set accessor") != ZR_NULL ||
        property_refactor_find_action(
                &ambiguousActions,
                "Implement required init accessor") != ZR_NULL) {
        goto cleanup;
    }
    if (!ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                refContent,
                strlen(refContent),
                2U) ||
        !lsp_find_position_for_substring(
                refContent,
                "property value",
                0U,
                9,
                &position)) {
        goto cleanup;
    }
    range.start = position;
    range.end = position;
    if (!ZrLanguageServer_Lsp_GetCodeActions(
                state,
                context,
                uri,
                range,
                &refActions) ||
        property_refactor_find_action(
                &refActions,
                "Introduce explicit field for property value") != ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                uri,
                invalidContent,
                strlen(invalidContent),
                3U) ||
        !lsp_find_position_for_substring(
                invalidContent,
                "property value",
                0U,
                9,
                &position)) {
        goto cleanup;
    }
    range.start = position;
    range.end = position;
    if (ZrLanguageServer_Lsp_GetCodeActions(
                state,
                context,
                uri,
                range,
                &invalidActions) &&
        property_refactor_find_action(
                &invalidActions,
                "Introduce explicit field for property value") == ZR_NULL &&
        property_refactor_find_action(
                &invalidActions,
                "Implement required set accessor") == ZR_NULL) {
        valid = ZR_TRUE;
    }

cleanup:
    if (!valid) {
        TEST_FAIL(
                timer,
                summary,
                "Ambiguous or ref property contracts exposed an unsafe refactor");
    } else {
        TEST_PASS(timer, summary);
    }
    ZrLanguageServer_Lsp_FreeCodeActions(state, &ambiguousActions);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &refActions);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &invalidActions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
