#include "zr_vm_language_server_stdio_internal.h"

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
    return parse_position(positionJson, outPosition);
}

int handle_did_open(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *textJson;
    const cJSON *versionJson;
    const char *uriText;
    SZrString *uri;
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
    version = parse_size_value(versionJson, 0);
    text = cJSON_GetStringValue((cJSON *)textJson);
    if (text == NULL) {
        text = "";
    }

    return update_document_contents(server, uri, text, strlen(text), version);
}

int handle_did_change(SZrStdioServer *server, const cJSON *params) {
    const cJSON *textDocument;
    const cJSON *versionJson;
    const cJSON *changes;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    const char *originalContent;
    size_t originalLength;
    char *updatedContent;
    size_t updatedLength = 0;
    TZrSize version;
    int success;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    textDocument = get_object_item(params, ZR_LSP_FIELD_TEXT_DOCUMENT);
    versionJson = get_object_item(textDocument, ZR_LSP_FIELD_VERSION);
    changes = get_object_item(params, ZR_LSP_FIELD_CONTENT_CHANGES);
    if (!cJSON_IsArray((cJSON *)changes)) {
        return 0;
    }

    fileVersion = get_file_version_for_uri(server, uri);
    if (fileVersion != ZR_NULL && fileVersion->content != ZR_NULL) {
        originalContent = fileVersion->content;
        originalLength = (size_t)fileVersion->contentLength;
        version = parse_size_value(versionJson, fileVersion->version + 1);
    } else {
        originalContent = "";
        originalLength = 0;
        version = parse_size_value(versionJson, 0);
    }

    updatedContent = apply_content_changes(uri, originalContent, originalLength, changes, &updatedLength);
    if (updatedContent == NULL) {
        return 0;
    }

    success = update_document_contents(server, uri, updatedContent, updatedLength, version);
    free(updatedContent);
    return success;
}

int handle_did_close(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return 0;
    }

    remove_semantic_token_cache_for_uri(server, uriText);
    publish_empty_diagnostics(server, uri);
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
        const char *text = cJSON_GetStringValue((cJSON *)textJson);
        fileVersion = get_file_version_for_uri(server, uri);
        return update_document_contents(
            server,
            uri,
            text != NULL ? text : "",
            text != NULL ? strlen(text) : 0,
            fileVersion != ZR_NULL ? fileVersion->version : 0
        );
    }

    publish_diagnostics(server, uri);
    return 1;
}
