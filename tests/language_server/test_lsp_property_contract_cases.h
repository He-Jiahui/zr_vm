#ifndef ZR_VM_TEST_LSP_PROPERTY_CONTRACT_CASES_H
#define ZR_VM_TEST_LSP_PROPERTY_CONTRACT_CASES_H

static TZrSize property_contract_completion_count(
        SZrArray *completions,
        const TZrChar *label) {
    TZrSize count = 0U;

    for (TZrSize index = 0U;
         completions != ZR_NULL && index < completions->length;
         index++) {
        SZrLspCompletionItem **itemPtr =
                (SZrLspCompletionItem **)ZrCore_Array_Get(completions, index);
        if (itemPtr != ZR_NULL && *itemPtr != ZR_NULL &&
            (*itemPtr)->label != ZR_NULL &&
            strcmp(test_string_ptr((*itemPtr)->label), label) == 0) {
            count++;
        }
    }
    return count;
}

static TZrBool property_contract_has_refactor_action(SZrArray *actions) {
    static const TZrChar *titles[] = {
        "Implement required set accessor",
        "Implement required init accessor",
        "Introduce explicit field for property shared",
    };

    for (TZrSize actionIndex = 0U;
         actions != ZR_NULL && actionIndex < actions->length;
         actionIndex++) {
        SZrLspCodeAction **actionPtr =
                (SZrLspCodeAction **)ZrCore_Array_Get(actions, actionIndex);
        const TZrChar *title = actionPtr != ZR_NULL && *actionPtr != ZR_NULL &&
                                      (*actionPtr)->title != ZR_NULL
                                      ? test_string_ptr((*actionPtr)->title)
                                      : ZR_NULL;
        for (TZrSize titleIndex = 0U;
             title != ZR_NULL && titleIndex < sizeof(titles) / sizeof(titles[0]);
             titleIndex++) {
            if (strcmp(title, titles[titleIndex]) == 0) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static TZrBool property_contract_has_parameter_identity(
        SZrSemanticAnalyzer *analyzer,
        const TZrChar *name,
        TZrSymbolId expectedSymbolId,
        TZrTypeId expectedTypeId) {
    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL ||
        name == ZR_NULL || expectedSymbolId == ZR_SEMANTIC_ID_INVALID ||
        expectedTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
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

            if (symbol != ZR_NULL && symbol->type == ZR_SYMBOL_PARAMETER &&
                symbol->name != ZR_NULL &&
                strcmp(test_string_ptr(symbol->name), name) == 0 &&
                symbol->semanticId == expectedSymbolId &&
                symbol->semanticTypeId == expectedTypeId) {
                return ZR_TRUE;
            }
        }
    }
    return ZR_FALSE;
}

static void test_lsp_unified_property_uses_one_canonical_contract(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Unified Property Uses One Canonical Contract";
    const TZrChar *uriText = "file:///unified_property_contract.zr";
    const TZrChar *content =
            "class Meter {\n"
            "    pri var stored: int = 7;\n"
            "    pub property value: int {\n"
            "        get { return this.stored; }\n"
            "        pri set { this.stored = value; }\n"
            "    }\n"
            "}\n"
            "fn read(meter: Meter): int { return meter.value; }\n";
    SZrTestTimer timer;
    SZrLspContext *context = ZR_NULL;
    SZrString *uri = ZR_NULL;
    SZrArray diagnostics = {0};
    SZrArray definitions = {0};
    SZrArray completions = {0};
    SZrLspHover *hover = ZR_NULL;
    SZrString *placeholder = ZR_NULL;
    SZrLspRange renameRange = {0};
    SZrLspPosition usagePosition;
    SZrLspPosition completionPosition;
    SZrLspPosition valuePosition;
    SZrLspPosition declarationPosition;
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    SZrParserSemanticPropertyQuery propertyQuery = {0};
    SZrFilePosition propertyFilePosition;
    SZrFileRange propertyRange;
    SZrArray semanticTokens = {0};
    TZrBool propertyAvailable = ZR_FALSE;
    TZrBool parameterIdentityAvailable = ZR_FALSE;
    TZrBool semanticTokensAvailable = ZR_FALSE;
    TZrBool parameterTokenAvailable = ZR_FALSE;
    TZrBool diagnosticsAvailable = ZR_FALSE;
    TZrBool hoverAvailable = ZR_FALSE;
    TZrBool completionAvailable = ZR_FALSE;
    TZrBool definitionAvailable = ZR_FALSE;
    TZrBool renameAvailable = ZR_FALSE;
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Canonical property consumer",
            "Unified PropertyDecl must drive hover, completion, definition and rename without hidden accessor spelling");
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
                "meter.value",
                0U,
                6,
                &usagePosition) ||
        !lsp_find_position_for_substring(
                content,
                "meter.value",
                0U,
                6,
                &completionPosition) ||
        !lsp_find_position_for_substring(
                content,
                "property value",
                0U,
                9,
                &declarationPosition) ||
        !lsp_find_position_for_substring(
                content,
                "= value",
                0U,
                2,
                &valuePosition)) {
        goto cleanup;
    }

    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    propertyFilePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context,
            uri,
            declarationPosition);
    propertyRange = ZrParser_FileRange_Create(
            propertyFilePosition,
            propertyFilePosition,
            uri);

    ZrCore_Array_Init(
            state,
            &diagnostics,
            sizeof(SZrLspDiagnostic *),
            4U);
    ZrCore_Array_Init(
            state,
            &definitions,
            sizeof(SZrLspLocation *),
            2U);
    ZrCore_Array_Init(
            state,
            &completions,
            sizeof(SZrLspCompletionItem *),
            8U);
    ZrCore_Array_Init(
            state,
            &semanticTokens,
            sizeof(TZrUInt32),
            32U);
    propertyAvailable =
            analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL &&
            ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                propertyRange,
                ZR_NULL,
                &propertyQuery);
    parameterIdentityAvailable =
            propertyAvailable &&
            propertyQuery.setterValueSymbolId != ZR_SEMANTIC_ID_INVALID &&
            property_contract_has_parameter_identity(
                analyzer,
                "value",
                propertyQuery.setterValueSymbolId,
                propertyQuery.propertyTypeId);
    semanticTokensAvailable = ZrLanguageServer_Lsp_GetSemanticTokens(
                state,
                context,
                uri,
                &semanticTokens);
    parameterTokenAvailable = semanticTokensAvailable && semantic_tokens_contain(
                &semanticTokens,
                valuePosition.line,
                valuePosition.character,
                5,
                "parameter");
    diagnosticsAvailable = ZrLanguageServer_Lsp_GetDiagnostics(
                state,
                context,
                uri,
                &diagnostics);
    hoverAvailable = ZrLanguageServer_Lsp_GetHover(
                state,
                context,
                uri,
                usagePosition,
                &hover);
    completionAvailable = ZrLanguageServer_Lsp_GetCompletion(
                state,
                context,
                uri,
                completionPosition,
                &completions);
    definitionAvailable = ZrLanguageServer_Lsp_GetDefinition(
                state,
                context,
                uri,
                usagePosition,
                &definitions);
    renameAvailable = ZrLanguageServer_Lsp_PrepareRename(
                state,
                context,
                uri,
                usagePosition,
                &renameRange,
                &placeholder);
    if (propertyAvailable &&
        parameterIdentityAvailable &&
        semanticTokensAvailable &&
        parameterTokenAvailable &&
        diagnosticsAvailable &&
        diagnostics.length == 0U &&
        hoverAvailable &&
        hover_contains_text(hover, "property value: int") &&
        hover_contains_text(hover, "access public") &&
        hover_contains_text(hover, "get public") &&
        hover_contains_text(hover, "set private") &&
        hover_contains_text(hover, "receiver readonly") &&
        !hover_contains_text(hover, "__get_") &&
        !hover_contains_text(hover, "__set_") &&
        completionAvailable &&
        property_contract_completion_count(&completions, "value") == 1U &&
        property_contract_completion_count(&completions, "__get_value") == 0U &&
        property_contract_completion_count(&completions, "__set_value") == 0U &&
        definitionAvailable &&
        location_array_contains_range(&definitions, 2, 17, 2, 22) &&
        renameAvailable &&
        placeholder != ZR_NULL &&
        strcmp(test_string_ptr(placeholder), "value") == 0 &&
        lsp_range_equals(renameRange, 7, 42, 7, 47)) {
        valid = ZR_TRUE;
    }

cleanup:
    if (!valid) {
        printf(
                "  property=%d parameterIdentity=%d semanticTokens=%d parameterToken=%d diagnostics=%d diagnosticCount=%zu hover=%d completion=%d definition=%d rename=%d\n",
                propertyAvailable,
                parameterIdentityAvailable,
                semanticTokensAvailable,
                parameterTokenAvailable,
                diagnosticsAvailable,
                (size_t)diagnostics.length,
                hoverAvailable,
                completionAvailable,
                definitionAvailable,
                renameAvailable);
        printf(
                "  completionCount=%zu hiddenGet=%zu hiddenSet=%zu definitionRange=%d placeholder=%s renameRange=%d\n",
                (size_t)property_contract_completion_count(&completions, "value"),
                (size_t)property_contract_completion_count(&completions, "__get_value"),
                (size_t)property_contract_completion_count(&completions, "__set_value"),
                location_array_contains_range(&definitions, 2, 17, 2, 22),
                placeholder != ZR_NULL ? test_string_ptr(placeholder) : "<null>",
                lsp_range_equals(renameRange, 7, 42, 7, 47));
        if (context != ZR_NULL && uri != ZR_NULL) {
            dump_analyzer_state(state, context, uri);
        }
        TEST_FAIL(
                timer,
                summary,
                "Unified property surfaces did not converge on one visible property identity");
    } else {
        TEST_PASS(timer, summary);
    }
    ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
    ZrCore_Array_Free(state, &definitions);
    ZrCore_Array_Free(state, &completions);
    ZrCore_Array_Free(state, &semanticTokens);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

static void test_lsp_binary_property_preserves_canonical_contract(
        SZrState *state) {
    const TZrChar *summary =
            "LSP Binary Property Preserves Canonical Contract";
    const TZrChar *binarySource =
            "pub class Meter {\n"
            "    pub static property shared: int {\n"
            "        get { return 7; }\n"
            "    }\n"
            "}\n";
    const TZrChar *mainContent =
            "var binaryStage = import(\"graph_binary_stage\");\n"
            "var answer = binaryStage.Meter.shared;\n"
            "binaryStage.Meter.\n"
            "return answer;\n";
    SZrTestTimer timer;
    SZrGeneratedBinaryMetadataFixture fixture;
    SZrLspContext *context = ZR_NULL;
    SZrString *mainUri = ZR_NULL;
    SZrString *binaryUri = ZR_NULL;
    SZrLspPosition propertyPosition;
    SZrLspPosition completionPosition;
    SZrLspHover *hover = ZR_NULL;
    SZrArray completions = {0};
    SZrArray definitions = {0};
    SZrArray codeActions = {0};
    SZrSemanticAnalyzer *analyzer = ZR_NULL;
    TZrBool structuredPrototypeRowsAvailable = ZR_FALSE;
    TZrBool canonicalPropertyContractAvailable = ZR_FALSE;
    TZrBool propertyReferenceAvailable = ZR_FALSE;
    TZrBool hoverAvailable = ZR_FALSE;
    TZrBool completionAvailable = ZR_FALSE;
    TZrBool definitionAvailable = ZR_FALSE;
    TZrBool codeActionsAvailable = ZR_FALSE;
    TZrBool valid = ZR_FALSE;

    TEST_START(summary);
    TEST_INFO(
            "Binary PropertyDef consumer",
            "Reloaded property metadata must preserve the visible property label and declaration identity without hidden accessor reconstruction");
    memset(&fixture, 0, sizeof(fixture));
    if (!prepare_generated_binary_metadata_fixture(
                state,
                "interface_binary_property_contract",
                &fixture) ||
        !regenerate_binary_metadata_fixture_artifacts(
                state,
                fixture.binaryPath,
                binarySource) ||
        !write_text_file(
                fixture.mainPath,
                mainContent,
                strlen(mainContent))) {
        TEST_FAIL(timer, summary, "Failed to generate the binary property fixture");
        return;
    }

    context = ZrLanguageServer_LspContext_New(state);
    mainUri = create_file_uri_from_native_path(state, fixture.mainPath);
    binaryUri = create_file_uri_from_native_path(state, fixture.binaryPath);
    if (context == ZR_NULL || mainUri == ZR_NULL || binaryUri == ZR_NULL ||
        !ZrLanguageServer_Lsp_UpdateDocument(
                state,
                context,
                mainUri,
                mainContent,
                strlen(mainContent),
                1U) ||
        !lsp_find_position_for_substring(
                mainContent,
                "Meter.shared",
                0U,
                6,
                &propertyPosition) ||
        !lsp_find_position_for_substring(
                mainContent,
                "binaryStage.Meter.",
                1U,
                18,
                &completionPosition)) {
        goto cleanup;
    }

    ZrCore_Array_Init(
            state,
            &completions,
            sizeof(SZrLspCompletionItem *),
            8U);
    ZrCore_Array_Init(
            state,
            &definitions,
            sizeof(SZrLspLocation *),
            4U);
    hoverAvailable = ZrLanguageServer_Lsp_GetHover(
                state,
                context,
                mainUri,
                propertyPosition,
                &hover);
    completionAvailable = ZrLanguageServer_Lsp_GetCompletion(
                state,
                context,
                mainUri,
                completionPosition,
                &completions);
    definitionAvailable = ZrLanguageServer_Lsp_GetDefinition(
                state,
                context,
                mainUri,
                propertyPosition,
                &definitions);
    {
        SZrLspRange propertyRange;

        propertyRange.start = propertyPosition;
        propertyRange.end = propertyPosition;
        codeActionsAvailable = ZrLanguageServer_Lsp_GetCodeActions(
                state,
                context,
                mainUri,
                propertyRange,
                &codeActions);
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, mainUri);
    if (analyzer != ZR_NULL && analyzer->semanticContext != ZR_NULL) {
        SZrFilePosition propertyFilePosition =
                ZrLanguageServer_Lsp_GetDocumentFilePosition(
                        context,
                        mainUri,
                        propertyPosition);
        SZrFileRange propertyRange = ZrParser_FileRange_Create(
                propertyFilePosition,
                propertyFilePosition,
                mainUri);
        SZrParserSemanticPropertyQuery query;

        propertyReferenceAvailable = ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                propertyRange,
                ZR_NULL,
                &query);
    }
    if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL) {
        for (TZrSize prototypeIndex = 0U;
             prototypeIndex < analyzer->compilerState->typePrototypes.length;
             prototypeIndex++) {
            SZrTypePrototypeInfo *prototype =
                    (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                            &analyzer->compilerState->typePrototypes,
                            prototypeIndex);
            SZrTypeMemberInfo *visibleProperty = ZR_NULL;
            TZrBool hasGetter = ZR_FALSE;

            if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
                strcmp(test_string_ptr(prototype->name), "Meter") != 0) {
                continue;
            }
            for (TZrSize memberIndex = 0U;
                 memberIndex < prototype->members.length;
                 memberIndex++) {
                SZrTypeMemberInfo *member =
                        (SZrTypeMemberInfo *)ZrCore_Array_Get(
                                &prototype->members,
                                memberIndex);
                if (member == ZR_NULL ||
                    member->propertyIdentity == UINT32_MAX) {
                    continue;
                }
                if (member->memberType == ZR_AST_PROPERTY_DECLARATION &&
                    member->accessorRole == ZR_PROPERTY_ACCESSOR_ROLE_NONE) {
                    visibleProperty = member;
                }
            }
            if (visibleProperty != ZR_NULL) {
                for (TZrSize memberIndex = 0U;
                     memberIndex < prototype->members.length;
                     memberIndex++) {
                    SZrTypeMemberInfo *member =
                            (SZrTypeMemberInfo *)ZrCore_Array_Get(
                                    &prototype->members,
                                    memberIndex);
                    if (member != ZR_NULL &&
                        member->propertyIdentity ==
                                visibleProperty->propertyIdentity &&
                        member->accessorRole ==
                                ZR_PROPERTY_ACCESSOR_ROLE_GET) {
                        hasGetter = ZR_TRUE;
                        break;
                    }
                }
            }
            structuredPrototypeRowsAvailable =
                    visibleProperty != ZR_NULL && hasGetter &&
                    prototype->importModuleName != ZR_NULL &&
                    strcmp(
                            test_string_ptr(prototype->importModuleName),
                            "graph_binary_stage") == 0;
            if (structuredPrototypeRowsAvailable &&
                visibleProperty->propertySymbolId !=
                        ZR_SEMANTIC_ID_INVALID &&
                visibleProperty->propertyValueTypeId !=
                        ZR_SEMANTIC_ID_INVALID) {
                SZrParserSemanticPropertyQuery query;

                canonicalPropertyContractAvailable =
                        ZrParser_SemanticQuery_PropertyBySymbolId(
                                analyzer->semanticContext,
                                visibleProperty->propertySymbolId,
                                &query) &&
                        query.propertyTypeId ==
                                visibleProperty->propertyValueTypeId &&
                        query.getterSymbolId != ZR_SEMANTIC_ID_INVALID;
            }
            break;
        }
    }
    if (structuredPrototypeRowsAvailable &&
        canonicalPropertyContractAvailable &&
        propertyReferenceAvailable &&
        hoverAvailable &&
        hover_contains_text(hover, "property shared: int") &&
        !hover_contains_text(hover, "__get_") &&
        !hover_contains_text(hover, "__set_") &&
        completionAvailable &&
        property_contract_completion_count(&completions, "shared") == 1U &&
        property_contract_completion_count(&completions, "__get_shared") == 0U &&
        definitionAvailable &&
        location_array_contains_uri_text(
                &definitions,
                test_string_ptr(binaryUri)) &&
        codeActionsAvailable &&
        !property_contract_has_refactor_action(&codeActions)) {
        valid = ZR_TRUE;
    }

cleanup:
    if (!valid) {
        if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL) {
            for (TZrSize prototypeIndex = 0U;
                 prototypeIndex < analyzer->compilerState->typePrototypes.length;
                 prototypeIndex++) {
                SZrTypePrototypeInfo *prototype =
                        (SZrTypePrototypeInfo *)ZrCore_Array_Get(
                                &analyzer->compilerState->typePrototypes,
                                prototypeIndex);
                if (prototype == ZR_NULL || prototype->name == ZR_NULL ||
                    strcmp(test_string_ptr(prototype->name), "Meter") != 0) {
                    continue;
                }
                printf("  Meter prototype members=%zu importedNative=%d importModule=%s\n",
                       (size_t)prototype->members.length,
                       prototype->isImportedNative,
                       prototype->importModuleName != ZR_NULL
                               ? test_string_ptr(prototype->importModuleName)
                               : "<null>");
                for (TZrSize memberIndex = 0U;
                     memberIndex < prototype->members.length;
                     memberIndex++) {
                    SZrTypeMemberInfo *member =
                            (SZrTypeMemberInfo *)ZrCore_Array_Get(
                                    &prototype->members,
                                    memberIndex);
                    printf(
                            "  member[%zu]=%s type=%d static=%d propertyIdentity=%u role=%d symbol=%llu valueType=%llu declaration=%p\n",
                            (size_t)memberIndex,
                            member != ZR_NULL && member->name != ZR_NULL
                                    ? test_string_ptr(member->name)
                                    : "<null>",
                            member != ZR_NULL ? (int)member->memberType : -1,
                            member != ZR_NULL ? member->isStatic : 0,
                            member != ZR_NULL ? member->propertyIdentity : 0U,
                            member != ZR_NULL ? member->accessorRole : 0,
                            (unsigned long long)(member != ZR_NULL
                                    ? member->propertySymbolId
                                    : 0U),
                            (unsigned long long)(member != ZR_NULL
                                    ? member->propertyValueTypeId
                                    : 0U),
                            (void *)(member != ZR_NULL
                                    ? member->declarationNode
                                    : ZR_NULL));
                }
            }
        }
        printf(
                "  structuredRows=%d canonicalProperty=%d propertyReference=%d hover=%d hoverProperty=%d completion=%d shared=%zu hiddenGet=%zu definition=%d binaryUri=%d propertyRefactor=%d\n",
                structuredPrototypeRowsAvailable,
                canonicalPropertyContractAvailable,
                propertyReferenceAvailable,
                hoverAvailable,
                hover_contains_text(hover, "property shared: int"),
                completionAvailable,
                (size_t)property_contract_completion_count(&completions, "shared"),
                (size_t)property_contract_completion_count(&completions, "__get_shared"),
                definitionAvailable,
                binaryUri != ZR_NULL && location_array_contains_uri_text(
                        &definitions,
                        test_string_ptr(binaryUri)),
                property_contract_has_refactor_action(&codeActions));
        TEST_FAIL(
                timer,
                summary,
                "Binary property hover, completion, or declaration projection was unavailable");
    } else {
        TEST_PASS(timer, summary);
    }
    ZrCore_Array_Free(state, &completions);
    ZrCore_Array_Free(state, &definitions);
    ZrLanguageServer_Lsp_FreeCodeActions(state, &codeActions);
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}

#endif
