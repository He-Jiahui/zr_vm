#include "module/module_import_signature_manifest_export.h"

#include "zr_vm_core/metadata_runtime.h"

static TZrUInt32 module_import_signature_manifest_export_kind(const SZrFunctionModuleEffect *effect) {
    if (effect == ZR_NULL) {
        return 0u;
    }
    if (effect->exportKind == ZR_MODULE_EXPORT_KIND_TYPE) {
        return ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE;
    }
    if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_CALL ||
        effect->exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION) {
        return ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD;
    }
    if (effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_READ ||
        effect->kind == ZR_MODULE_ENTRY_EFFECT_IMPORT_REF ||
        effect->exportKind == ZR_MODULE_EXPORT_KIND_VALUE) {
        return ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD;
    }
    return 0u;
}

static void module_import_signature_manifest_fill_binding(
        const SZrMetadataTokenRecord *memberRefRecord,
        const SZrFunctionModuleEffect *effect,
        const SZrFunctionTypedExportSymbol *symbol,
        const SZrFunction *entryFunction,
        SZrMetadataTokenBinding *outBinding) {
    ZrCore_Memory_RawSet(outBinding, 0, sizeof(*outBinding));
    if (memberRefRecord != ZR_NULL) {
        outBinding->refToken = memberRefRecord->token;
        outBinding->refSignatureToken = memberRefRecord->relatedToken;
        outBinding->refSignatureHash = memberRefRecord->signatureHash;
    }
    if (effect != ZR_NULL) {
        outBinding->expectedMetadataToken = effect->targetMetadataToken;
        outBinding->expectedSignatureToken = effect->targetSignatureToken;
        outBinding->expectedSignatureHash = effect->targetSignatureHash;
        outBinding->expectedModuleSignatureHash = effect->targetModuleSignatureHash;
    }
    if (symbol != ZR_NULL) {
        outBinding->resolvedMetadataToken = symbol->metadataToken;
        outBinding->resolvedSignatureToken = symbol->signatureToken;
        outBinding->resolvedSignatureHash = symbol->signatureHash;
    }
    if (entryFunction != ZR_NULL) {
        outBinding->resolvedModuleSignatureHash = entryFunction->moduleSignatureHash;
    }
}

static void module_import_signature_record_manifest_export_mismatch(
        SZrModuleImportSignatureMismatch *outMismatch,
        const SZrFunctionModuleEffect *effect,
        const SZrFunctionTypedExportSymbol *symbol,
        const SZrMetadataRuntimeBindingCompatibilityReport *report,
        EZrMetadataRuntimeBindingCompatibilityStatus status) {
    if (outMismatch == ZR_NULL || outMismatch->effect != ZR_NULL || effect == ZR_NULL) {
        return;
    }

    outMismatch->effectSnapshot = *effect;
    outMismatch->effect = &outMismatch->effectSnapshot;
    if (status == ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH) {
        outMismatch->kind = ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_VERSION;
        if (report != ZR_NULL) {
            outMismatch->expectedMinVersionInclusive = report->expectedMinVersionInclusive;
            outMismatch->expectedMaxVersionExclusive = report->expectedMaxVersionExclusive;
            outMismatch->actualModuleVersion = report->actualModuleVersion;
        }
        return;
    }
    if (status == ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_SIGNATURE_HASH_MISMATCH) {
        outMismatch->kind = ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_ASSEMBLY_SIGNATURE;
        if (report != ZR_NULL) {
            outMismatch->expectedHash = report->expectedModuleSignatureHash;
            outMismatch->actualHash = report->actualModuleSignatureHash;
            outMismatch->hasActualHash = report->actualModuleSignatureHash != 0u ? ZR_TRUE : ZR_FALSE;
        }
        return;
    }

    outMismatch->kind = ZR_MODULE_IMPORT_SIGNATURE_MISMATCH_MEMBER_SIGNATURE;
    outMismatch->expectedHash = effect->targetSignatureHash;
    outMismatch->actualHash = symbol != ZR_NULL ? symbol->signatureHash : 0u;
    outMismatch->hasActualHash = outMismatch->actualHash != 0u ? ZR_TRUE : ZR_FALSE;
    if (report != ZR_NULL &&
        report->expectedMetadataToken != 0u &&
        report->actualMetadataToken != 0u &&
        report->expectedMetadataToken != report->actualMetadataToken) {
        outMismatch->expectedMetadataToken = report->expectedMetadataToken;
        outMismatch->actualMetadataToken = report->actualMetadataToken;
        outMismatch->hasMetadataTokenMismatch = ZR_TRUE;
    }
    if (report != ZR_NULL &&
        report->expectedSignatureToken != 0u &&
        report->actualSignatureToken != 0u &&
        report->expectedSignatureToken != report->actualSignatureToken) {
        outMismatch->expectedSignatureToken = report->expectedSignatureToken;
        outMismatch->actualSignatureToken = report->actualSignatureToken;
        outMismatch->hasSignatureTokenMismatch = ZR_TRUE;
    }
}

TZrBool zr_module_import_signature_verify_manifest_export_binding(
        SZrObjectModule *module,
        const SZrMetadataTokenRecord *memberRefRecord,
        const SZrFunctionModuleEffect *effect,
        const SZrFunctionTypedExportSymbol *symbol,
        const SZrFunction *entryFunction,
        SZrModuleImportSignatureMismatch *outMismatch) {
    SZrMetadataRuntime *runtime;
    TZrUInt32 exportKind;
    const TZrChar *exportTarget;
    SZrMetadataTokenBinding binding;
    SZrMetadataRuntimeBindingCompatibilityReport report;
    EZrMetadataRuntimeBindingCompatibilityStatus status;

    if (module == ZR_NULL || effect == ZR_NULL || symbol == ZR_NULL || entryFunction == ZR_NULL) {
        return ZR_TRUE;
    }

    runtime = ZrCore_Module_GetMetadataRuntime(module);
    if (runtime == ZR_NULL || runtime->manifestExports == ZR_NULL || runtime->manifestExportCount == 0u) {
        return ZR_TRUE;
    }

    exportKind = module_import_signature_manifest_export_kind(effect);
    exportTarget = effect->symbolName != ZR_NULL ? ZrCore_String_GetNativeString(effect->symbolName) : ZR_NULL;
    if (exportKind == 0u || exportTarget == ZR_NULL || exportTarget[0] == '\0') {
        return ZR_TRUE;
    }

    module_import_signature_manifest_fill_binding(memberRefRecord,
                                                  effect,
                                                  symbol,
                                                  entryFunction,
                                                  &binding);
    status = ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility(
            runtime,
            exportKind,
            exportTarget,
            &binding,
            memberRefRecord,
            entryFunction->moduleVersion,
            ZR_NULL,
            &report);
    if (status == ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE) {
        return ZR_TRUE;
    }

    module_import_signature_record_manifest_export_mismatch(outMismatch,
                                                            effect,
                                                            symbol,
                                                            &report,
                                                            status);
    return ZR_FALSE;
}
