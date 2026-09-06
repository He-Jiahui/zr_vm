#ifndef ZR_VM_LANGUAGE_SERVER_WASM_RESPONSE_H
#define ZR_VM_LANGUAGE_SERVER_WASM_RESPONSE_H

#include "cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returned text is caller-owned. SuccessResponse always consumes data. */
const char *ZrLanguageServer_Wasm_ErrorResponse(int code, const char *message);
const char *ZrLanguageServer_Wasm_SuccessResponse(cJSON *data);

#ifdef __cplusplus
}
#endif

#endif
