#ifndef ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_H
#define ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_H

#include "module/module_internal.h"

typedef enum EZrModuleImportSignatureMismatchKind {
    ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_MEMBER_SIGNATURE = 0,
    ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_SIGNATURE = 1,
    ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_VERSION = 2
} EZrModuleImportSignatureMismatchKind;

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

ZR_CORE_API TZrBool zr_module_import_signature_verify(SZrState *state,
                                                       SZrFunction *callerFunction,
                                                       SZrString *path,
                                                       SZrObjectModule *module,
                                                       SZrModuleImportSignatureMismatch *outMismatch);

#endif // ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_H
