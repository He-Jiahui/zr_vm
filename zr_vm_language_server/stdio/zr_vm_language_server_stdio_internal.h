#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

typedef struct SZrCachedUri {
    char *text;
    SZrString *value;
} SZrCachedUri;

typedef struct SZrUriCache {
    SZrCachedUri *items;
    size_t count;
    size_t capacity;
} SZrUriCache;

typedef struct SZrStdioServer {
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrUriCache uriCache;
    TZrBool shutdownRequested;
} SZrStdioServer;

char *duplicate_string_range(const char *text, size_t length);
char *duplicate_c_string(const char *text);
int starts_with_case_insensitive(const char *text, const char *prefix);
const char *skip_spaces(const char *text);
char *zr_string_to_c_string(SZrString *value);
SZrString *server_get_cached_uri(SZrStdioServer *server, const char *uriText);
void free_uri_cache(SZrUriCache *cache);

void send_json_message(cJSON *message);
void send_result_response(const cJSON *id, cJSON *result);
void send_error_response(const cJSON *id, int code, const char *messageText);
void send_notification(const char *method, cJSON *params);
char *read_message_payload(size_t *outLength);

cJSON *serialize_position(SZrLspPosition position);
cJSON *serialize_range(SZrLspRange range);
cJSON *serialize_location(const SZrLspLocation *location);
cJSON *serialize_symbol_information(const SZrLspSymbolInformation *info);
cJSON *serialize_diagnostic(const SZrLspDiagnostic *diagnostic);
cJSON *serialize_completion_item(const SZrLspCompletionItem *item);
cJSON *serialize_hover(const SZrLspHover *hover);
cJSON *serialize_signature_help(const SZrLspSignatureHelp *help);
cJSON *serialize_document_highlight(const SZrLspDocumentHighlight *highlight);
cJSON *serialize_locations_array(SZrArray *locations);
cJSON *serialize_symbols_array(SZrArray *symbols);
cJSON *serialize_diagnostics_array(SZrArray *diagnostics);
cJSON *serialize_completion_items_array(SZrArray *items);
cJSON *serialize_highlights_array(SZrArray *highlights);

void free_locations_array(SZrState *state, SZrArray *locations);
void free_symbols_array(SZrState *state, SZrArray *symbols);
void free_diagnostics_array(SZrState *state, SZrArray *diagnostics);
void free_completion_items_array(SZrState *state, SZrArray *items);
void free_highlights_array(SZrState *state, SZrArray *highlights);
void free_hover(SZrState *state, SZrLspHover *hover);
void free_signature_help(SZrState *state, SZrLspSignatureHelp *help);

int parse_position(const cJSON *json, SZrLspPosition *outPosition);
int parse_range(const cJSON *json, SZrLspRange *outRange);

SZrFileVersion *get_file_version_for_uri(SZrStdioServer *server, SZrString *uri);
char *apply_content_changes(SZrString *uri,
                            const char *original,
                            size_t originalLength,
                            const cJSON *changes,
                            size_t *outLength);
void publish_diagnostics(SZrStdioServer *server, SZrString *uri);
void publish_empty_diagnostics(SZrStdioServer *server, SZrString *uri);
const cJSON *get_object_item(const cJSON *json, const char *key);
TZrSize parse_size_value(const cJSON *json, TZrSize fallback);
int get_uri_from_text_document(SZrStdioServer *server,
                               const cJSON *params,
                               const char **outUriText,
                               SZrString **outUri);
int get_uri_and_position(SZrStdioServer *server,
                         const cJSON *params,
                         const char **outUriText,
                         SZrString **outUri,
                         SZrLspPosition *outPosition);
int update_document_contents(SZrStdioServer *server,
                             SZrString *uri,
                             const char *content,
                             size_t contentLength,
                             TZrSize version);
int update_document_contents_from_disk(SZrStdioServer *server, SZrString *uri);

void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params);
void handle_notification_message(SZrStdioServer *server,
                                 const char *method,
                                 const cJSON *params,
                                 int *outShouldExit,
                                 int *outExitCode);

#endif
