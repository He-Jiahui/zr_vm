#ifndef ZR_VM_PARSER_LEGACY_MIGRATION_MODULE_SPECIFIER_H
#define ZR_VM_PARSER_LEGACY_MIGRATION_MODULE_SPECIFIER_H

#include "zr_vm_parser/conf.h"

typedef struct SZrLegacyMigrationModuleSpecifierMatch {
    TZrSize itemEnd;
    TZrSize editStart;
    TZrSize editEnd;
} SZrLegacyMigrationModuleSpecifierMatch;

TZrBool ZrParser_LegacyMigrationModuleSpecifier_TryMatchBareDebugImport(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize wordStart,
        TZrSize wordEnd,
        SZrLegacyMigrationModuleSpecifierMatch *outMatch);

#endif // ZR_VM_PARSER_LEGACY_MIGRATION_MODULE_SPECIFIER_H
