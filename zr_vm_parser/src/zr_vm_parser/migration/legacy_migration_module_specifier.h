#ifndef ZR_VM_PARSER_LEGACY_MIGRATION_MODULE_SPECIFIER_H
#define ZR_VM_PARSER_LEGACY_MIGRATION_MODULE_SPECIFIER_H

#include "zr_vm_parser/conf.h"

/** @brief 裸 debug 导入中可替换字面量的原始字节区间。
 * itemEnd 用于规划器跳过整个调用，editStart/editEnd 只覆盖引号内的模块名。 */
typedef struct SZrLegacyMigrationModuleSpecifierMatch {
    TZrSize itemEnd;
    TZrSize editStart;
    TZrSize editEnd;
} SZrLegacyMigrationModuleSpecifierMatch;

/** @brief 为旧 `import("debug")` 找到可直接改写的模块名，而不改动调用其余部分。
 * @pre wordStart/wordEnd 是 sourceLength 内的完整 `import` 标识符区间；outMatch 可写。
 * @return 匹配成功时写入源区间；其他导入形态不产生迁移候选。 */
TZrBool ZrParser_LegacyMigrationModuleSpecifier_TryMatchBareDebugImport(
        const TZrChar *source,
        TZrSize sourceLength,
        TZrSize wordStart,
        TZrSize wordEnd,
        SZrLegacyMigrationModuleSpecifierMatch *outMatch);

#endif // ZR_VM_PARSER_LEGACY_MIGRATION_MODULE_SPECIFIER_H
