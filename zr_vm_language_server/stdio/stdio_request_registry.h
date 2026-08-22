#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_REGISTRY_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_REGISTRY_H

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/conf.h"

typedef struct SZrStdioRequestRegistry SZrStdioRequestRegistry;

typedef enum EZrStdioRequestReservation {
    ZR_STDIO_REQUEST_RESERVATION_NONE = 0,
    ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
    ZR_STDIO_REQUEST_RESERVATION_DUPLICATE,
    ZR_STDIO_REQUEST_RESERVATION_FAILED,
} EZrStdioRequestReservation;

SZrStdioRequestRegistry *ZrLanguageServer_StdioRequestRegistry_New(void);
void ZrLanguageServer_StdioRequestRegistry_Free(SZrStdioRequestRegistry *registry);
EZrStdioRequestReservation ZrLanguageServer_StdioRequestRegistry_Reserve(
        SZrStdioRequestRegistry *registry,
        const cJSON *id);
TZrBool ZrLanguageServer_StdioRequestRegistry_Cancel(SZrStdioRequestRegistry *registry,
                                                      const cJSON *id);
TZrBool ZrLanguageServer_StdioRequestRegistry_IsCancelled(
        SZrStdioRequestRegistry *registry,
        const cJSON *id);
void ZrLanguageServer_StdioRequestRegistry_Complete(SZrStdioRequestRegistry *registry,
                                                     const cJSON *id);

#endif
