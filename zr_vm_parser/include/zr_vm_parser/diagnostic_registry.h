#ifndef ZR_VM_PARSER_DIAGNOSTIC_REGISTRY_H
#define ZR_VM_PARSER_DIAGNOSTIC_REGISTRY_H

#include "zr_vm_parser/diagnostic_builder.h"

typedef enum EZrLintCategory {
    ZR_LINT_CATEGORY_UNKNOWN = 0,
    ZR_LINT_CATEGORY_SYNTAX,
    ZR_LINT_CATEGORY_SEMANTIC,
    ZR_LINT_CATEGORY_TYPE,
    ZR_LINT_CATEGORY_FLOW,
    ZR_LINT_CATEGORY_OWNERSHIP,
    ZR_LINT_CATEGORY_STYLE
} EZrLintCategory;

typedef struct SZrDiagnosticDescriptor {
    TZrUInt32 id;
    const TZrChar *code;
    const TZrChar *titleKey;
    const TZrChar *messageFormatKey;
    EZrStructuredDiagnosticSeverity defaultSeverity;
    const TZrChar *helpUri;
    EZrLintCategory category;
} SZrDiagnosticDescriptor;

ZR_PARSER_API TZrSize ZrParser_DiagnosticRegistry_Count(void);
ZR_PARSER_API const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_DescriptorAt(
        TZrSize index);
ZR_PARSER_API const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_FindByCode(
        const TZrChar *code);
ZR_PARSER_API const SZrDiagnosticDescriptor *ZrParser_DiagnosticRegistry_FindById(
        TZrUInt32 id);
ZR_PARSER_API TZrUInt32 ZrParser_DiagnosticRegistry_DescriptorIdForCode(
        const TZrChar *code);

#endif // ZR_VM_PARSER_DIAGNOSTIC_REGISTRY_H
