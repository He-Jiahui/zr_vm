#include "zr_vm_language_server_stdio_internal.h"

static int execute_command_is_advertised(const char *command) {
    return command != NULL &&
           (strcmp(command, ZR_LSP_COMMAND_RUN_CURRENT_PROJECT) == 0 ||
            strcmp(command, ZR_LSP_COMMAND_SHOW_REFERENCES) == 0);
}

cJSON *handle_execute_command_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *commandJson;
    const char *commandText;

    ZR_UNUSED_PARAMETER(server);

    commandJson = get_object_item(params, ZR_LSP_FIELD_COMMAND);
    if (!cJSON_IsString((cJSON *)commandJson)) {
        return NULL;
    }

    commandText = cJSON_GetStringValue((cJSON *)commandJson);
    if (!execute_command_is_advertised(commandText)) {
        return cJSON_CreateNull();
    }

    return cJSON_CreateNull();
}
