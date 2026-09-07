#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_PROGRESS_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_PROGRESS_H

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/conf.h"

typedef struct SZrStdioServer SZrStdioServer;

void stdio_request_progress_clear(SZrStdioServer *server);
TZrBool stdio_request_progress_prepare(SZrStdioServer *server,
                                        const char *method,
                                        const cJSON *params);
TZrBool stdio_request_progress_begin(SZrStdioServer *server, const char *method);
void stdio_request_progress_end(SZrStdioServer *server);
TZrBool stdio_request_progress_publish_partial_result(SZrStdioServer *server,
                                                      const char *method,
                                                      cJSON **inOutResult);

#endif
