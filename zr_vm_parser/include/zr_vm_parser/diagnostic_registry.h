#ifndef ZR_VM_PARSER_DIAGNOSTIC_REGISTRY_H
#define ZR_VM_PARSER_DIAGNOSTIC_REGISTRY_H

#include "zr_vm_parser/diagnostic_builder.h"

/** @brief 描述符的诊断分类，供目录查询和客户端分组；UNKNOWN 表示未分类。 */
typedef enum EZrLintCategory {
    ZR_LINT_CATEGORY_UNKNOWN = 0,
    ZR_LINT_CATEGORY_SYNTAX,
    ZR_LINT_CATEGORY_SEMANTIC,
    ZR_LINT_CATEGORY_TYPE,
    ZR_LINT_CATEGORY_FLOW,
    ZR_LINT_CATEGORY_OWNERSHIP,
    ZR_LINT_CATEGORY_STYLE
} EZrLintCategory;

/** @brief 静态目录项将稳定 code/id、消息键和展示元数据绑定在一起。
 * @note 目录持有本结构及所有字符串；查询得到的指针只可借用，不由调用方释放。
 */
typedef struct SZrDiagnosticDescriptor {
    TZrUInt32 id;
    const TZrChar *code;
    const TZrChar *titleKey;
    const TZrChar *messageFormatKey;
    EZrStructuredDiagnosticSeverity defaultSeverity;
    const TZrChar *helpUri;
    EZrLintCategory category;
} SZrDiagnosticDescriptor;

/** @brief 返回静态描述符数量，供目录遍历与覆盖检查。 */
ZR_PARSER_API TZrSize ZrParser_DiagnosticRegistry_Count(void);
/** @brief 按索引借用描述符；越界返回 ZR_NULL。 */
ZR_PARSER_API const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_DescriptorAt(
        TZrSize index);
/** @brief 将构建器使用的文本 code 映射到静态描述符；空值或未知 code 返回 ZR_NULL。 */
ZR_PARSER_API const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_FindByCode(
        const TZrChar *code);
/** @brief 按结构化诊断携带的 id 查目录；0 或未登记 id 返回 ZR_NULL。 */
ZR_PARSER_API const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_FindById(
        TZrUInt32 id);
/** @brief 构建结构化诊断时取得稳定编号；未知 code 以 0 表示无目录项。 */
ZR_PARSER_API TZrUInt32 ZrParser_DiagnosticRegistry_DescriptorIdForCode(
        const TZrChar *code);

#endif // ZR_VM_PARSER_DIAGNOSTIC_REGISTRY_H
