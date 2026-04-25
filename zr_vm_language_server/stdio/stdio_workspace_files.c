#include "zr_vm_language_server_stdio_internal.h"

TZrBool ZrLanguageServer_LspProject_RemoveProjectByProjectUri(SZrState *state,
                                                              SZrLspContext *context,
                                                              SZrString *uri);
TZrBool ZrLanguageServer_LspProject_RemoveFileRecordByUri(SZrState *state,
                                                          SZrLspContext *context,
                                                          SZrString *uri);
TZrBool ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(SZrState *state,
                                                                     SZrLspContext *context,
                                                                     SZrString *uri);

static cJSON *create_file_operation_registration(void) {
    cJSON *registration = cJSON_CreateObject();
    cJSON *filters = cJSON_CreateArray();
    cJSON *filter = cJSON_CreateObject();
    cJSON *pattern = cJSON_CreateObject();

    if (registration == NULL || filters == NULL || filter == NULL || pattern == NULL) {
        cJSON_Delete(registration);
        cJSON_Delete(filters);
        cJSON_Delete(filter);
        cJSON_Delete(pattern);
        return NULL;
    }

    cJSON_AddStringToObject(pattern, ZR_LSP_FIELD_GLOB, "**/*.{zr,zrp,zro,dll,so,dylib}");
    cJSON_AddItemToObject(filter, ZR_LSP_FIELD_PATTERN, pattern);
    cJSON_AddItemToArray(filters, filter);
    cJSON_AddItemToObject(registration, ZR_LSP_FIELD_FILTERS, filters);
    return registration;
}

void add_workspace_file_operation_capabilities(cJSON *workspace) {
    cJSON *fileOperations;

    if (workspace == NULL) {
        return;
    }

    fileOperations = cJSON_CreateObject();
    if (fileOperations == NULL) {
        return;
    }

    cJSON_AddItemToObject(fileOperations, ZR_LSP_FIELD_WILL_CREATE, create_file_operation_registration());
    cJSON_AddItemToObject(fileOperations, ZR_LSP_FIELD_DID_CREATE, create_file_operation_registration());
    cJSON_AddItemToObject(fileOperations, ZR_LSP_FIELD_WILL_RENAME, create_file_operation_registration());
    cJSON_AddItemToObject(fileOperations, ZR_LSP_FIELD_DID_RENAME, create_file_operation_registration());
    cJSON_AddItemToObject(fileOperations, ZR_LSP_FIELD_WILL_DELETE, create_file_operation_registration());
    cJSON_AddItemToObject(fileOperations, ZR_LSP_FIELD_DID_DELETE, create_file_operation_registration());
    cJSON_AddItemToObject(workspace, ZR_LSP_FIELD_FILE_OPERATIONS, fileOperations);
}

static TZrBool workspace_file_string_ends_with(SZrString *value, const TZrChar *suffix) {
    TZrNativeString text;
    TZrSize length;
    TZrSize suffixLength;

    if (value == ZR_NULL || suffix == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        text = ZrCore_String_GetNativeStringShort(value);
        length = value->shortStringLength;
    } else {
        text = ZrCore_String_GetNativeString(value);
        length = value->longStringLength;
    }

    suffixLength = strlen(suffix);
    return text != ZR_NULL && length >= suffixLength &&
           memcmp(text + length - suffixLength, suffix, suffixLength) == 0;
}

static TZrBool workspace_file_uri_has_metadata_extension(SZrString *uri) {
    return workspace_file_string_ends_with(uri, ZR_VM_BINARY_MODULE_FILE_EXTENSION) ||
           workspace_file_string_ends_with(uri, ".dll") ||
           workspace_file_string_ends_with(uri, ".so") ||
           workspace_file_string_ends_with(uri, ".dylib");
}

static int handle_single_workspace_file_change(SZrStdioServer *server, SZrString *uri, TZrSize changeType) {
    if (server == ZR_NULL || uri == ZR_NULL) {
        return 0;
    }

    if (changeType == 3) {
        if (workspace_file_string_ends_with(uri, ".zrp")) {
            ZrLanguageServer_LspProject_RemoveProjectByProjectUri(server->state, server->context, uri);
            publish_empty_diagnostics(server, uri);
            return 1;
        }

        if (workspace_file_string_ends_with(uri, ".zr")) {
            ZrLanguageServer_LspProject_RemoveFileRecordByUri(server->state, server->context, uri);
            publish_empty_diagnostics(server, uri);
            return 1;
        }

        if (workspace_file_uri_has_metadata_extension(uri)) {
            return ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(server->state,
                                                                                server->context,
                                                                                uri)
                       ? 1
                       : 0;
        }

        return 1;
    }

    if (workspace_file_string_ends_with(uri, ".zrp") || workspace_file_string_ends_with(uri, ".zr")) {
        return update_document_contents_from_disk(server, uri);
    }

    if (workspace_file_uri_has_metadata_extension(uri)) {
        return ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(server->state,
                                                                            server->context,
                                                                            uri)
                   ? 1
                   : 0;
    }

    return 1;
}

int handle_did_change_watched_files(SZrStdioServer *server, const cJSON *params) {
    const cJSON *changes;
    int handledAny = 0;

    if (server == ZR_NULL || params == NULL) {
        return 0;
    }

    changes = get_object_item(params, ZR_LSP_FIELD_CHANGES);
    if (!cJSON_IsArray((cJSON *)changes)) {
        return 0;
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)changes); index++) {
        const cJSON *change = cJSON_GetArrayItem((cJSON *)changes, index);
        const cJSON *uriJson = get_object_item(change, ZR_LSP_FIELD_URI);
        const cJSON *typeJson = get_object_item(change, ZR_LSP_FIELD_TYPE);
        const char *uriText;
        SZrString *uri;
        TZrSize changeType;

        if (!cJSON_IsString((cJSON *)uriJson) || !cJSON_IsNumber((cJSON *)typeJson)) {
            continue;
        }

        uriText = cJSON_GetStringValue((cJSON *)uriJson);
        if (uriText == NULL) {
            continue;
        }

        uri = server_get_cached_uri(server, uriText);
        if (uri == ZR_NULL) {
            continue;
        }

        changeType = parse_size_value(typeJson, 0);
        if (changeType == 0) {
            continue;
        }

        handledAny = handle_single_workspace_file_change(server, uri, changeType) || handledAny;
    }

    return handledAny;
}

static int handle_file_operation_uri(SZrStdioServer *server, const cJSON *uriJson, TZrSize changeType) {
    const char *uriText;
    SZrString *uri;

    if (!cJSON_IsString((cJSON *)uriJson)) {
        return 0;
    }

    uriText = cJSON_GetStringValue((cJSON *)uriJson);
    if (uriText == NULL) {
        return 0;
    }

    uri = server_get_cached_uri(server, uriText);
    if (uri == ZR_NULL) {
        return 0;
    }

    return handle_single_workspace_file_change(server, uri, changeType);
}

static int handle_file_operation_list(SZrStdioServer *server,
                                      const cJSON *params,
                                      TZrSize changeType,
                                      const char *uriField) {
    const cJSON *files;
    int handledAny = 0;

    if (server == ZR_NULL || params == NULL || uriField == NULL) {
        return 0;
    }

    files = get_object_item(params, ZR_LSP_FIELD_FILES);
    if (!cJSON_IsArray((cJSON *)files)) {
        return 0;
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)files); index++) {
        const cJSON *file = cJSON_GetArrayItem((cJSON *)files, index);
        handledAny = handle_file_operation_uri(server, get_object_item(file, uriField), changeType) || handledAny;
    }

    return handledAny;
}

int handle_did_create_files(SZrStdioServer *server, const cJSON *params) {
    return handle_file_operation_list(server, params, 1, ZR_LSP_FIELD_URI);
}

cJSON *handle_will_create_files_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    ZR_UNUSED_PARAMETER(params);
    return cJSON_CreateNull();
}

int handle_did_delete_files(SZrStdioServer *server, const cJSON *params) {
    return handle_file_operation_list(server, params, 3, ZR_LSP_FIELD_URI);
}

cJSON *handle_will_delete_files_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    ZR_UNUSED_PARAMETER(params);
    return cJSON_CreateNull();
}

int handle_did_rename_files(SZrStdioServer *server, const cJSON *params) {
    const cJSON *files;
    int handledAny = 0;

    if (server == ZR_NULL || params == NULL) {
        return 0;
    }

    files = get_object_item(params, ZR_LSP_FIELD_FILES);
    if (!cJSON_IsArray((cJSON *)files)) {
        return 0;
    }

    for (int index = 0; index < cJSON_GetArraySize((cJSON *)files); index++) {
        const cJSON *file = cJSON_GetArrayItem((cJSON *)files, index);
        handledAny = handle_file_operation_uri(server, get_object_item(file, ZR_LSP_FIELD_OLD_URI), 3) || handledAny;
        handledAny = handle_file_operation_uri(server, get_object_item(file, ZR_LSP_FIELD_NEW_URI), 1) || handledAny;
    }

    return handledAny;
}

cJSON *handle_will_rename_files_request(SZrStdioServer *server, const cJSON *params) {
    ZR_UNUSED_PARAMETER(server);
    ZR_UNUSED_PARAMETER(params);
    return cJSON_CreateNull();
}
