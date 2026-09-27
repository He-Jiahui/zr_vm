#ifndef ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_H
#define ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_H

#include "module/module_internal.h"

/** @brief 将导入失败归类为成员 ABI、模块 ABI 或版本范围错误，供加载器选择诊断。 */
typedef enum EZrModuleImportSignatureMismatchKind {
    ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_MEMBER_SIGNATURE = 0,
    ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_SIGNATURE = 1,
    ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_VERSION = 2
} EZrModuleImportSignatureMismatchKind;

/**
 * @brief 保存首个导入签名不匹配及加载器生成错误信息所需的身份信息。
 * @note effect 指向本结构内的 effectSnapshot；调用方不得在复制结构后继续使用旧指针。
 *       字符串仍由运行时持有，结果仅在相关模块和函数存活期间有效。
 */
typedef struct SZrModuleImportSignatureMismatch {
    const SZrFunctionModuleEffect *effect;
    SZrFunctionModuleEffect effectSnapshot;
    EZrModuleImportSignatureMismatchKind kind;
    TZrUInt64 expectedHash;
    TZrUInt64 actualHash;
    TZrBool hasActualHash;
    TZrMetadataToken expectedMetadataToken;
    TZrMetadataToken actualMetadataToken;
    TZrBool hasMetadataTokenMismatch;
    TZrMetadataToken expectedSignatureToken;
    TZrMetadataToken actualSignatureToken;
    TZrBool hasSignatureTokenMismatch;
    SZrString *expectedMinVersionInclusive;
    SZrString *expectedMaxVersionExclusive;
    SZrString *actualModuleVersion;
} SZrModuleImportSignatureMismatch;

/**
 * @brief 在模块导入进入调用链接前，核对调用方记录的成员、模块与清单导出身份。
 * @pre outMismatch 非空时须先清零；失败时仅写入首个匹配的错误信息。
 * @return ZR_FALSE 表示已确认不兼容；无调用方或模块时不执行校验并返回 ZR_TRUE。
 * @note 成功路径也会登记元数据绑定；TypeRef/TypeSpec 绑定问题另写模块加载诊断。
 */
ZR_CORE_API TZrBool zr_module_import_signature_verify(SZrState *state,
                                                       SZrFunction *callerFunction,
                                                       SZrString *path,
                                                       SZrObjectModule *module,
                                                       SZrModuleImportSignatureMismatch *outMismatch);

#endif // ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_H
