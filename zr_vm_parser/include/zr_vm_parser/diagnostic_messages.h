#ifndef ZR_VM_PARSER_DIAGNOSTIC_MESSAGES_H
#define ZR_VM_PARSER_DIAGNOSTIC_MESSAGES_H

#include "zr_vm_parser/conf.h"

typedef enum EZrDiagnosticLocale {
    ZR_DIAGNOSTIC_LOCALE_ENGLISH = 0,
    ZR_DIAGNOSTIC_LOCALE_CHINESE_SIMPLIFIED
} EZrDiagnosticLocale;

typedef struct SZrDiagnosticMessage {
    const TZrChar *key;
    const TZrChar *english;
    const TZrChar *chineseSimplified;
} SZrDiagnosticMessage;

ZR_PARSER_API TZrSize ZrParser_DiagnosticMessages_Count(void);
ZR_PARSER_API const SZrDiagnosticMessage *ZrParser_DiagnosticMessages_MessageAt(
        TZrSize index);
ZR_PARSER_API const SZrDiagnosticMessage *ZrParser_DiagnosticMessages_Find(
        const TZrChar *key);
ZR_PARSER_API const TZrChar *ZrParser_DiagnosticMessages_Resolve(
        EZrDiagnosticLocale locale,
        const TZrChar *key);

#endif // ZR_VM_PARSER_DIAGNOSTIC_MESSAGES_H
