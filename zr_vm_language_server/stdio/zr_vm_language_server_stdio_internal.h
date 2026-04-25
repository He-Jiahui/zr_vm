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
cJSON *serialize_rich_hover(const SZrLspRichHover *hover);
cJSON *serialize_signature_help(const SZrLspSignatureHelp *help);
cJSON *serialize_inlay_hints_array(SZrArray *hints);
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
void free_inlay_hints_array(SZrState *state, SZrArray *hints);
void free_highlights_array(SZrState *state, SZrArray *highlights);
void free_hover(SZrState *state, SZrLspHover *hover);
void free_rich_hover(SZrState *state, SZrLspRichHover *hover);
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
cJSON *handle_inlay_hint_resolve_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_workspace_symbol_resolve_request(SZrStdioServer *server, const cJSON *params);
cJSON *create_semantic_token_legend_json(void);
cJSON *handle_semantic_tokens_full_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_semantic_tokens_full_delta_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_semantic_tokens_range_request(SZrStdioServer *server, const cJSON *params);
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
void add_workspace_file_operation_capabilities(cJSON *workspace);
int handle_did_change_watched_files(SZrStdioServer *server, const cJSON *params);
cJSON *handle_will_create_files_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_will_rename_files_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_will_delete_files_request(SZrStdioServer *server, const cJSON *params);
int handle_did_create_files(SZrStdioServer *server, const cJSON *params);
int handle_did_delete_files(SZrStdioServer *server, const cJSON *params);
int handle_did_rename_files(SZrStdioServer *server, const cJSON *params);

void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params);
int dispatch_request_method(SZrStdioServer *server,
                            const char *method,
                            const cJSON *params,
                            cJSON **outResult);
void handle_notification_message(SZrStdioServer *server,
                                 const char *method,
                                 const cJSON *params,
                                 int *outShouldExit,
                                 int *outExitCode);

void add_advanced_editor_capabilities(cJSON *capabilities);
cJSON *handle_completion_item_resolve_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_completion_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_hover_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_rich_hover_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_signature_help_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_inlay_hint_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_definition_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_references_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_document_symbols_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_workspace_symbols_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_document_highlights_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_prepare_rename_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_rename_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_native_declaration_document_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_project_modules_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_formatting_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_range_formatting_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_ranges_formatting_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_on_type_formatting_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_code_action_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_code_action_resolve_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_folding_range_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_selection_range_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_linked_editing_range_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_moniker_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_inline_value_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_inline_completion_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_document_color_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_color_presentation_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_document_link_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_document_link_resolve_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_declaration_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_type_definition_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_implementation_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_code_lens_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_code_lens_resolve_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_execute_command_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_prepare_call_hierarchy_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_call_hierarchy_incoming_calls_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_call_hierarchy_outgoing_calls_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_prepare_type_hierarchy_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_type_hierarchy_supertypes_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_type_hierarchy_subtypes_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_text_document_diagnostic_request(SZrStdioServer *server, const cJSON *params);
cJSON *handle_workspace_diagnostic_request(SZrStdioServer *server, const cJSON *params);

#endif
