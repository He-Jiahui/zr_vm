#include "module/module_import_signature_binding.h"

#include "zr_vm_core/function.h"

#define ZR_MODULE_METADATA_TOKEN ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MODULE, 1u)

static const SZrMetadataTokenRecord *module_import_signature_find_record(const SZrFunction *function,
                                                                         TZrMetadataToken token) {
    if (function == ZR_NULL || function->metadataTokenRecords == ZR_NULL || token == 0u) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->metadataTokenRecordLength; index++) {
        if (function->metadataTokenRecords[index].token == token) {
            return &function->metadataTokenRecords[index];
        }
    }

    return ZR_NULL;
}

static void module_import_signature_record_assembly_ref_binding(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrMetadataTokenRecord *assemblyRefRecord,
        const SZrFunctionModuleEffect *effect,
        const SZrFunction *entryFunction) {
    SZrMetadataTokenBinding *binding;
    const SZrMetadataTokenRecord *providerModuleRecord;
    const SZrMetadataTokenRecord *providerModuleSignatureRecord;
    TZrUInt64 expectedModuleHash = 0u;

    if (assemblyRefRecord == ZR_NULL || entryFunction == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(assemblyRefRecord->token) != ZR_METADATA_TABLE_ASSEMBLY_REF) {
        return;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state, callerFunction, assemblyRefRecord->token);
    if (binding == ZR_NULL) {
        return;
    }

    if (effect != ZR_NULL && effect->targetModuleSignatureHash != 0u) {
        expectedModuleHash = effect->targetModuleSignatureHash;
    } else {
        expectedModuleHash = assemblyRefRecord->targetModuleSignatureHash;
    }

    binding->refToken = assemblyRefRecord->token;
    binding->refSignatureToken = assemblyRefRecord->relatedToken;
    binding->refSignatureHash = assemblyRefRecord->signatureHash;
    binding->expectedMetadataToken = assemblyRefRecord->token;
    binding->expectedSignatureToken = assemblyRefRecord->relatedToken;
    binding->expectedSignatureHash = assemblyRefRecord->signatureHash;
    binding->expectedModuleSignatureHash = expectedModuleHash;
    binding->expectedLayoutVersion = 0u;
    binding->expectedLayoutHash = 0u;
    binding->resolvedMetadataToken = ZR_MODULE_METADATA_TOKEN;
    binding->resolvedSignatureToken = 0u;
    binding->resolvedSignatureHash = 0u;
    providerModuleRecord = module_import_signature_find_record(entryFunction, ZR_MODULE_METADATA_TOKEN);
    if (providerModuleRecord != ZR_NULL &&
        ZR_METADATA_TOKEN_TABLE(providerModuleRecord->relatedToken) == ZR_METADATA_TABLE_SIGNATURE) {
        providerModuleSignatureRecord = module_import_signature_find_record(entryFunction,
                                                                            providerModuleRecord->relatedToken);
        binding->resolvedSignatureToken = providerModuleRecord->relatedToken;
        binding->resolvedSignatureHash = providerModuleSignatureRecord != ZR_NULL
                                                 ? providerModuleSignatureRecord->signatureHash
                                                 : providerModuleRecord->signatureHash;
    }
    binding->resolvedModuleSignatureHash = entryFunction->moduleSignatureHash;
    binding->resolvedLayoutVersion = 0u;
    binding->resolvedLayoutHash = 0u;
}

void zr_module_import_signature_record_binding(SZrState *state,
                                               SZrFunction *callerFunction,
                                               const SZrMetadataTokenRecord *memberRefRecord,
                                               const SZrMetadataTokenRecord *assemblyRefRecord,
                                               const SZrFunctionModuleEffect *effect,
                                               const SZrFunctionTypedExportSymbol *symbol,
                                               const SZrFunction *entryFunction) {
    SZrMetadataTokenBinding *binding;

    if (memberRefRecord == ZR_NULL || effect == ZR_NULL || symbol == ZR_NULL || entryFunction == ZR_NULL ||
        ZR_METADATA_TOKEN_TABLE(memberRefRecord->token) != ZR_METADATA_TABLE_MEMBER_REF) {
        return;
    }

    binding = ZrCore_Function_UpsertModuleMetadataBinding(state, callerFunction, memberRefRecord->token);
    if (binding == ZR_NULL) {
        return;
    }

    binding->refToken = memberRefRecord->token;
    binding->refSignatureToken = memberRefRecord->relatedToken;
    binding->refSignatureHash = memberRefRecord->signatureHash;
    binding->expectedMetadataToken = effect->targetMetadataToken;
    binding->expectedSignatureToken = effect->targetSignatureToken;
    binding->expectedSignatureHash = effect->targetSignatureHash;
    binding->expectedModuleSignatureHash = effect->targetModuleSignatureHash;
    binding->resolvedMetadataToken = symbol->metadataToken;
    binding->resolvedSignatureToken = symbol->signatureToken;
    binding->resolvedSignatureHash = symbol->signatureHash;
    binding->resolvedModuleSignatureHash = entryFunction->moduleSignatureHash;

    module_import_signature_record_assembly_ref_binding(state,
                                                        callerFunction,
                                                        assemblyRefRecord,
                                                        effect,
                                                        entryFunction);
}

static const TZrChar *module_import_signature_string_text(SZrString *value) {
    TZrNativeString text;

    if (value == ZR_NULL) {
        return "<unknown>";
    }

    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL ? text : "<unknown>";
}

static void module_import_signature_record_type_spec_diagnostic(
        SZrState *state,
        const SZrFunctionModuleEffect *effect,
        const SZrMetadataTypeSpecBindStatus *status) {
    if (state == ZR_NULL || state->global == ZR_NULL || effect == ZR_NULL ||
        status == ZR_NULL ||
        (status->unmatchedTypeSpecCount == 0u &&
         status->definitionMismatchCount == 0u &&
         status->layoutMismatchCount == 0u)) {
        return;
    }

    ZrCore_GlobalState_SetModuleLoadDiagnostic(
            state->global,
            "type_spec_mismatch: module '%s' member '%s' callerTypeSpecs=%u matchedTypeSpecs=%u "
            "unmatchedTypeSpecs=%u firstUnmatchedTypeSpecToken=0x%x firstUnmatchedSignatureHash=0x%llx "
            "definitionMismatches=%u firstDefinitionMismatchTypeSpecToken=0x%x "
            "expectedDefinitionSignatureHash=0x%llx actualDefinitionSignatureHash=0x%llx "
            "layoutMismatches=%u firstLayoutMismatchTypeSpecToken=0x%x "
            "expectedLayoutVersion=%u actualLayoutVersion=%u expectedLayoutHash=0x%llx actualLayoutHash=0x%llx",
            module_import_signature_string_text(effect->moduleName),
            module_import_signature_string_text(effect->symbolName),
            (unsigned)status->callerTypeSpecCount,
            (unsigned)status->matchedTypeSpecCount,
            (unsigned)status->unmatchedTypeSpecCount,
            (unsigned)status->firstUnmatchedTypeSpecToken,
            (unsigned long long)status->firstUnmatchedSignatureHash,
            (unsigned)status->definitionMismatchCount,
            (unsigned)status->firstDefinitionMismatchTypeSpecToken,
            (unsigned long long)status->firstExpectedDefinitionSignatureHash,
            (unsigned long long)status->firstActualDefinitionSignatureHash,
            (unsigned)status->layoutMismatchCount,
            (unsigned)status->firstLayoutMismatchTypeSpecToken,
            (unsigned)status->firstExpectedLayoutVersion,
            (unsigned)status->firstActualLayoutVersion,
            (unsigned long long)status->firstExpectedLayoutHash,
            (unsigned long long)status->firstActualLayoutHash);
}

static void module_import_signature_record_type_ref_diagnostic(
        SZrState *state,
        const SZrFunctionModuleEffect *effect,
        const SZrMetadataTypeRefBindStatus *status) {
    if (state == ZR_NULL || state->global == ZR_NULL || effect == ZR_NULL ||
        status == ZR_NULL ||
        (status->unmatchedTypeRefCount == 0u &&
         status->definitionMismatchCount == 0u &&
         status->layoutMismatchCount == 0u)) {
        return;
    }

    ZrCore_GlobalState_SetModuleLoadDiagnostic(
            state->global,
            "type_ref_mismatch: module '%s' member '%s' callerTypeRefs=%u matchedTypeRefs=%u "
            "unmatchedTypeRefs=%u firstUnmatchedTypeRefToken=0x%x firstUnmatchedSignatureHash=0x%llx "
            "definitionMismatches=%u firstDefinitionMismatchTypeRefToken=0x%x "
            "expectedDefinitionSignatureHash=0x%llx actualDefinitionSignatureHash=0x%llx "
            "layoutMismatches=%u firstLayoutMismatchTypeRefToken=0x%x "
            "expectedLayoutVersion=%u actualLayoutVersion=%u expectedLayoutHash=0x%llx actualLayoutHash=0x%llx",
            module_import_signature_string_text(effect->moduleName),
            module_import_signature_string_text(effect->symbolName),
            (unsigned)status->callerTypeRefCount,
            (unsigned)status->matchedTypeRefCount,
            (unsigned)status->unmatchedTypeRefCount,
            (unsigned)status->firstUnmatchedTypeRefToken,
            (unsigned long long)status->firstUnmatchedSignatureHash,
            (unsigned)status->definitionMismatchCount,
            (unsigned)status->firstDefinitionMismatchTypeRefToken,
            (unsigned long long)status->firstExpectedDefinitionSignatureHash,
            (unsigned long long)status->firstActualDefinitionSignatureHash,
            (unsigned)status->layoutMismatchCount,
            (unsigned)status->firstLayoutMismatchTypeRefToken,
            (unsigned)status->firstExpectedLayoutVersion,
            (unsigned)status->firstActualLayoutVersion,
            (unsigned long long)status->firstExpectedLayoutHash,
            (unsigned long long)status->firstActualLayoutHash);
}

void zr_module_import_signature_bind_type_metadata_with_diagnostic(
        SZrState *state,
        SZrFunction *callerFunction,
        const SZrFunctionModuleEffect *effect,
        const SZrFunction *entryFunction) {
    SZrMetadataTypeRefBindStatus typeRefStatus;
    SZrMetadataTypeSpecBindStatus status;

    ZrCore_Memory_RawSet(&typeRefStatus, 0, sizeof(typeRefStatus));
    if (!ZrCore_Function_BindMatchingTypeRefMetadataWithStatus(state,
                                                               callerFunction,
                                                               entryFunction,
                                                               &typeRefStatus)) {
        module_import_signature_record_type_ref_diagnostic(state, effect, &typeRefStatus);
    }
    ZrCore_Memory_RawSet(&status, 0, sizeof(status));
    if (!ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus(state,
                                                               callerFunction,
                                                               entryFunction,
                                                               &status)) {
        module_import_signature_record_type_spec_diagnostic(state, effect, &status);
    }
}
