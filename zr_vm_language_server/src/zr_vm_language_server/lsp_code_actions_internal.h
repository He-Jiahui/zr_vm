#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CODE_ACTIONS_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CODE_ACTIONS_INTERNAL_H

#include "lsp_editor_features_internal.h"

TZrBool lsp_code_action_trimmed_line_is_import_declaration(const TZrChar *line, TZrSize length);
TZrBool lsp_code_action_collect_import_organize_edit(SZrState *state,
                                                     SZrFileVersion *fileVersion,
                                                     SZrArray *edits);
TZrBool lsp_code_action_collect_unused_import_cleanup_edit(SZrState *state,
                                                           SZrFileVersion *fileVersion,
                                                           SZrArray *edits);

#endif
