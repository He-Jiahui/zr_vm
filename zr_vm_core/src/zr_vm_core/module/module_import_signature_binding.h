#ifndef ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_BINDING_H
#define ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_BINDING_H

#include "module/module_import_signature.h"

/**
 * @brief 将已验证的导入成员及其模块身份登记为调用方元数据绑定，供后续运行时检查。
 * @pre memberRefRecord、effect、symbol、entryFunction 应来自同一次成功的目标匹配。
 * @note 无返回值；绑定表扩容失败时不会向签名校验调用方传播错误。
 */
void zr_module_import_signature_record_binding(SZrState *state,
                                               SZrFunction *callerFunction,
                                               const SZrMetadataTokenRecord *memberRefRecord,
                                               const SZrMetadataTokenRecord *assemblyRefRecord,
                                               const SZrFunctionModuleEffect *effect,
                                               const SZrFunctionTypedExportSymbol *symbol,
                                               const SZrFunction *entryFunction);

/**
 * @brief 尝试把调用方的 TypeRef/TypeSpec 关联到提供方，并记录详细不匹配诊断。
 * @note 类型绑定失败不改变成员签名校验的结果；调用方可查询全局模块加载诊断。
 */
void zr_module_import_signature_bind_type_metadata_with_diagnostic(SZrState *state,
                                                                   SZrFunction *callerFunction,
                                                                   const SZrFunctionModuleEffect *effect,
                                                                   const SZrFunction *entryFunction);

#endif // ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_BINDING_H
