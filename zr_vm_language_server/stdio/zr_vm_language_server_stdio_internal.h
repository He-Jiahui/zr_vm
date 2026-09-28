#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_language_server/lsp_diagnostic_store.h"
#include "zr_vm_language_server/lsp_uri.h"
#include "interface/lsp_workspace_edit_snapshot.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "stdio_frame_reader.h"
#include "stdio_json_rpc.h"
#include "stdio_lifecycle.h"
#include "stdio_request_registry.h"
#include "stdio_server.h"

/* URI 缓存将入站 C 文本映射到 VM 字符串；键独立分配，值由 VM GC 管理。
 * BUG: 长 URI 值仅保存在原生 items 裸指针中，GC 根扫描未覆盖该表；
 * 长 URI 无其他 VM/LSP 根时，后续 GC 可回收该值，缓存命中后返回悬空指针。 */
typedef struct SZrCachedUri {
    char *text;
    SZrString *value;
} SZrCachedUri;

/* 会话级 URI 复用表；items 的有效范围为 [0, count)，扩容只移动原生表项。 */
typedef struct SZrUriCache {
    SZrCachedUri *items;
    size_t count;
    size_t capacity;
} SZrUriCache;

/* 增量同步失败后的文档集合；只借用 VM 中的 URI，完整同步后解除标记。 */
typedef struct SZrDesynchronizedDocumentSet {
    SZrString **items;
    size_t count;
    size_t capacity;
} SZrDesynchronizedDocumentSet;

/* semanticTokens/full 的前次结果，供 delta 比较；URI 和数据均由缓存持有。 */
typedef struct SZrSemanticTokenSnapshot {
    char *uriText;
    char resultId[64];
    TZrUInt32 *data;
    TZrSize length;
} SZrSemanticTokenSnapshot;

/* 客户端增量 token 协议的会话历史，关闭或文档关闭时清除相应快照。 */
typedef struct SZrSemanticTokenCache {
    SZrSemanticTokenSnapshot *items;
    size_t count;
    size_t capacity;
} SZrSemanticTokenCache;

/* 推送诊断的版本和结果 ID，用于抑制相同文档版本的重复通知。 */
typedef struct SZrDiagnosticPushSnapshot {
    char *uriText;
    char resultId[ZR_LSP_DIAGNOSTIC_RESULT_ID_MAX];
    TZrSize documentVersion;
    TZrBool hasDocumentVersion;
} SZrDiagnosticPushSnapshot;

/* 按 URI 保存最近一次已推送诊断；URI 副本归缓存，元素随会话释放。 */
typedef struct SZrDiagnosticPushCache {
    SZrDiagnosticPushSnapshot *items;
    size_t count;
    size_t capacity;
} SZrDiagnosticPushCache;

/* 语义层内部以 UTF-16 定位，入站/出站边界按初始化协商值转换。 */
typedef enum EZrStdioPositionEncoding {
    ZR_STDIO_POSITION_ENCODING_UTF16 = 0,
    ZR_STDIO_POSITION_ENCODING_UTF8 = 1,
} EZrStdioPositionEncoding;

/* trace 只影响 stderr 的诊断输出，不能污染 stdout 的 JSON-RPC 帧。 */
typedef enum EZrStdioTraceLevel {
    ZR_STDIO_TRACE_OFF = 0,
    ZR_STDIO_TRACE_MESSAGES,
    ZR_STDIO_TRACE_VERBOSE,
} EZrStdioTraceLevel;

/* 进度 token 借用当前入站 params；主循环删除消息树前必须 End/Clear。 */
typedef struct SZrStdioRequestProgress {
    const cJSON *workDoneToken;
    const cJSON *partialResultToken;
    TZrBool workDoneBegan;
} SZrStdioRequestProgress;

/* 读线程入队、主线程 Take 的转移单元；message 和预留结果保持同帧配对。 */
typedef struct SZrStdioInboundMessage {
    cJSON *message;
    TZrBool isParseError;
    EZrStdioRequestReservation requestReservation;
    struct SZrStdioInboundMessage *next;
} SZrStdioInboundMessage;

/* 单读线程与主请求线程共用的队列；锁保护链表及关闭标志，input 为借用流。 */
typedef struct SZrStdioRequestInputState {
#ifdef _WIN32
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE messageAvailable;
#else
    pthread_mutex_t lock;
    pthread_cond_t messageAvailable;
#endif
    SZrStdioInboundMessage *head;
    SZrStdioInboundMessage *tail;
    FILE *input;
    TZrBool inputClosed;
    TZrBool stopRequested;
    TZrBool isInitialized;
    TZrBool readerStarted;
#ifdef _WIN32
    HANDLE readerThread;
#else
    pthread_t readerThread;
#endif
} SZrStdioRequestInputState;

/* 会话拥有 VM、LSP 上下文、请求注册表及缓存；activeRequestId 借用当前消息树，
 * 读线程只访问输入队列和注册表，主线程负责请求处理和 stdout 响应。 */
typedef struct SZrStdioServer {
    SZrGlobalState *global;
    SZrState *state;
    SZrLspContext *context;
    SZrUriCache uriCache;
    SZrDesynchronizedDocumentSet desynchronizedDocuments;
    SZrSemanticTokenCache semanticTokenCache;
    SZrDiagnosticPushCache diagnosticPushCache;
    SZrStdioRequestInputState requestInput;
    SZrStdioRequestRegistry *requestRegistry;
    const cJSON *activeRequestId;
    SZrStdioRequestProgress requestProgress;
    EZrStdioPositionEncoding positionEncoding;
    TZrBool supportsInlineCompletion;
    TZrBool supportsRangesFormatting;
    EZrStdioTraceLevel traceLevel;
    EZrStdioServerFaultPoint faultPoint;
    SZrStdioLifecycle lifecycle;
} SZrStdioServer;

/* 字符串及 URI 边界工具：副本由调用者释放，缓存值归会话/VM 所有。 */
char *duplicate_string_range(const char *text, size_t length);
char *duplicate_c_string(const char *text);
int starts_with_case_insensitive(const char *text, const char *prefix);
const char *skip_spaces(const char *text);
char *zr_string_to_c_string(SZrString *value);
SZrString *server_get_cached_uri(SZrStdioServer *server, const char *uriText);
void free_uri_cache(SZrUriCache *cache);
void free_desynchronized_document_set(SZrDesynchronizedDocumentSet *set);

/* 发送状态区分构造失败与可能已写出部分帧的 I/O 失败；失败不可盲目重试。 */
typedef enum EZrStdioSendStatus {
    ZR_STDIO_SEND_OK = 0,
    ZR_STDIO_SEND_BUILD_ERROR,
    ZR_STDIO_SEND_IO_ERROR,
} EZrStdioSendStatus;

/* 输出函数无论状态均消费自有 JSON 参数；id、method 仅借用调用方缓冲。 */
EZrStdioSendStatus send_json_message(cJSON *message);
EZrStdioSendStatus send_result_response(const cJSON *id, cJSON *result);
EZrStdioSendStatus send_error_response(const cJSON *id, int code, const char *messageText);
EZrStdioSendStatus send_notification(const char *method, cJSON *params);
/* 输入线程和请求注册表的生命周期由 Server_New/Free 包围；Take 将 JSON 树移交主线程。 */
TZrBool ZrLanguageServer_StdioRequestInput_Init(SZrStdioServer *server);
TZrBool ZrLanguageServer_StdioRequestInput_Start(SZrStdioServer *server);
void ZrLanguageServer_StdioRequestInput_Stop(SZrStdioServer *server);
void ZrLanguageServer_StdioRequestInput_Join(SZrStdioServer *server);
void ZrLanguageServer_StdioRequestInput_Free(SZrStdioServer *server);
TZrBool ZrLanguageServer_StdioRequestInput_Take(SZrStdioServer *server,
                                                 cJSON **outMessage,
                                                 TZrBool *outIsParseError,
                                                 EZrStdioRequestReservation *outRequestReservation);
void ZrLanguageServer_StdioRequestInput_Activate(SZrStdioServer *server, const cJSON *id);
TZrBool ZrLanguageServer_StdioRequestInput_IsActiveCancelled(SZrStdioServer *server);
void ZrLanguageServer_StdioRequestInput_Complete(SZrStdioServer *server, const cJSON *id);
void ZrLanguageServer_StdioTrace_Set(SZrStdioServer *server, const cJSON *params);
void ZrLanguageServer_StdioTrace_Log(SZrStdioServer *server,
                                     const char *direction,
                                     const char *kind,
                                     const char *method,
                                     TZrBool isNotification);

/* 协议投影函数将 VM 查询结果复制到 cJSON；处理器随后归还 VM 原生结果。 */
cJSON *serialize_position(SZrLspPosition position);
cJSON *serialize_range(SZrLspRange range);
cJSON *serialize_location(const SZrLspLocation *location);
cJSON *serialize_symbol_information(const SZrLspSymbolInformation *info);
cJSON *serialize_diagnostic(const SZrLspDiagnostic *diagnostic);
cJSON *create_completion_commit_characters_array(void);
cJSON *serialize_completion_item(const SZrLspCompletionItem *item);
cJSON *serialize_hover(const SZrLspHover *hover);
cJSON *serialize_rich_hover(const SZrLspRichHover *hover);
cJSON *serialize_signature_help(const SZrLspSignatureHelp *help);
cJSON *serialize_inlay_hints_array(SZrArray *hints);
cJSON *serialize_document_highlight(const SZrLspDocumentHighlight *highlight);
cJSON *serialize_locations_array(SZrArray *locations);
cJSON *serialize_symbols_array(SZrArray *symbols);
cJSON *serialize_diagnostics_array(SZrArray *diagnostics);
cJSON *serialize_diagnostics_array_for_uri(SZrArray *diagnostics, const char *uriText);
cJSON *serialize_completion_items_array(SZrArray *items);
cJSON *serialize_highlights_array(SZrArray *highlights);
cJSON *serialize_folding_ranges_array(SZrArray *ranges);
cJSON *serialize_selection_ranges_array(SZrArray *ranges);
cJSON *serialize_document_links_array(SZrArray *links);
cJSON *serialize_code_lens_array(SZrArray *lenses);
cJSON *serialize_hierarchy_items_array(SZrArray *items);
cJSON *serialize_hierarchy_calls_array(SZrArray *calls, TZrBool outgoing);
cJSON *serialize_text_edit(const SZrLspTextEdit *edit);
cJSON *serialize_text_edits_array(SZrArray *edits);
cJSON *create_workspace_edit_for_locations(SZrStdioServer *server,
                                           SZrArray *locations,
                                           SZrString *newName,
                                           const SZrArray *documentSnapshots);
TZrBool append_workspace_edit_locations(SZrStdioServer *server,
                                        cJSON *edit,
                                        SZrArray *locations,
                                        SZrString *newName,
                                        const SZrArray *documentSnapshots);
cJSON *serialize_code_actions_array(const char *uriText,
                                    const SZrLspWorkspaceEditDocumentSnapshot *documentSnapshot,
                                    SZrArray *actions,
                                    const cJSON *params);

/* 与上面的序列化调用配对；原生数组/对象不随 cJSON 树一同释放。 */
void free_locations_array(SZrState *state, SZrArray *locations);
void free_symbols_array(SZrState *state, SZrArray *symbols);
void free_diagnostics_array(SZrState *state, SZrArray *diagnostics);
void free_completion_items_array(SZrState *state, SZrArray *items);
void free_inlay_hints_array(SZrState *state, SZrArray *hints);
void free_highlights_array(SZrState *state, SZrArray *highlights);
void free_hover(SZrState *state, SZrLspHover *hover);
void free_rich_hover(SZrState *state, SZrLspRichHover *hover);
void free_signature_help(SZrState *state, SZrLspSignatureHelp *help);

/* 客户端坐标先按协商编码解码为内部位置；输出 JSON 在发送前反向编码。 */
int parse_position(const cJSON *json, SZrLspPosition *outPosition);
int parse_range(const cJSON *json, SZrLspRange *outRange);
int parse_position_for_uri(SZrStdioServer *server,
                           SZrString *uri,
                           const cJSON *json,
                           SZrLspPosition *outPosition);
int parse_range_for_uri(SZrStdioServer *server,
                        SZrString *uri,
                        const cJSON *json,
                        SZrLspRange *outRange);
int parse_range_for_content(SZrStdioServer *server,
                            const char *content,
                            size_t contentLength,
                            const cJSON *json,
                            SZrLspRange *outRange);
TZrBool content_change_range_to_byte_offsets(SZrStdioServer *server,
                                             const char *content,
                                             size_t contentLength,
                                             const cJSON *json,
                                             TZrSize *outStartOffset,
                                             TZrSize *outEndOffset,
                                             TZrSize *outClientLength);
void negotiate_position_encoding(SZrStdioServer *server, const cJSON *params);
const char *position_encoding_name(const SZrStdioServer *server);
void apply_position_encoding_to_response(SZrStdioServer *server,
                                         const char *method,
                                         const cJSON *requestParams,
                                         cJSON *response);
void apply_position_encoding_to_json_for_uri(SZrStdioServer *server,
                                             const char *uriText,
                                             cJSON *json);

/* 文档、诊断和 semantic token 辅助入口共享会话缓存；token 历史可随文档关闭移除，
 * 诊断去重快照仍保留到会话销毁。 */
SZrFileVersion *get_file_version_for_uri(SZrStdioServer *server, SZrString *uri);
char *read_document_text_from_uri(SZrString *uri, size_t *outLength);
char *apply_content_changes(SZrStdioServer *server,
                            SZrString *uri,
                            const char *original,
                            size_t originalLength,
                            const cJSON *changes,
                            size_t *outLength);
void publish_diagnostics(SZrStdioServer *server, SZrString *uri);
void publish_empty_diagnostics(SZrStdioServer *server, SZrString *uri);
const cJSON *get_object_item(const cJSON *json, const char *key);
TZrSize parse_size_value(const cJSON *json, TZrSize fallback);
TZrBool parse_size_value_strict(const cJSON *json, TZrSize *outValue);
cJSON *create_semantic_token_legend_json(void);
TZrUInt32 semantic_tokens_value_at(SZrArray *tokens, TZrSize index);
SZrSemanticTokenSnapshot *find_semantic_token_snapshot(SZrStdioServer *server, const char *uriText);
TZrBool upsert_semantic_token_snapshot(SZrStdioServer *server,
                                       const char *uriText,
                                       const char *resultId,
                                       SZrArray *tokens);
void remove_semantic_token_cache_for_uri(SZrStdioServer *server, const char *uriText);
cJSON *serialize_semantic_tokens_result(SZrArray *tokens, const char *resultId);
TZrSize semantic_tokens_previous_result_length(const cJSON *params);
cJSON *serialize_semantic_tokens_delta_result(SZrArray *tokens,
                                              TZrSize previousLength,
                                              const char *previousResultId,
                                              const SZrSemanticTokenSnapshot *previousSnapshot,
                                              const char *resultId);
cJSON *serialize_semantic_tokens_range_result(SZrArray *tokens, SZrLspRange range);
/* semantic token 三种请求均由请求分发表选择，结果树统一交回请求事务发送。 */
SZrLspHandlerResult handle_semantic_tokens_full_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_semantic_tokens_full_delta_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_semantic_tokens_range_request(SZrStdioServer *server, const cJSON *params);
/* URI 取得后交给会话缓存；文档变更通知维护与请求查询同一份内容。 */
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
void mark_document_desynchronized(SZrStdioServer *server, SZrString *uri);
void clear_document_desynchronization(SZrStdioServer *server, SZrString *uri);
TZrBool document_is_desynchronized(SZrStdioServer *server, SZrString *uri);
int update_document_contents_from_disk(SZrStdioServer *server, SZrString *uri);
int handle_did_open(SZrStdioServer *server, const cJSON *params);
int handle_did_change(SZrStdioServer *server, const cJSON *params);
int handle_did_close(SZrStdioServer *server, const cJSON *params);
int handle_did_save(SZrStdioServer *server, const cJSON *params);
void handle_did_change_workspace_folders(SZrStdioServer *server, const cJSON *params);
TZrBool add_workspace_file_operation_capabilities(cJSON *workspace);
int handle_did_change_watched_files(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_will_rename_files_request(SZrStdioServer *server, const cJSON *params);
int handle_did_create_files(SZrStdioServer *server, const cJSON *params);
int handle_did_delete_files(SZrStdioServer *server, const cJSON *params);
int handle_did_rename_files(SZrStdioServer *server, const cJSON *params);

/* 请求总入口持有单个活动 ID；处理器返回状态和 JSON，通知入口只改变会话状态。 */
void handle_request_message(SZrStdioServer *server,
                            const cJSON *id,
                            const char *method,
                            const cJSON *params);
SZrLspHandlerResult handle_initialize_request(SZrStdioServer *server, const cJSON *params);
TZrBool add_advanced_editor_capabilities(SZrStdioServer *server,
                                      const cJSON *params,
                                      cJSON *capabilities);
void handle_zr_selected_project_notification(SZrStdioServer *server, const cJSON *params);
int dispatch_request_method(SZrStdioServer *server,
                            const char *method,
                            const cJSON *params,
                            cJSON **outResult,
                            EZrLspHandlerStatus *outStatus);
void handle_notification_message(SZrStdioServer *server,
                                 const char *method,
                                 const cJSON *params,
                                 int *outShouldExit,
                                 int *outExitCode);

/* 各方法处理器不直接写 stdout；返回值由请求总入口映射为结果或协议错误。 */
SZrLspHandlerResult handle_completion_item_resolve_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_completion_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_hover_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_rich_hover_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_signature_help_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_inlay_hint_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_definition_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_references_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_document_symbols_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_workspace_symbols_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_document_highlights_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_prepare_rename_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_rename_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_native_declaration_document_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_project_modules_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_formatting_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_range_formatting_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_ranges_formatting_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_on_type_formatting_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_code_action_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_code_action_resolve_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_folding_range_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_selection_range_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_linked_editing_range_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_moniker_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_inline_value_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_inline_completion_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_document_link_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_implementation_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_code_lens_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_prepare_call_hierarchy_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_call_hierarchy_incoming_calls_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_call_hierarchy_outgoing_calls_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_prepare_type_hierarchy_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_type_hierarchy_supertypes_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_type_hierarchy_subtypes_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_text_document_diagnostic_request(SZrStdioServer *server, const cJSON *params);
SZrLspHandlerResult handle_workspace_diagnostic_request(SZrStdioServer *server, const cJSON *params);

#endif
