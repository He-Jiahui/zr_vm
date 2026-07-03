#ifndef ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_MANIFEST_EXPORT_H
#define ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_MANIFEST_EXPORT_H

#include "module/module_import_signature.h"

TZrBool zr_module_import_signature_verify_manifest_export_binding(
        SZrObjectModule *module,
        const SZrMetadataTokenRecord *memberRefRecord,
        const SZrFunctionModuleEffect *effect,
        const SZrFunctionTypedExportSymbol *symbol,
        const SZrFunction *entryFunction,
        SZrModuleImportSignatureMismatch *outMismatch);

#endif // ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_MANIFEST_EXPORT_H
