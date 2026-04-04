#include "zr_vm_language_server_stdio_internal.h"

static cJSON *duplicate_id(const cJSON *id) {
    if (id == NULL) {
        return cJSON_CreateNull();
    }
    return cJSON_Duplicate(id, 1);
}

void send_json_message(cJSON *message) {
    char *payload;
    size_t payloadLength;

    if (message == NULL) {
        return;
    }

    payload = cJSON_PrintUnformatted(message);
    if (payload == NULL) {
        cJSON_Delete(message);
        return;
    }

    payloadLength = strlen(payload);
    fprintf(stdout, "%s %zu\r\n\r\n", ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX, payloadLength);
    fwrite(payload, 1, payloadLength, stdout);
    fflush(stdout);

    free(payload);
    cJSON_Delete(message);
}

void send_result_response(const cJSON *id, cJSON *result) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL) {
        cJSON_Delete(result);
        return;
    }

    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION);
    cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_ID, duplicate_id(id));
    if (result == NULL) {
        cJSON_AddNullToObject(message, ZR_LSP_JSON_RPC_FIELD_RESULT);
    } else {
        cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_RESULT, result);
    }

    send_json_message(message);
}

void send_error_response(const cJSON *id, int code, const char *messageText) {
    cJSON *message = cJSON_CreateObject();
    cJSON *errorObject = cJSON_CreateObject();

    if (message == NULL || errorObject == NULL) {
        cJSON_Delete(message);
        cJSON_Delete(errorObject);
        return;
    }

    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION);
    cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_ID, duplicate_id(id));
    cJSON_AddNumberToObject(errorObject, ZR_LSP_JSON_RPC_FIELD_CODE, code);
    cJSON_AddStringToObject(
            errorObject, ZR_LSP_JSON_RPC_FIELD_MESSAGE, messageText != NULL ? messageText : "Unknown error");
    cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_ERROR, errorObject);

    send_json_message(message);
}

void send_notification(const char *method, cJSON *params) {
    cJSON *message = cJSON_CreateObject();

    if (message == NULL) {
        cJSON_Delete(params);
        return;
    }

    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_JSONRPC, ZR_LSP_JSON_RPC_VERSION);
    cJSON_AddStringToObject(message, ZR_LSP_JSON_RPC_FIELD_METHOD, method);
    if (params == NULL) {
        cJSON_AddNullToObject(message, ZR_LSP_JSON_RPC_FIELD_PARAMS);
    } else {
        cJSON_AddItemToObject(message, ZR_LSP_JSON_RPC_FIELD_PARAMS, params);
    }

    send_json_message(message);
}

char *read_message_payload(size_t *outLength) {
    char headerLine[ZR_LSP_STDIO_HEADER_BUFFER_LENGTH];
    size_t contentLength = 0;
    int sawHeader = 0;

    if (outLength == NULL) {
        return NULL;
    }
    *outLength = 0;

    while (fgets(headerLine, sizeof(headerLine), stdin) != NULL) {
        size_t lineLength = strlen(headerLine);
        sawHeader = 1;

        if (lineLength == 0 || strcmp(headerLine, "\n") == 0 || strcmp(headerLine, "\r\n") == 0) {
            break;
        }

        if (starts_with_case_insensitive(headerLine, ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX)) {
            const char *valueText =
                    skip_spaces(headerLine + strlen(ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX));
            contentLength = (size_t)strtoul(valueText, NULL, 10);
        }
    }

    if (!sawHeader || contentLength == 0) {
        return NULL;
    }

    {
        char *payload = (char *)malloc(contentLength + 1);
        size_t totalRead = 0;

        if (payload == NULL) {
            return NULL;
        }

        while (totalRead < contentLength) {
            size_t readNow = fread(payload + totalRead, 1, contentLength - totalRead, stdin);
            if (readNow == 0) {
                free(payload);
                return NULL;
            }
            totalRead += readNow;
        }

        payload[contentLength] = '\0';
        *outLength = contentLength;
        return payload;
    }
}
