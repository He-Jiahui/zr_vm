/**
 * @file
 * @brief 共享核心、stdio 和 WASM 对 LSP 协议字面量与资源上限的约定。
 * @note 协议字段与方法名同时用于响应序列化和请求分派；修改时须核对两端及客户端扩展。
 */

#ifndef ZR_VM_LANGUAGE_SERVER_CONF_H
#define ZR_VM_LANGUAGE_SERVER_CONF_H

#include "zr_vm_common.h"

/** @brief 对外声明沿用仓库统一导出修饰符，供核心库及适配层共享头文件。 */
#define ZR_LANGUAGE_SERVER_API ZR_API

/** @brief stdio 侧 URI、诊断和语义 token 缓存扩容时共用的倍率。 */
#define ZR_LSP_DYNAMIC_CAPACITY_GROWTH_FACTOR 2U

/**
 * @brief 项目索引和查询容器的初始容量；SMALL 宏还限制联动编辑固定数组的结果数。
 * @note 历史文档与历史语义快照共用代际窗口。TODO: SMALL 一值两用，需核对是否拆出独立上限，避免调优初始容量时改变响应数量。
 */
#define ZR_LSP_PROJECT_INDEX_INITIAL_CAPACITY 2U
#define ZR_LSP_ARRAY_INITIAL_CAPACITY 8U
#define ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY 4U
#define ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY 16U
#define ZR_LSP_FILE_VERSION_HISTORICAL_CONTENT_CAPACITY 2U
#define ZR_LSP_HISTORICAL_SEMANTIC_SNAPSHOT_CAPACITY \
    ZR_LSP_FILE_VERSION_HISTORICAL_CONTENT_CAPACITY
/** @brief LRU 语义缓存的默认内存预算；实际驱逐由快照缓存层执行。 */
#define ZR_LSP_SEMANTIC_CACHE_DEFAULT_LIMIT_BYTES \
    (256ULL * 1024ULL * 1024ULL)
#define ZR_LSP_GLOBAL_SCOPE_SYMBOL_INITIAL_CAPACITY ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY

/** @brief URI 索引及内容／AST 指纹共用的哈希配置；改变后需复核增量缓存失效路径。 */
#define ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2 4U
#define ZR_LSP_HASH_MULTIPLIER 31ULL

/**
 * @brief 从 native 声明投影泛型参数时的本地缓冲上限。
 * @note 超额参数或文本由解析调用点截断，不能将该常量理解为语言语法上限。
 */
#define ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX 8U
#define ZR_LSP_NATIVE_GENERIC_TEXT_MAX 128U

/** @brief hover 与导航文案使用的栈上 Markdown 缓冲区容量。 */
#define ZR_LSP_MARKDOWN_BUFFER_SIZE ZR_VM_PATH_LENGTH_MAX
/**
 * @brief 当源码偏移不可用时，将行列范围转为候选长度评分的行权重。
 * @note 供 signature help 和引用选择比较候选范围，并非真实字符数或协议位置编码。
 * TODO: 超过 4095 列的行可能破坏回退评分的长度顺序；需针对长行调用与引用匹配补测试，
 *       再决定是否统一改用源码偏移或无溢出的行列比较。
 */
#define ZR_LSP_SIGNATURE_RANGE_PACK_BASE 4096U

/** @brief 类型成员展开与 AST 遍历的递归／栈深保护；到达上限时相应功能会停止深入。 */
#define ZR_LSP_MEMBER_RECURSION_MAX_DEPTH 8U
#define ZR_LSP_AST_RECURSION_MAX_DEPTH 32U

/**
 * @brief 编辑器文案及元数据投影使用的固定栈缓冲容量。
 * @note 容量按用途分级，不代表协议文本的最大合法长度；调用点决定截断或失败语义。
 */
#define ZR_LSP_SHORT_TEXT_BUFFER_LENGTH 32U
#define ZR_LSP_INTEGER_BUFFER_LENGTH 64U
#define ZR_LSP_TYPE_BUFFER_LENGTH 128U
#define ZR_LSP_DETAIL_BUFFER_LENGTH 192U
#define ZR_LSP_TEXT_BUFFER_LENGTH 256U
#define ZR_LSP_LONG_TEXT_BUFFER_LENGTH 512U
#define ZR_LSP_HOVER_BUFFER_LENGTH 1024U
/** @brief stdio Content-Length 帧读取的头部数量、头部字节及消息体上限。 */
#define ZR_LSP_MAX_HEADER_BYTES (8U * 1024U)
#define ZR_LSP_MAX_HEADER_COUNT 32U
#define ZR_LSP_MAX_MESSAGE_BYTES (16U * 1024U * 1024U)
/** @brief 文档、声明投影及编辑文案的本地缓冲容量，与 stdio 帧大小限制无关。 */
#define ZR_LSP_DOCUMENTATION_BUFFER_LENGTH 2048U

/** @brief 注释关联扫描的回溯窗口和文档缓冲，与全文件语义分析范围不同。 */
#define ZR_LSP_COMMENT_SCAN_LINE_LIMIT 32U
#define ZR_LSP_COMMENT_BUFFER_LENGTH ZR_LSP_HOVER_BUFFER_LENGTH
/** @brief 语义 token 结果数组的首次分配容量，不限制实际 token 数。 */
#define ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY 32U
/** @brief 数值范围展示时最多拼接的区间数，避免 hover 文案过长。 */
#define ZR_LSP_NUMERIC_RANGE_SEGMENT_DISPLAY_LIMIT 4U

/** @brief stdio 帧头解析使用的协议前缀；JSON 安全整数界限约束版本和请求数字。 */
#define ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX "Content-Length:"
#define ZR_LSP_JSON_SAFE_INTEGER_MAX 9007199254740991.0

/** @brief JSON-RPC 2.0 请求、通知及响应的字段键和版本字面量；各字段按消息形状使用。 */
#define ZR_LSP_JSON_RPC_FIELD_JSONRPC "jsonrpc"
#define ZR_LSP_JSON_RPC_VERSION "2.0"
#define ZR_LSP_JSON_RPC_FIELD_ID "id"
#define ZR_LSP_JSON_RPC_FIELD_METHOD "method"
#define ZR_LSP_JSON_RPC_FIELD_PARAMS "params"
#define ZR_LSP_JSON_RPC_FIELD_RESULT "result"
#define ZR_LSP_JSON_RPC_FIELD_ERROR "error"
#define ZR_LSP_JSON_RPC_FIELD_CODE "code"
#define ZR_LSP_JSON_RPC_FIELD_MESSAGE "message"

/**
 * @brief LSP 位置、诊断与通用对象的 JSON 字段键。
 * @note 部分字段由 stdio 和 WASM 共用；输入解析与输出组包按各自消息形状使用字段。
 */
#define ZR_LSP_FIELD_LINE "line"
#define ZR_LSP_FIELD_CHARACTER "character"
#define ZR_LSP_FIELD_START "start"
#define ZR_LSP_FIELD_END "end"
#define ZR_LSP_FIELD_URI "uri"
#define ZR_LSP_FIELD_RANGE "range"
#define ZR_LSP_FIELD_RANGE_LENGTH "rangeLength"
#define ZR_LSP_FIELD_NAME "name"
#define ZR_LSP_FIELD_KIND "kind"
#define ZR_LSP_FIELD_LOCATION "location"
#define ZR_LSP_FIELD_CONTAINER_NAME "containerName"
#define ZR_LSP_FIELD_SEVERITY "severity"
#define ZR_LSP_FIELD_SOURCE "source"
#define ZR_LSP_FIELD_CODE ZR_LSP_JSON_RPC_FIELD_CODE
#define ZR_LSP_FIELD_MESSAGE ZR_LSP_JSON_RPC_FIELD_MESSAGE
#define ZR_LSP_FIELD_CODE_DESCRIPTION "codeDescription"
#define ZR_LSP_FIELD_HREF "href"
#define ZR_LSP_FIELD_RELATED_INFORMATION "relatedInformation"
#define ZR_LSP_FIELD_FIXES "fixes"
#define ZR_LSP_FIELD_DESCRIPTOR_ID "descriptorId"
#define ZR_LSP_FIELD_NO_FIX_REASON "noFixReason"
#define ZR_LSP_FIELD_APPLICABILITY "applicability"
#define ZR_LSP_FIELD_TYPE "type"
#define ZR_LSP_FIELD_LABEL "label"
#define ZR_LSP_FIELD_LABEL_DETAILS "labelDetails"
#define ZR_LSP_FIELD_DETAIL "detail"
#define ZR_LSP_FIELD_VALUE "value"
#define ZR_LSP_FIELD_WORK_DONE_TOKEN "workDoneToken"
#define ZR_LSP_FIELD_PARTIAL_RESULT_TOKEN "partialResultToken"
#define ZR_LSP_FIELD_TOKEN "token"
#define ZR_LSP_FIELD_CANCELLABLE "cancellable"
#define ZR_LSP_FIELD_ROLE "role"
#define ZR_LSP_FIELD_SECTIONS "sections"
#define ZR_LSP_FIELD_DOCUMENTATION "documentation"
#define ZR_LSP_FIELD_INSERT_TEXT "insertText"
#define ZR_LSP_FIELD_INSERT_TEXT_FORMAT "insertTextFormat"
#define ZR_LSP_FIELD_FILTER_TEXT "filterText"
#define ZR_LSP_FIELD_SORT_TEXT "sortText"
#define ZR_LSP_FIELD_COMMIT_CHARACTERS "commitCharacters"
#define ZR_LSP_FIELD_CONTENTS "contents"
#define ZR_LSP_FIELD_PARAMETERS "parameters"
#define ZR_LSP_FIELD_SIGNATURES "signatures"
#define ZR_LSP_FIELD_ACTIVE_SIGNATURE "activeSignature"
#define ZR_LSP_FIELD_ACTIVE_PARAMETER "activeParameter"
#define ZR_LSP_FIELD_TEXT_DOCUMENT "textDocument"
#define ZR_LSP_FIELD_TEXT "text"
#define ZR_LSP_FIELD_VERSION "version"
/** @brief 文档代际和快照身份字段供编辑请求在应用前验证旧状态。 */
#define ZR_LSP_FIELD_CONTENT_HASH "contentHash"
#define ZR_LSP_FIELD_CONTENT_LENGTH "contentLength"
#define ZR_LSP_FIELD_CONTENT_GENERATION "contentGeneration"
#define ZR_LSP_FIELD_SEMANTIC_IDENTITY "semanticIdentity"
#define ZR_LSP_FIELD_SYMBOL_ID "symbolId"
#define ZR_LSP_FIELD_TYPE_ID "typeId"
#define ZR_LSP_FIELD_DOCUMENT_GENERATION "documentGeneration"
#define ZR_LSP_FIELD_PROJECT_GENERATION "projectGeneration"
#define ZR_LSP_FIELD_PROVIDER_GENERATION "providerGeneration"
#define ZR_LSP_FIELD_SEMANTIC_GENERATION "semanticGeneration"
#define ZR_LSP_FIELD_DEPENDENCY_FINGERPRINT "dependencyFingerprint"
#define ZR_LSP_FIELD_IS_OPEN_DOCUMENT "isOpenDocument"
/** @brief 推送诊断通知及编辑上下文中的诊断列表字段，并非快照代际标识。 */
#define ZR_LSP_FIELD_DIAGNOSTICS "diagnostics"
/** @brief 文档编辑、请求上下文及语义 token 的通用协议字段。 */
#define ZR_LSP_FIELD_POSITION "position"
#define ZR_LSP_FIELD_NEW_TEXT "newText"
#define ZR_LSP_FIELD_CHANGES "changes"
#define ZR_LSP_FIELD_DOCUMENT_CHANGES "documentChanges"
#define ZR_LSP_FIELD_CONTENT_CHANGES "contentChanges"
#define ZR_LSP_FIELD_CONTEXT "context"
#define ZR_LSP_FIELD_ONLY "only"
#define ZR_LSP_FIELD_INCLUDE_DECLARATION "includeDeclaration"
#define ZR_LSP_FIELD_QUERY "query"
#define ZR_LSP_FIELD_TOKEN_TYPES "tokenTypes"
#define ZR_LSP_FIELD_TOKEN_MODIFIERS "tokenModifiers"
#define ZR_LSP_FIELD_DATA "data"
#define ZR_LSP_FIELD_SNAPSHOT "snapshot"
#define ZR_LSP_FIELD_DISABLED "disabled"
#define ZR_LSP_FIELD_REASON "reason"
#define ZR_LSP_FIELD_PLACEHOLDER "placeholder"
#define ZR_LSP_FIELD_NEW_NAME "newName"
/** @brief initialize 的客户端参数及服务端能力响应共用字段区；输出能力须与请求分派和后端支持一致。 */
#define ZR_LSP_FIELD_OPEN_CLOSE "openClose"
#define ZR_LSP_FIELD_CHANGE "change"
#define ZR_LSP_FIELD_SAVE "save"
#define ZR_LSP_FIELD_WILL_SAVE_WAIT_UNTIL "willSaveWaitUntil"
#define ZR_LSP_FIELD_INCLUDE_TEXT "includeText"
#define ZR_LSP_FIELD_RESOLVE_PROVIDER "resolveProvider"
#define ZR_LSP_FIELD_TRIGGER_CHARACTERS "triggerCharacters"
#define ZR_LSP_FIELD_ALL_COMMIT_CHARACTERS "allCommitCharacters"
#define ZR_LSP_FIELD_PREPARE_PROVIDER "prepareProvider"
#define ZR_LSP_FIELD_CAPABILITIES "capabilities"
#define ZR_LSP_FIELD_GENERAL "general"
#define ZR_LSP_FIELD_POSITION_ENCODING "positionEncoding"
#define ZR_LSP_FIELD_POSITION_ENCODINGS "positionEncodings"
#define ZR_LSP_FIELD_TEXT_DOCUMENT_SYNC "textDocumentSync"
#define ZR_LSP_FIELD_COMPLETION_PROVIDER "completionProvider"
#define ZR_LSP_FIELD_HOVER_PROVIDER "hoverProvider"
#define ZR_LSP_FIELD_SIGNATURE_HELP_PROVIDER "signatureHelpProvider"
#define ZR_LSP_FIELD_DEFINITION_PROVIDER "definitionProvider"
#define ZR_LSP_FIELD_REFERENCES_PROVIDER "referencesProvider"
#define ZR_LSP_FIELD_RENAME_PROVIDER "renameProvider"
#define ZR_LSP_FIELD_DOCUMENT_SYMBOL_PROVIDER "documentSymbolProvider"
#define ZR_LSP_FIELD_WORKSPACE_SYMBOL_PROVIDER "workspaceSymbolProvider"
#define ZR_LSP_FIELD_DOCUMENT_HIGHLIGHT_PROVIDER "documentHighlightProvider"
#define ZR_LSP_FIELD_INLAY_HINT_PROVIDER "inlayHintProvider"
#define ZR_LSP_FIELD_LEGEND "legend"
#define ZR_LSP_FIELD_FULL "full"
#define ZR_LSP_FIELD_DELTA "delta"
#define ZR_LSP_FIELD_SEMANTIC_TOKENS_PROVIDER "semanticTokensProvider"
#define ZR_LSP_FIELD_SUPPORTED "supported"
#define ZR_LSP_FIELD_CHANGE_NOTIFICATIONS "changeNotifications"
#define ZR_LSP_FIELD_WORKSPACE_FOLDERS "workspaceFolders"
#define ZR_LSP_FIELD_WORKSPACE "workspace"
#define ZR_LSP_FIELD_ROOT_URI "rootUri"
#define ZR_LSP_FIELD_ROOT_PATH "rootPath"
/** @brief 工作区变更输入、文件操作能力、诊断及编辑器结果共用的对象字段。 */
#define ZR_LSP_FIELD_EVENT "event"
#define ZR_LSP_FIELD_ADDED "added"
#define ZR_LSP_FIELD_REMOVED "removed"
#define ZR_LSP_FIELD_FILE_OPERATIONS "fileOperations"
#define ZR_LSP_FIELD_DID_CREATE "didCreate"
#define ZR_LSP_FIELD_WILL_RENAME "willRename"
#define ZR_LSP_FIELD_DID_RENAME "didRename"
#define ZR_LSP_FIELD_DID_DELETE "didDelete"
#define ZR_LSP_FIELD_FILTERS "filters"
#define ZR_LSP_FIELD_PATTERN "pattern"
#define ZR_LSP_FIELD_GLOB "glob"
#define ZR_LSP_FIELD_FILES "files"
#define ZR_LSP_FIELD_OLD_URI "oldUri"
#define ZR_LSP_FIELD_NEW_URI "newUri"
#define ZR_LSP_FIELD_SERVER_INFO "serverInfo"
#define ZR_LSP_FIELD_SOURCE_KIND "sourceKind"
#define ZR_LSP_FIELD_IS_ENTRY "isEntry"
#define ZR_LSP_FIELD_MODULE_NAME "moduleName"
#define ZR_LSP_FIELD_DISPLAY_NAME "displayName"
#define ZR_LSP_FIELD_DESCRIPTION "description"
#define ZR_LSP_FIELD_NAVIGATION_URI "navigationUri"
#define ZR_LSP_FIELD_PADDING_LEFT "paddingLeft"
#define ZR_LSP_FIELD_PADDING_RIGHT "paddingRight"
#define ZR_LSP_FIELD_TITLE "title"
#define ZR_LSP_FIELD_COMMAND "command"
#define ZR_LSP_FIELD_ARGUMENT "argument"
#define ZR_LSP_FIELD_ARGUMENTS "arguments"
#define ZR_LSP_FIELD_EDIT "edit"
#define ZR_LSP_FIELD_CODE_ACTION_KINDS "codeActionKinds"
#define ZR_LSP_FIELD_DOCUMENT_FORMATTING_PROVIDER "documentFormattingProvider"
#define ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER "documentRangeFormattingProvider"
#define ZR_LSP_FIELD_DOCUMENT_ON_TYPE_FORMATTING_PROVIDER "documentOnTypeFormattingProvider"
#define ZR_LSP_FIELD_FIRST_TRIGGER_CHARACTER "firstTriggerCharacter"
#define ZR_LSP_FIELD_MORE_TRIGGER_CHARACTER "moreTriggerCharacter"
#define ZR_LSP_FIELD_CH "ch"
#define ZR_LSP_FIELD_CODE_ACTION_PROVIDER "codeActionProvider"
#define ZR_LSP_FIELD_FOLDING_RANGE_PROVIDER "foldingRangeProvider"
#define ZR_LSP_FIELD_SELECTION_RANGE_PROVIDER "selectionRangeProvider"
#define ZR_LSP_FIELD_LINKED_EDITING_RANGE_PROVIDER "linkedEditingRangeProvider"
#define ZR_LSP_FIELD_MONIKER_PROVIDER "monikerProvider"
#define ZR_LSP_FIELD_INLINE_VALUE_PROVIDER "inlineValueProvider"
#define ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER "inlineCompletionProvider"
#define ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER "documentLinkProvider"
#define ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER "implementationProvider"
#define ZR_LSP_FIELD_CODE_LENS_PROVIDER "codeLensProvider"
#define ZR_LSP_FIELD_CALL_HIERARCHY_PROVIDER "callHierarchyProvider"
#define ZR_LSP_FIELD_TYPE_HIERARCHY_PROVIDER "typeHierarchyProvider"
#define ZR_LSP_FIELD_DIAGNOSTIC_PROVIDER "diagnosticProvider"
#define ZR_LSP_FIELD_WORKSPACE_DIAGNOSTICS "workspaceDiagnostics"
#define ZR_LSP_FIELD_RANGES "ranges"
#define ZR_LSP_FIELD_WORD_PATTERN "wordPattern"
#define ZR_LSP_FIELD_SCHEME "scheme"
#define ZR_LSP_FIELD_IDENTIFIER "identifier"
#define ZR_LSP_FIELD_UNIQUE "unique"
#define ZR_LSP_FIELD_VARIABLE_NAME "variableName"
#define ZR_LSP_FIELD_CASE_SENSITIVE_LOOKUP "caseSensitiveLookup"
#define ZR_LSP_FIELD_TEXT_EDIT "textEdit"
#define ZR_LSP_FIELD_INTER_FILE_DEPENDENCIES "interFileDependencies"
#define ZR_LSP_FIELD_START_LINE "startLine"
#define ZR_LSP_FIELD_START_CHARACTER "startCharacter"
#define ZR_LSP_FIELD_END_LINE "endLine"
#define ZR_LSP_FIELD_END_CHARACTER "endCharacter"
#define ZR_LSP_FIELD_PARENT "parent"
#define ZR_LSP_FIELD_TARGET "target"
#define ZR_LSP_FIELD_TOOLTIP "tooltip"
#define ZR_LSP_FIELD_IS_PREFERRED "isPreferred"
#define ZR_LSP_FIELD_ITEMS "items"
#define ZR_LSP_FIELD_RESULT_ID "resultId"
#define ZR_LSP_FIELD_PREVIOUS_RESULT_ID "previousResultId"
#define ZR_LSP_FIELD_PREVIOUS_RESULT_IDS "previousResultIds"
#define ZR_LSP_FIELD_EDITS "edits"
#define ZR_LSP_FIELD_DELETE_COUNT "deleteCount"
#define ZR_LSP_FIELD_SELECTION_RANGE "selectionRange"
#define ZR_LSP_FIELD_ITEM "item"
#define ZR_LSP_FIELD_FROM "from"
#define ZR_LSP_FIELD_TO "to"
#define ZR_LSP_FIELD_FROM_RANGES "fromRanges"

/** @brief LSP 富文本、内部插入格式标签、诊断源和协商位置编码的字面量；插入格式的线上值另用数值宏。 */
#define ZR_LSP_MARKUP_KIND_MARKDOWN "markdown"
#define ZR_LSP_INSERT_TEXT_FORMAT_KIND_PLAINTEXT "plaintext"
#define ZR_LSP_INSERT_TEXT_FORMAT_KIND_SNIPPET "snippet"
#define ZR_LSP_DIAGNOSTIC_SOURCE_NAME "zr"
#define ZR_LSP_POSITION_ENCODING_UTF8 "utf-8"
#define ZR_LSP_POSITION_ENCODING_UTF16 "utf-16"

/** @brief completion 输出时可提示客户端接受候选项的提交字符。 */
#define ZR_LSP_COMPLETION_COMMIT_CHARACTER_SEMICOLON ";"
#define ZR_LSP_COMPLETION_COMMIT_CHARACTER_COMMA ","
#define ZR_LSP_COMPLETION_COMMIT_CHARACTER_DOT "."
#define ZR_LSP_COMPLETION_COMMIT_CHARACTER_OPEN_PAREN "("

/** @brief initialize 向客户端协商的 completion 与 signature help 触发字符。 */
#define ZR_LSP_COMPLETION_TRIGGER_CHARACTER_MEMBER_ACCESS "."
#define ZR_LSP_COMPLETION_TRIGGER_CHARACTER_NAMESPACE_ACCESS ":"
#define ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_OPEN_PAREN "("
#define ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_ARGUMENT_SEPARATOR ","

/**
 * @brief JSON-RPC/LSP 方法名由 stdio 请求分派、通知发送及一致性测试共用。
 * @note 方法存在常量不表示该后端一定宣告或实现该能力；以能力表及适配层分派为准。
 */
#define ZR_LSP_METHOD_INITIALIZE "initialize"
#define ZR_LSP_METHOD_SHUTDOWN "shutdown"
#define ZR_LSP_METHOD_INITIALIZED "initialized"
#define ZR_LSP_METHOD_EXIT "exit"
#define ZR_LSP_METHOD_CANCEL_REQUEST "$/cancelRequest"
#define ZR_LSP_METHOD_SET_TRACE "$/setTrace"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS "textDocument/publishDiagnostics"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_COMPLETION "textDocument/completion"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER "textDocument/hover"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_SIGNATURE_HELP "textDocument/signatureHelp"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DEFINITION "textDocument/definition"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES "textDocument/references"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL "textDocument/documentSymbol"
#define ZR_LSP_METHOD_WORKSPACE_SYMBOL "workspace/symbol"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT "textDocument/documentHighlight"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_INLAY_HINT "textDocument/inlayHint"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL "textDocument/semanticTokens/full"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_FULL_DELTA "textDocument/semanticTokens/full/delta"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_SEMANTIC_TOKENS_RANGE "textDocument/semanticTokens/range"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_RENAME "textDocument/prepareRename"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_RENAME "textDocument/rename"
#define ZR_LSP_METHOD_COMPLETION_ITEM_RESOLVE "completionItem/resolve"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_FORMATTING "textDocument/formatting"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_RANGE_FORMATTING "textDocument/rangeFormatting"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_RANGES_FORMATTING "textDocument/rangesFormatting"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_ON_TYPE_FORMATTING "textDocument/onTypeFormatting"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_CODE_ACTION "textDocument/codeAction"
#define ZR_LSP_METHOD_CODE_ACTION_RESOLVE "codeAction/resolve"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_FOLDING_RANGE "textDocument/foldingRange"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_SELECTION_RANGE "textDocument/selectionRange"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_LINKED_EDITING_RANGE "textDocument/linkedEditingRange"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_MONIKER "textDocument/moniker"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_INLINE_VALUE "textDocument/inlineValue"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_INLINE_COMPLETION "textDocument/inlineCompletion"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_LINK "textDocument/documentLink"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_IMPLEMENTATION "textDocument/implementation"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_CODE_LENS "textDocument/codeLens"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_CALL_HIERARCHY "textDocument/prepareCallHierarchy"
#define ZR_LSP_METHOD_CALL_HIERARCHY_INCOMING_CALLS "callHierarchy/incomingCalls"
#define ZR_LSP_METHOD_CALL_HIERARCHY_OUTGOING_CALLS "callHierarchy/outgoingCalls"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_PREPARE_TYPE_HIERARCHY "textDocument/prepareTypeHierarchy"
#define ZR_LSP_METHOD_TYPE_HIERARCHY_SUPERTYPES "typeHierarchy/supertypes"
#define ZR_LSP_METHOD_TYPE_HIERARCHY_SUBTYPES "typeHierarchy/subtypes"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DIAGNOSTIC "textDocument/diagnostic"
#define ZR_LSP_METHOD_WORKSPACE_DIAGNOSTIC "workspace/diagnostic"
#define ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_CONFIGURATION "workspace/didChangeConfiguration"
#define ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WATCHED_FILES "workspace/didChangeWatchedFiles"
#define ZR_LSP_METHOD_WORKSPACE_DID_CHANGE_WORKSPACE_FOLDERS "workspace/didChangeWorkspaceFolders"
#define ZR_LSP_METHOD_WORKSPACE_DID_CREATE_FILES "workspace/didCreateFiles"
#define ZR_LSP_METHOD_WORKSPACE_WILL_RENAME_FILES "workspace/willRenameFiles"
#define ZR_LSP_METHOD_WORKSPACE_DID_RENAME_FILES "workspace/didRenameFiles"
#define ZR_LSP_METHOD_WORKSPACE_DID_DELETE_FILES "workspace/didDeleteFiles"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DID_OPEN "textDocument/didOpen"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CHANGE "textDocument/didChange"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DID_CLOSE "textDocument/didClose"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_WILL_SAVE_WAIT_UNTIL "textDocument/willSaveWaitUntil"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DID_SAVE "textDocument/didSave"
#define ZR_LSP_METHOD_ZR_NATIVE_DECLARATION_DOCUMENT "zr/nativeDeclarationDocument"
#define ZR_LSP_METHOD_ZR_PROJECT_MODULES "zr/projectModules"
#define ZR_LSP_METHOD_ZR_RICH_HOVER "zr/richHover"
#define ZR_LSP_METHOD_ZR_SELECTED_PROJECT "zr/selectedProject"

/** @brief initialize 选定工程的扩展参数，与后续工程切换请求共同维护工作区上下文。 */
#define ZR_LSP_FIELD_INITIALIZATION_OPTIONS "initializationOptions"
#define ZR_LSP_INITIALIZATION_OPTION_SELECTED_PROJECT_URI "zrSelectedProjectUri"

/** @brief stdio initialize 的 serverInfo 标识和版本。 */
#define ZR_LSP_SERVER_NAME "zr_vm_language_server_stdio"
#define ZR_LSP_SERVER_VERSION "0.0.1"

/** @brief initialize 协商与 completion 序列化使用的 LSP 枚举数值。 */
#define ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL 2
#define ZR_LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT 1
#define ZR_LSP_INSERT_TEXT_FORMAT_SNIPPET 2

/** @brief code action、code lens、折叠范围与拉取诊断报告的协议类别。 */
#define ZR_LSP_CODE_ACTION_KIND_QUICK_FIX "quickfix"
#define ZR_LSP_CODE_ACTION_KIND_REFACTOR_REWRITE "refactor.rewrite"
#define ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS "source.organizeImports"
#define ZR_LSP_CODE_ACTION_KIND_SOURCE_REMOVE_UNUSED "source.removeUnused"
#define ZR_LSP_COMMAND_RUN_CURRENT_PROJECT "zr.runCurrentProject"
#define ZR_LSP_FOLDING_RANGE_KIND_REGION "region"
#define ZR_LSP_FOLDING_RANGE_KIND_IMPORTS "imports"
#define ZR_LSP_FOLDING_RANGE_KIND_COMMENT "comment"
#define ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL "full"
#define ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED "unchanged"

/** @brief 核心 completion 项到 LSP 协议 kind 数值的映射范围。 */
typedef enum EZrLspCompletionItemKind {
    ZR_LSP_COMPLETION_ITEM_KIND_TEXT = 1,
    ZR_LSP_COMPLETION_ITEM_KIND_METHOD = 2,
    ZR_LSP_COMPLETION_ITEM_KIND_FUNCTION = 3,
    ZR_LSP_COMPLETION_ITEM_KIND_FIELD = 5,
    ZR_LSP_COMPLETION_ITEM_KIND_VARIABLE = 6,
    ZR_LSP_COMPLETION_ITEM_KIND_CLASS = 7,
    ZR_LSP_COMPLETION_ITEM_KIND_INTERFACE = 8,
    ZR_LSP_COMPLETION_ITEM_KIND_MODULE = 9,
    ZR_LSP_COMPLETION_ITEM_KIND_PROPERTY = 10,
    ZR_LSP_COMPLETION_ITEM_KIND_ENUM = 13,
    ZR_LSP_COMPLETION_ITEM_KIND_CONSTANT = 21,
    ZR_LSP_COMPLETION_ITEM_KIND_STRUCT = 22,
} EZrLspCompletionItemKind;

/** @brief 符号表条目到文档／工作区符号及层级项目的 LSP kind 映射。 */
typedef enum EZrLspSymbolKind {
    ZR_LSP_SYMBOL_KIND_MODULE = 2,
    ZR_LSP_SYMBOL_KIND_CLASS = 5,
    ZR_LSP_SYMBOL_KIND_METHOD = 6,
    ZR_LSP_SYMBOL_KIND_PROPERTY = 7,
    ZR_LSP_SYMBOL_KIND_FIELD = 8,
    ZR_LSP_SYMBOL_KIND_ENUM = 10,
    ZR_LSP_SYMBOL_KIND_INTERFACE = 11,
    ZR_LSP_SYMBOL_KIND_FUNCTION = 12,
    ZR_LSP_SYMBOL_KIND_VARIABLE = 13,
    ZR_LSP_SYMBOL_KIND_ENUM_MEMBER = 22,
    ZR_LSP_SYMBOL_KIND_STRUCT = 23,
} EZrLspSymbolKind;

/** @brief inlay hint 序列化到客户端时使用的 LSP kind 值。 */
typedef enum EZrLspInlayHintKind {
    ZR_LSP_INLAY_HINT_KIND_TYPE = 1,
    ZR_LSP_INLAY_HINT_KIND_PARAMETER = 2,
} EZrLspInlayHintKind;

/** @brief 请求解析、分派及取消路径共用的 JSON-RPC/LSP 错误码。 */
#define ZR_LSP_JSON_RPC_PARSE_ERROR_CODE (-32700)
#define ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE (-32600)
#define ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE (-32601)
#define ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE (-32602)
#define ZR_LSP_JSON_RPC_INTERNAL_ERROR_CODE (-32603)
#define ZR_LSP_JSON_RPC_REQUEST_CANCELLED_CODE (-32800)
#define ZR_LSP_JSON_RPC_CONTENT_MODIFIED_CODE (-32801)

#endif //ZR_VM_LANGUAGE_SERVER_CONF_H
/**
 * @brief stdio 长请求通过 $/progress 发布工作进度和分批结果时使用的协议常量。
 * TODO: 这些定义位于头文件保护宏之外；需核对多次包含及外部覆盖场景，
 *       再确认这是否为刻意保留的布局。
 */
#define ZR_LSP_PROGRESS_KIND_BEGIN "begin"
#define ZR_LSP_PROGRESS_KIND_END "end"
#define ZR_LSP_METHOD_PROGRESS "$/progress"
#define ZR_LSP_PARTIAL_RESULT_BATCH_SIZE 64
