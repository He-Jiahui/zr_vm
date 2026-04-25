#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EDITOR_FEATURES_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EDITOR_FEATURES_INTERNAL_H

#include "interface/lsp_interface_internal.h"

typedef struct SZrLspTextBuilder {
    TZrChar *data;
    TZrSize length;
    TZrSize capacity;
} SZrLspTextBuilder;

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

#endif
