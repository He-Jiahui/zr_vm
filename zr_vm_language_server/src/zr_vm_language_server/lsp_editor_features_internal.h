#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EDITOR_FEATURES_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EDITOR_FEATURES_INTERNAL_H

#include "interface/lsp_interface_internal.h"

typedef struct SZrLspTextBuilder {
    TZrChar *data;
    TZrSize length;
    TZrSize capacity;
} SZrLspTextBuilder;

typedef enum EZrLspEditorScanMode {
    ZR_LSP_EDITOR_SCAN_CODE = 0,
    ZR_LSP_EDITOR_SCAN_LINE_COMMENT,
    ZR_LSP_EDITOR_SCAN_BLOCK_COMMENT,
    ZR_LSP_EDITOR_SCAN_STRING,
    ZR_LSP_EDITOR_SCAN_CHAR,
    ZR_LSP_EDITOR_SCAN_TEMPLATE_STRING
} EZrLspEditorScanMode;

typedef struct SZrLspEditorScanState {
    EZrLspEditorScanMode mode;
    TZrBool escaped;
} SZrLspEditorScanState;

typedef TZrBool (*TZrLspEditorStructuralCharCallback)(TZrChar value,
                                                      TZrSize offset,
                                                      void *userData);

SZrString *lsp_editor_create_string(SZrState *state, const TZrChar *text, TZrSize length);
SZrFileVersion *lsp_editor_get_file_version(SZrLspContext *context, SZrString *uri);
SZrLspPosition lsp_editor_position_from_offset(const TZrChar *content,
                                               TZrSize contentLength,
                                               TZrSize offset);
TZrSize lsp_editor_line_start_offset(const TZrChar *content,
                                     TZrSize contentLength,
                                     TZrInt32 line);
TZrSize lsp_editor_line_end_offset(const TZrChar *content,
                                   TZrSize contentLength,
                                   TZrInt32 line);
SZrLspRange lsp_editor_range_from_offsets(const TZrChar *content,
                                          TZrSize contentLength,
                                          TZrSize startOffset,
                                          TZrSize endOffset);
TZrBool lsp_text_builder_append_range(SZrLspTextBuilder *builder,
                                      const TZrChar *text,
                                      TZrSize length);
TZrBool lsp_text_builder_append_char(SZrLspTextBuilder *builder, TZrChar value);
TZrBool lsp_editor_append_text_edit(SZrState *state,
                                    SZrArray *result,
                                    SZrLspRange range,
                                    const TZrChar *newText,
                                    TZrSize newTextLength);
TZrBool lsp_editor_scan_structural_chars(const TZrChar *content,
                                         TZrSize contentLength,
                                         TZrSize startOffset,
                                         TZrSize endOffset,
                                         SZrLspEditorScanState *scanState,
                                         TZrLspEditorStructuralCharCallback callback,
                                         void *userData);
TZrBool lsp_editor_offset_is_code(const TZrChar *content,
                                  TZrSize contentLength,
                                  TZrSize offset);

#endif
