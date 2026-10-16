#ifndef ZR_VM_LANGUAGE_SERVER_LSP_EDITOR_FEATURES_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_LSP_EDITOR_FEATURES_INTERNAL_H

#include "interface/lsp_interface_internal.h"

/** 编辑提案的临时 C 缓冲：格式化与导入整理共用；调用方在转换为 VM 字符串后 free(data)。 */
typedef struct SZrLspTextBuilder {
    TZrChar *data;
    TZrSize length;
    TZrSize capacity;
} SZrLspTextBuilder;

/** 扫描文本而非 AST 时的词法上下文，避免把注释和字面量中的括号当成代码结构。 */
typedef enum EZrLspEditorScanMode {
    ZR_LSP_EDITOR_SCAN_CODE = 0,
    ZR_LSP_EDITOR_SCAN_LINE_COMMENT,
    ZR_LSP_EDITOR_SCAN_BLOCK_COMMENT,
    ZR_LSP_EDITOR_SCAN_STRING,
    ZR_LSP_EDITOR_SCAN_CHAR,
    ZR_LSP_EDITOR_SCAN_TEMPLATE_STRING
} EZrLspEditorScanMode;

/** 跨扫描区间保留的状态；分段调用者必须传递上一区间的结束状态。 */
typedef struct SZrLspEditorScanState {
    EZrLspEditorScanMode mode;
    TZrBool escaped;
} SZrLspEditorScanState;

/** 只接收代码区的花括号；返回失败会中止扫描，供折叠与缩进调用方控制。 */
typedef TZrBool (*TZrLspEditorStructuralCharCallback)(TZrChar value,
                                                      TZrSize offset,
                                                      void *userData);

/** @brief 为编辑器结果创建 VM 字符串；结果随 state 管理，不能按 malloc 缓冲释放。 */
SZrString *lsp_editor_create_string(SZrState *state, const TZrChar *text, TZrSize length);
/** @brief 从已打开文档取得版本句柄；读取内容时还须单独获取 owned snapshot。 */
SZrFileVersion *lsp_editor_get_file_version(SZrLspContext *context, SZrString *uri);
/**
 * @brief 将同一份 UTF-8 内容的字节偏移转成编辑器位置，供格式化、折叠与编辑范围使用。
 * @note BUG: 当前实现逐字节计列；非 ASCII 文本后的响应列与 LSP UTF-16 约定不符，需改用位置编解码器并覆盖上述调用链。
 */
SZrLspPosition lsp_editor_position_from_offset(const TZrChar *content,
                                               TZrSize contentLength,
                                               TZrSize offset);
/** @brief 为整行格式化和选择范围查找行起点；超出文档的行收敛到文末。 */
TZrSize lsp_editor_line_start_offset(const TZrChar *content,
                                     TZrSize contentLength,
                                     TZrInt32 line);
/** @brief 取得不含 CRLF 终止符的行末偏移；调用方决定编辑是否覆盖换行。 */
TZrSize lsp_editor_line_end_offset(const TZrChar *content,
                                   TZrSize contentLength,
                                   TZrInt32 line);
/** @brief 用同一份内容构造编辑范围；两个偏移必须来自该内容的字节坐标。 */
SZrLspRange lsp_editor_range_from_offsets(const TZrChar *content,
                                          TZrSize contentLength,
                                          TZrSize startOffset,
                                          TZrSize endOffset);
/** @brief 追加临时文本供格式化或导入编辑使用；失败后调用方仍负责释放原缓冲。 */
TZrBool lsp_text_builder_append_range(SZrLspTextBuilder *builder,
                                      const TZrChar *text,
                                      TZrSize length);
/** @brief 单字符追加走同一容量和失败契约。 */
TZrBool lsp_text_builder_append_char(SZrLspTextBuilder *builder, TZrChar value);
/** @brief 追加原生 TextEdit 供多种 code action 使用；result 由 FreeTextEdits 回收。 */
TZrBool lsp_editor_append_text_edit(SZrState *state,
                                    SZrArray *result,
                                    SZrLspRange range,
                                    const TZrChar *newText,
                                    TZrSize newTextLength);
/**
 * @brief 给缩进、折叠扫描代码花括号；调用方可跨连续区间传入 scanState。
 * @pre 若 startOffset 不是文首，scanState 应反映其前文词法状态。
 */
TZrBool lsp_editor_scan_structural_chars(const TZrChar *content,
                                         TZrSize contentLength,
                                         TZrSize startOffset,
                                         TZrSize endOffset,
                                         SZrLspEditorScanState *scanState,
                                         TZrLspEditorStructuralCharCallback callback,
                                         void *userData);
/** @brief 给 code action、document link 和 CodeLens 筛掉注释/字面量中的文本候选。 */
TZrBool lsp_editor_offset_is_code(const TZrChar *content,
                                  TZrSize contentLength,
                                  TZrSize offset);

#endif
