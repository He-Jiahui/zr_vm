#ifndef ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_MANIFEST_EXPORT_H
#define ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_MANIFEST_EXPORT_H

#include "module/module_import_signature.h"

/**
 * @brief 用提供方的 manifest export 表再次核对 typed export 对应的成员或类型 token。
 * @return 存在 manifest 表且目标不兼容时返回 ZR_FALSE；未附带表时跳过此附加校验。
 * @note outMismatch 非空时须已清零，失败后供上层模块加载错误诊断使用。
 */
TZrBool zr_module_import_signature_verify_manifest_export_binding(
        SZrObjectModule *module,
        const SZrMetadataTokenRecord *memberRefRecord,
        const SZrFunctionModuleEffect *effect,
        const SZrFunctionTypedExportSymbol *symbol,
        const SZrFunction *entryFunction,
        SZrModuleImportSignatureMismatch *outMismatch);

#endif // ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_MANIFEST_EXPORT_H
