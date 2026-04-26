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

void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params) {
    cJSON *result = NULL;

    if (server == ZR_NULL || id == NULL || method == NULL) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_INITIALIZE) == 0) {
        result = handle_initialize_request(server, params);
        send_result_response(id, result != NULL ? result : cJSON_CreateNull());
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_SHUTDOWN) == 0) {
        server->shutdownRequested = ZR_TRUE;
        send_result_response(id, NULL);
        return;
    }

    if (!dispatch_request_method(server, method, params, &result)) {
        send_error_response(id, ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE, "Method not found");
        return;
    }

    if (result == NULL) {
        char message[160];
        snprintf(message, sizeof(message), "Invalid params for %s", method);
        send_error_response(id, ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE, message);
    } else {
        send_result_response(id, result);
    }
}

void handle_notification_message(SZrStdioServer *server,
                                 const char *method,
                                 const cJSON *params,
                                 int *outShouldExit,
                                 int *outExitCode) {
    if (outShouldExit != NULL) {
        *outShouldExit = 0;
    }
    if (outExitCode != NULL) {
        *outExitCode = 0;
    }

    if (server == ZR_NULL || method == NULL) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_INITIALIZED) == 0 ||
        strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_CONFIGURATION) == 0 ||
        strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS) == 0 ||
        strcmp(method, ZR_LSP_METHOD_CANCEL_REQUEST) == 0) {
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_ZR_SELECTED_PROJECT) == 0) {
        handle_zr_selected_project_notification(server, params);
        return;
    }

    if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_OPEN) == 0) {
        handle_did_open(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE) == 0) {
        handle_did_change(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE) == 0) {
        handle_did_close(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_TEXT_DOCUMENT_DID_SAVE) == 0) {
        handle_did_save(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WATCHED_FILES) == 0) {
        handle_did_change_watched_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_CREATE_FILES) == 0) {
        handle_did_create_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_RENAME_FILES) == 0) {
        handle_did_rename_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_WORKSPACE_DID_DELETE_FILES) == 0) {
        handle_did_delete_files(server, params);
    } else if (strcmp(method, ZR_LSP_METHOD_EXIT) == 0) {
        if (outShouldExit != NULL) {
            *outShouldExit = 1;
        }
        if (outExitCode != NULL) {
            *outExitCode = server->shutdownRequested ? 0 : 1;
        }
    }
}
