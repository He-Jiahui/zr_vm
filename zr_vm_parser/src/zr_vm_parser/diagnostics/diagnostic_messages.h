#ifndef ZR_VM_PARSER_DIAGNOSTIC_MESSAGES_H
#define ZR_VM_PARSER_DIAGNOSTIC_MESSAGES_H

#include "zr_vm_parser/conf.h"

/** @brief 诊断文案请求语言；当前无简体中文翻译时仍返回英文基准文案。 */
typedef enum EZrDiagnosticLocale {
    ZR_DIAGNOSTIC_LOCALE_ENGLISH = 0,
    ZR_DIAGNOSTIC_LOCALE_CHINESE_SIMPLIFIED
} EZrDiagnosticLocale;

/** @brief 静态消息键及其语言文本；中文字段可为空，由 Resolve 处理回退。
 * @note 表项和字符串均由目录持有，调用方仅借用。
 */
typedef struct SZrDiagnosticMessage {
    const TZrChar *key;
    const TZrChar *english;
    const TZrChar *chineseSimplified;
} SZrDiagnosticMessage;

/** @brief 返回静态消息项数量，每组诊断通常有 title 与 message 两项。 */
ZR_PARSER_API TZrSize ZrParser_DiagnosticMessages_Count(void);
/** @brief 按索引借用静态消息项；越界返回 ZR_NULL。 */
ZR_PARSER_API const SZrDiagnosticMessage *ZrParser_DiagnosticMessages_MessageAt(
        TZrSize index);
/** @brief 精确查找消息键；空值或未注册键返回 ZR_NULL。 */
ZR_PARSER_API const SZrDiagnosticMessage *ZrParser_DiagnosticMessages_Find(
        const TZrChar *key);
/** @brief 解析消息键并借用语言文本；缺译时回退英文，未知键返回 ZR_NULL。 */
ZR_PARSER_API const TZrChar *ZrParser_DiagnosticMessages_Resolve(
        EZrDiagnosticLocale locale,
        const TZrChar *key);

#endif // ZR_VM_PARSER_DIAGNOSTIC_MESSAGES_H
