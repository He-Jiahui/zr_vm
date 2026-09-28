#ifndef ZR_VM_LANGUAGE_SERVER_LSP_CODE_ACTIONS_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_CODE_ACTIONS_INTERNAL_H

#include "lsp_editor_features_internal.h"

/* 顶部导入行的词法识别供整理动作和缺失导入插入点共享；line 已去除缩进。 */
TZrBool lsp_code_action_trimmed_line_is_import_declaration(const TZrChar *line, TZrSize length);
/* 两个 source 动作只追加编辑；空差异成功返回，失败后由调用方释放部分 edits。 */
TZrBool lsp_code_action_collect_import_organize_edit(SZrState *state,
                                                     SZrFileVersion *fileVersion,
                                                     SZrArray *edits);
TZrBool lsp_code_action_collect_unused_import_cleanup_edit(SZrState *state,
                                                           SZrFileVersion *fileVersion,
                                                           SZrArray *edits);

#endif
