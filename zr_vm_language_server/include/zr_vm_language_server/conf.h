//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_LANGUAGE_SERVER_CONF_H
#define ZR_VM_LANGUAGE_SERVER_CONF_H

#include "zr_vm_common.h"

#define ZR_LANGUAGE_SERVER_API ZR_API

#define ZR_LSP_DYNAMIC_CAPACITY_GROWTH_FACTOR 2U

#define ZR_LSP_PROJECT_INDEX_INITIAL_CAPACITY 2U
#define ZR_LSP_ARRAY_INITIAL_CAPACITY 8U
#define ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY 4U
#define ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY 16U
#define ZR_LSP_GLOBAL_SCOPE_SYMBOL_INITIAL_CAPACITY ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY

#define ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2 4U
#define ZR_LSP_HASH_MULTIPLIER 31ULL

#define ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX 8U
#define ZR_LSP_NATIVE_GENERIC_TEXT_MAX 128U

#define ZR_LSP_MARKDOWN_BUFFER_SIZE ZR_VM_PATH_LENGTH_MAX
#define ZR_LSP_SIGNATURE_RANGE_PACK_BASE 4096U

#define ZR_LSP_MEMBER_RECURSION_MAX_DEPTH 8U
#define ZR_LSP_AST_RECURSION_MAX_DEPTH 32U

#define ZR_LSP_SHORT_TEXT_BUFFER_LENGTH 32U
#define ZR_LSP_INTEGER_BUFFER_LENGTH 64U
#define ZR_LSP_TYPE_BUFFER_LENGTH 128U
#define ZR_LSP_COMPLETION_DETAIL_BUFFER_LENGTH 160U
#define ZR_LSP_DETAIL_BUFFER_LENGTH 192U
#define ZR_LSP_TEXT_BUFFER_LENGTH 256U
#define ZR_LSP_LONG_TEXT_BUFFER_LENGTH 512U
#define ZR_LSP_HOVER_BUFFER_LENGTH 1024U
#define ZR_LSP_STDIO_HEADER_BUFFER_LENGTH 1024U
#define ZR_LSP_DOCUMENTATION_BUFFER_LENGTH 2048U

#define ZR_LSP_COMMENT_SCAN_LINE_LIMIT 32U
#define ZR_LSP_COMMENT_BUFFER_LENGTH ZR_LSP_HOVER_BUFFER_LENGTH
#define ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY 32U

#define ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX "Content-Length:"

#define ZR_LSP_JSON_RPC_FIELD_JSONRPC "jsonrpc"
#define ZR_LSP_JSON_RPC_VERSION "2.0"
#define ZR_LSP_JSON_RPC_FIELD_ID "id"
#define ZR_LSP_JSON_RPC_FIELD_METHOD "method"
#define ZR_LSP_JSON_RPC_FIELD_PARAMS "params"
#define ZR_LSP_JSON_RPC_FIELD_RESULT "result"
#define ZR_LSP_JSON_RPC_FIELD_ERROR "error"
#define ZR_LSP_JSON_RPC_FIELD_CODE "code"
#define ZR_LSP_JSON_RPC_FIELD_MESSAGE "message"

#define ZR_LSP_FIELD_LINE "line"
#define ZR_LSP_FIELD_CHARACTER "character"
#define ZR_LSP_FIELD_START "start"
#define ZR_LSP_FIELD_END "end"
#define ZR_LSP_FIELD_URI "uri"
#define ZR_LSP_FIELD_RANGE "range"
#define ZR_LSP_FIELD_NAME "name"
#define ZR_LSP_FIELD_KIND "kind"
#define ZR_LSP_FIELD_LOCATION "location"
#define ZR_LSP_FIELD_CONTAINER_NAME "containerName"
#define ZR_LSP_FIELD_SEVERITY "severity"
#define ZR_LSP_FIELD_SOURCE "source"
#define ZR_LSP_FIELD_CODE ZR_LSP_JSON_RPC_FIELD_CODE
#define ZR_LSP_FIELD_MESSAGE ZR_LSP_JSON_RPC_FIELD_MESSAGE
#define ZR_LSP_FIELD_RELATED_INFORMATION "relatedInformation"
#define ZR_LSP_FIELD_TYPE "type"
#define ZR_LSP_FIELD_LABEL "label"
#define ZR_LSP_FIELD_DETAIL "detail"
#define ZR_LSP_FIELD_VALUE "value"
#define ZR_LSP_FIELD_ROLE "role"
#define ZR_LSP_FIELD_SECTIONS "sections"
#define ZR_LSP_FIELD_DOCUMENTATION "documentation"
#define ZR_LSP_FIELD_INSERT_TEXT "insertText"
#define ZR_LSP_FIELD_INSERT_TEXT_FORMAT "insertTextFormat"
#define ZR_LSP_FIELD_CONTENTS "contents"
#define ZR_LSP_FIELD_PARAMETERS "parameters"
#define ZR_LSP_FIELD_SIGNATURES "signatures"
#define ZR_LSP_FIELD_ACTIVE_SIGNATURE "activeSignature"
#define ZR_LSP_FIELD_ACTIVE_PARAMETER "activeParameter"
#define ZR_LSP_FIELD_TEXT_DOCUMENT "textDocument"
#define ZR_LSP_FIELD_TEXT "text"
#define ZR_LSP_FIELD_VERSION "version"
#define ZR_LSP_FIELD_DIAGNOSTICS "diagnostics"
#define ZR_LSP_FIELD_POSITION "position"
#define ZR_LSP_FIELD_NEW_TEXT "newText"
#define ZR_LSP_FIELD_CHANGES "changes"
#define ZR_LSP_FIELD_CONTENT_CHANGES "contentChanges"
#define ZR_LSP_FIELD_CONTEXT "context"
#define ZR_LSP_FIELD_ONLY "only"
#define ZR_LSP_FIELD_INCLUDE_DECLARATION "includeDeclaration"
#define ZR_LSP_FIELD_QUERY "query"
#define ZR_LSP_FIELD_TOKEN_TYPES "tokenTypes"
#define ZR_LSP_FIELD_TOKEN_MODIFIERS "tokenModifiers"
#define ZR_LSP_FIELD_DATA "data"
#define ZR_LSP_FIELD_PLACEHOLDER "placeholder"
#define ZR_LSP_FIELD_NEW_NAME "newName"
#define ZR_LSP_FIELD_OPEN_CLOSE "openClose"
#define ZR_LSP_FIELD_CHANGE "change"
#define ZR_LSP_FIELD_SAVE "save"
#define ZR_LSP_FIELD_WILL_SAVE "willSave"
#define ZR_LSP_FIELD_WILL_SAVE_WAIT_UNTIL "willSaveWaitUntil"
#define ZR_LSP_FIELD_INCLUDE_TEXT "includeText"
#define ZR_LSP_FIELD_RESOLVE_PROVIDER "resolveProvider"
#define ZR_LSP_FIELD_TRIGGER_CHARACTERS "triggerCharacters"
#define ZR_LSP_FIELD_PREPARE_PROVIDER "prepareProvider"
#define ZR_LSP_FIELD_CAPABILITIES "capabilities"
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
#define ZR_LSP_FIELD_FILE_OPERATIONS "fileOperations"
#define ZR_LSP_FIELD_WILL_CREATE "willCreate"
#define ZR_LSP_FIELD_DID_CREATE "didCreate"
#define ZR_LSP_FIELD_WILL_RENAME "willRename"
#define ZR_LSP_FIELD_DID_RENAME "didRename"
#define ZR_LSP_FIELD_WILL_DELETE "willDelete"
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
#define ZR_LSP_FIELD_COLOR_PROVIDER "colorProvider"
#define ZR_LSP_FIELD_DOCUMENT_LINK_PROVIDER "documentLinkProvider"
#define ZR_LSP_FIELD_DECLARATION_PROVIDER "declarationProvider"
#define ZR_LSP_FIELD_TYPE_DEFINITION_PROVIDER "typeDefinitionProvider"
#define ZR_LSP_FIELD_IMPLEMENTATION_PROVIDER "implementationProvider"
#define ZR_LSP_FIELD_CODE_LENS_PROVIDER "codeLensProvider"
#define ZR_LSP_FIELD_EXECUTE_COMMAND_PROVIDER "executeCommandProvider"
#define ZR_LSP_FIELD_COMMANDS "commands"
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
#define ZR_LSP_FIELD_COLOR "color"
#define ZR_LSP_FIELD_RED "red"
#define ZR_LSP_FIELD_GREEN "green"
#define ZR_LSP_FIELD_BLUE "blue"
#define ZR_LSP_FIELD_ALPHA "alpha"
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

#define ZR_LSP_MARKUP_KIND_MARKDOWN "markdown"
#define ZR_LSP_INSERT_TEXT_FORMAT_KIND_PLAINTEXT "plaintext"
#define ZR_LSP_INSERT_TEXT_FORMAT_KIND_SNIPPET "snippet"
#define ZR_LSP_DIAGNOSTIC_SOURCE_NAME "zr"

#define ZR_LSP_COMPLETION_TRIGGER_CHARACTER_MEMBER_ACCESS "."
#define ZR_LSP_COMPLETION_TRIGGER_CHARACTER_NAMESPACE_ACCESS ":"
#define ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_OPEN_PAREN "("
#define ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_ARGUMENT_SEPARATOR ","

#define ZR_LSP_METHOD_INITIALIZE "initialize"
#define ZR_LSP_METHOD_SHUTDOWN "shutdown"
#define ZR_LSP_METHOD_INITIALIZED "initialized"
#define ZR_LSP_METHOD_EXIT "exit"
#define ZR_LSP_METHOD_CANCEL_REQUEST "$/cancelRequest"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_PUBLISH_DIAGNOSTICS "textDocument/publishDiagnostics"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_COMPLETION "textDocument/completion"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_HOVER "textDocument/hover"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_SIGNATURE_HELP "textDocument/signatureHelp"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DEFINITION "textDocument/definition"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_REFERENCES "textDocument/references"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_SYMBOL "textDocument/documentSymbol"
#define ZR_LSP_METHOD_WORKSPACE_SYMBOL "workspace/symbol"
#define ZR_LSP_METHOD_WORKSPACE_SYMBOL_RESOLVE "workspaceSymbol/resolve"
#define ZR_LSP_METHOD_WORKSPACE_EXECUTE_COMMAND "workspace/executeCommand"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_HIGHLIGHT "textDocument/documentHighlight"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_INLAY_HINT "textDocument/inlayHint"
#define ZR_LSP_METHOD_INLAY_HINT_RESOLVE "inlayHint/resolve"
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
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_COLOR "textDocument/documentColor"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_COLOR_PRESENTATION "textDocument/colorPresentation"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DOCUMENT_LINK "textDocument/documentLink"
#define ZR_LSP_METHOD_DOCUMENT_LINK_RESOLVE "documentLink/resolve"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_DECLARATION "textDocument/declaration"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_TYPE_DEFINITION "textDocument/typeDefinition"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_IMPLEMENTATION "textDocument/implementation"
#define ZR_LSP_METHOD_TEXT_DOCUMENT_CODE_LENS "textDocument/codeLens"
#define ZR_LSP_METHOD_CODE_LENS_RESOLVE "codeLens/resolve"
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
#define ZR_LSP_METHOD_WORKSPACE_WILL_CREATE_FILES "workspace/willCreateFiles"
#define ZR_LSP_METHOD_WORKSPACE_DID_CREATE_FILES "workspace/didCreateFiles"
#define ZR_LSP_METHOD_WORKSPACE_WILL_RENAME_FILES "workspace/willRenameFiles"
#define ZR_LSP_METHOD_WORKSPACE_DID_RENAME_FILES "workspace/didRenameFiles"
#define ZR_LSP_METHOD_WORKSPACE_WILL_DELETE_FILES "workspace/willDeleteFiles"
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

#define ZR_LSP_FIELD_INITIALIZATION_OPTIONS "initializationOptions"
#define ZR_LSP_INITIALIZATION_OPTION_SELECTED_PROJECT_URI "zrSelectedProjectUri"

#define ZR_LSP_SERVER_NAME "zr_vm_language_server_stdio"
#define ZR_LSP_SERVER_VERSION "0.0.1"

#define ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL 2
#define ZR_LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT 1
#define ZR_LSP_INSERT_TEXT_FORMAT_SNIPPET 2

#define ZR_LSP_CODE_ACTION_KIND_QUICK_FIX "quickfix"
#define ZR_LSP_CODE_ACTION_KIND_SOURCE_ORGANIZE_IMPORTS "source.organizeImports"
#define ZR_LSP_COMMAND_RUN_CURRENT_PROJECT "zr.runCurrentProject"
#define ZR_LSP_COMMAND_SHOW_REFERENCES "zr.showReferences"
#define ZR_LSP_FOLDING_RANGE_KIND_REGION "region"
#define ZR_LSP_FOLDING_RANGE_KIND_IMPORTS "imports"
#define ZR_LSP_FOLDING_RANGE_KIND_COMMENT "comment"
#define ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_FULL "full"
#define ZR_LSP_DOCUMENT_DIAGNOSTIC_REPORT_KIND_UNCHANGED "unchanged"

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

typedef enum EZrLspInlayHintKind {
    ZR_LSP_INLAY_HINT_KIND_TYPE = 1,
    ZR_LSP_INLAY_HINT_KIND_PARAMETER = 2,
} EZrLspInlayHintKind;

#define ZR_LSP_JSON_RPC_PARSE_ERROR_CODE (-32700)
#define ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE (-32600)
#define ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE (-32601)
#define ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE (-32602)

#endif //ZR_VM_LANGUAGE_SERVER_CONF_H
