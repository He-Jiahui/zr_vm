#include "zr_vm_language_server_stdio_internal.h"
#include "project/lsp_project_internal.h"

TZrBool ZrLanguageServer_LspWorkspace_CanProcessFileEvent(SZrLspContext *context,
                                                           SZrString *uri);

static TZrBool desynchronized_document_set_contains(
        const SZrDesynchronizedDocumentSet *set,
        const SZrString *uri) {
    size_t index;

    if (set == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < set->count; index++) {
        if (set->items[index] == uri ||
            ZrLanguageServer_LspUri_Equivalent(set->items[index], (SZrString *)uri)) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static void desynchronized_document_set_add(
        SZrDesynchronizedDocumentSet *set,
        SZrString *uri) {
    SZrString **items;
    size_t capacity;

    if (set == ZR_NULL || uri == ZR_NULL || desynchronized_document_set_contains(set, uri)) {
        return;
    }
    if (set->count == set->capacity) {
        capacity = set->capacity == 0 ? 4U : set->capacity * 2U;
        items = (SZrString **)realloc(set->items, capacity * sizeof(SZrString *));
        if (items == ZR_NULL) {
            return;
        }
        set->items = items;
        set->capacity = capacity;
    }
    set->items[set->count++] = uri;
}

static void desynchronized_document_set_remove(
        SZrDesynchronizedDocumentSet *set,
        const SZrString *uri) {
    size_t index;

    if (set == ZR_NULL || uri == ZR_NULL) {
        return;
    }
    for (index = 0; index < set->count; index++) {
        if (set->items[index] == uri ||
            ZrLanguageServer_LspUri_Equivalent(set->items[index], (SZrString *)uri)) {
            if (index + 1U < set->count) {
                memmove(&set->items[index],
                        &set->items[index + 1U],
                        (set->count - index - 1U) * sizeof(SZrString *));
            }
            set->count--;
            return;
        }
    }
}

void mark_document_desynchronized(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(server, uri);

    if (fileVersion != ZR_NULL) {
        fileVersion->isDesynchronized = ZR_TRUE;
    }
    if (server != ZR_NULL) {
        desynchronized_document_set_add(&server->desynchronizedDocuments, uri);
    }
}

void clear_document_desynchronization(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(server, uri);

    if (fileVersion != ZR_NULL) {
        fileVersion->isDesynchronized = ZR_FALSE;
    }
    if (server != ZR_NULL) {
        desynchronized_document_set_remove(&server->desynchronizedDocuments, uri);
    }
}

TZrBool document_is_desynchronized(SZrStdioServer *server, SZrString *uri) {
    SZrFileVersion *fileVersion = get_file_version_for_uri(server, uri);

    return (fileVersion != ZR_NULL && fileVersion->isDesynchronized) ||
           (server != ZR_NULL &&
            desynchronized_document_set_contains(&server->desynchronizedDocuments, uri));
}

static TZrBool content_changes_is_single_full_replacement(const cJSON *changes) {
    const cJSON *change;

    if (!cJSON_IsArray((cJSON *)changes) || cJSON_GetArraySize((cJSON *)changes) != 1) {
        return ZR_FALSE;
    }
    change = cJSON_GetArrayItem((cJSON *)changes, 0);
    return cJSON_IsObject((cJSON *)change) &&
           cJSON_IsString(get_object_item(change, ZR_LSP_FIELD_TEXT)) &&
           get_object_item(change, ZR_LSP_FIELD_RANGE) == ZR_NULL &&
           get_object_item(change, ZR_LSP_FIELD_RANGE_LENGTH) == ZR_NULL;
}

int get_uri_from_text_document(SZrStdioServer *server,
                               const cJSON *params,
                               const char **outUriText,
                               SZrString **outUri) {
    const cJSON *textDocument;
    const cJSON *uriJson;
    const char *uriText;

    if (server == ZR_NULL || params == NULL || outUriText == NULL || outUri == NULL) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    uriJson = get_object_item(textDocument, ZR_LSP_FIELD_URI);
    if (!cJSON_IsString((cJSON *)uriJson)) {
        return 0;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return 0;
    }

    *outUriText = uriText;
    *outUri = server_get_cached_uri(server, uriText);
    return *outUri != ZR_NULL;
}

int get_uri_and_position(SZrStdioServer *server,
                         const cJSON *params,
                         const char **outUriText,
                         SZrString **outUri,
                         SZrLspPosition *outPosition) {
    const cJSON *positionJson;

    if (!get_uri_from_text_document(server, params, outUriText, outUri) || outPosition == NULL) {
        return 0;
    }

    positionJson = get_object_item(params, ZR_LSP_FIELD_POSITION);
    return parse_position_for_uri(server, *outUri, positionJson, outPosition);
}

int handle_did_open(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *textJson;
    const cJSON *versionJson;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    const char *text;
    TZrSize version;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    textJson = get_object_item(textDocument, ZR_LSP_FIELD_TEXT);
    if (!cJSON_IsString((cJSON *)textJson)) {
        return 0;
    }

    versionJson = get_object_item(textDocument, ZR_LSP_FIELD_VERSION);
    if (!parse_size_value_strict(versionJson, &version)) {
        return 0;
    }
    text = cJSON_GetStringValue((cJSON *)textJson);
    if (text == NULL) {
        text = "";
    }
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        return 0;
    }

    if (!update_document_contents(server, uri, text, strlen(text), version)) {
        return 0;
    }
    clear_document_desynchronization(server, uri);
    return 1;
}

int handle_did_change(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *versionJson;
    const cJSON *changes;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *originalContent;
    size_t originalLength;
    char *updatedContent;
    size_t updatedLength = 0;
    TZrSize version;
    TZrBool isFullReplacement;
    int success;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    versionJson = get_object_item(textDocument, ZR_LSP_FIELD_VERSION);
    changes = get_object_item(params, ZR_LSP_FIELD_CONTENT_CHANGES);
    if (!parse_size_value_strict(versionJson, &version) ||
        !cJSON_IsArray((cJSON *)changes) || cJSON_GetArraySize((cJSON *)changes) == 0) {
        mark_document_desynchronized(server, uri);
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion == ZR_NULL || !fileVersion->isOpenDocument) {
        mark_document_desynchronized(server, uri);
        return 0;
    }
    isFullReplacement = content_changes_is_single_full_replacement(changes);
    if (fileVersion->isDesynchronized && !isFullReplacement) {
        return 0;
    }
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        originalContent = snapshot.content;
        originalLength = snapshot.contentLength;
        if (version <= snapshot.version) {
            ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
            mark_document_desynchronized(server, uri);
            return 0;
        }
    } else {
        mark_document_desynchronized(server, uri);
        return 0;
    }

    updatedContent = apply_content_changes(server, uri, originalContent, originalLength, changes, &updatedLength);
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    if (updatedContent == NULL) {
        mark_document_desynchronized(server, uri);
        return 0;
    }

    success = update_document_contents(server, uri, updatedContent, updatedLength, version);
    free(updatedContent);
    if (!success) {
        mark_document_desynchronized(server, uri);
    } else if (isFullReplacement) {
        clear_document_desynchronization(server, uri);
    }
    return success;
}

int handle_did_close(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    remove_semantic_token_cache_for_uri(server, uriText);
    clear_document_desynchronization(server, uri);
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL) {
        fileVersion->isOpenDocument = ZR_FALSE;
    }
    if (ZrLanguageServer_LspWorkspace_CanProcessFileEvent(server->context, uri) &&
        update_document_contents_from_disk(server, uri)) {
        fileVersion = get_file_version_for_uri(server, uri);
        if (fileVersion != ZR_NULL) {
            fileVersion->isOpenDocument = ZR_FALSE;
            clear_document_desynchronization(server, uri);
            return 1;
        }
    }
    publish_empty_diagnostics(server, uri);
    while (ZrLanguageServer_LspProject_RemoveFileRecordByUri(server->state, server->context, uri)) {
        /* A shared source can be registered in more than one project index. */
    }
    {
        SZrTypeValue key;
        SZrHashKeyValuePair *pair;

        if (server->context != ZR_NULL) {
            if (server->context->parser != ZR_NULL) {
                ZrLanguageServer_IncrementalParser_RemoveFile(server->state, server->context->parser, uri);
            }

            ZrCore_Value_InitAsRawObject(server->state, &key, &uri->super);
            pair = ZrCore_HashSet_Find(server->state, &server->context->uriToAnalyzerMap, &key);
            if (pair != ZR_NULL && pair->value.type == ZR_VALUE_TYPE_NATIVE_POINTER) {
                SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)pair->value.value.nativeObject.nativePointer;
                if (analyzer != ZR_NULL) {
                    ZrLanguageServer_SemanticAnalyzer_Free(server->state, analyzer);
                }
            }
            ZrCore_HashSet_Remove(server->state, &server->context->uriToAnalyzerMap, &key);
        }
    }
    return 1;
}

int handle_did_save(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textJson;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textJson = get_object_item(params, ZR_LSP_FIELD_TEXT);
    if (cJSON_IsString((cJSON *)textJson)) {
        fileVersion = get_file_version_for_uri(server, uri);
        if (fileVersion != ZR_NULL) {
            publish_diagnostics(server, uri);
        }
        return 1;
    }

    if (textJson != ZR_NULL && !cJSON_IsNull((cJSON *)textJson)) {
        return 0;
    }
    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->isOpenDocument) {
        publish_diagnostics(server, uri);
        return 1;
    }
    return update_document_contents_from_disk(server, uri);
}
