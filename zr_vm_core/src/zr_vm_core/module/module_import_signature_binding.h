#ifndef ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_BINDING_H
#define ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_BINDING_H

#include "module/module_import_signature.h"

void zr_module_import_signature_record_binding(SZrState *state,
                                               SZrFunction *callerFunction,
                                               const SZrMetadataTokenRecord *memberRefRecord,
                                               const SZrMetadataTokenRecord *assemblyRefRecord,
                                               const SZrFunctionModuleEffect *effect,
                                               const SZrFunctionTypedExportSymbol *symbol,
                                               const SZrFunction *entryFunction);

void zr_module_import_signature_bind_type_metadata_with_diagnostic(SZrState *state,
                                                                   SZrFunction *callerFunction,
                                                                   const SZrFunctionModuleEffect *effect,
                                                                   const SZrFunction *entryFunction);

#endif // ZR_VM_CORE_MODULE_IMPORT_SIGNATURE_BINDING_H
