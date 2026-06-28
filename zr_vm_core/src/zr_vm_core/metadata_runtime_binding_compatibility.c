#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

static TZrBool metadata_runtime_parse_semver(const TZrChar *text,
                                             TZrUInt32 *outMajor,
                                             TZrUInt32 *outMinor,
                                             TZrUInt32 *outPatch) {
    TZrUInt32 parts[3] = {0u, 0u, 0u};
    TZrUInt32 partIndex = 0u;
    TZrSize offset = 0u;

    if (outMajor != ZR_NULL) {
        *outMajor = 0u;
    }
    if (outMinor != ZR_NULL) {
        *outMinor = 0u;
    }
    if (outPatch != ZR_NULL) {
        *outPatch = 0u;
    }
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    while (partIndex < 3u) {
        TZrUInt32 value = 0u;
        TZrBool hasDigit = ZR_FALSE;

        while (text[offset] >= '0' && text[offset] <= '9') {
            TZrUInt32 digit = (TZrUInt32)(text[offset] - '0');
            if (value > (((TZrUInt32)0xFFFFFFFFu) - digit) / 10u) {
                return ZR_FALSE;
            }
            value = value * 10u + digit;
            hasDigit = ZR_TRUE;
            offset++;
        }
        if (!hasDigit) {
            return ZR_FALSE;
        }
        parts[partIndex++] = value;
        if (partIndex == 3u) {
            break;
        }
        if (text[offset] != '.') {
            return ZR_FALSE;
        }
        offset++;
    }

    if (text[offset] != '\0') {
        return ZR_FALSE;
    }

    if (outMajor != ZR_NULL) {
        *outMajor = parts[0];
    }
    if (outMinor != ZR_NULL) {
        *outMinor = parts[1];
    }
    if (outPatch != ZR_NULL) {
        *outPatch = parts[2];
    }
    return ZR_TRUE;
}

static TZrBool metadata_runtime_string_is_semver(SZrString *value) {
    TZrUInt32 major;
    TZrUInt32 minor;
    TZrUInt32 patch;

    return metadata_runtime_parse_semver(value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL,
                                         &major,
                                         &minor,
                                         &patch);
}

static int metadata_runtime_compare_semver(SZrString *left, SZrString *right) {
    TZrUInt32 leftMajor;
    TZrUInt32 leftMinor;
    TZrUInt32 leftPatch;
    TZrUInt32 rightMajor;
    TZrUInt32 rightMinor;
    TZrUInt32 rightPatch;

    if (!metadata_runtime_parse_semver(left != ZR_NULL ? ZrCore_String_GetNativeString(left) : ZR_NULL,
                                       &leftMajor,
                                       &leftMinor,
                                       &leftPatch) ||
        !metadata_runtime_parse_semver(right != ZR_NULL ? ZrCore_String_GetNativeString(right) : ZR_NULL,
                                       &rightMajor,
                                       &rightMinor,
                                       &rightPatch)) {
        return 0;
    }

    if (leftMajor != rightMajor) {
        return leftMajor < rightMajor ? -1 : 1;
    }
    if (leftMinor != rightMinor) {
        return leftMinor < rightMinor ? -1 : 1;
    }
    if (leftPatch != rightPatch) {
        return leftPatch < rightPatch ? -1 : 1;
    }
    return 0;
}

static TZrBool metadata_runtime_version_range_matches(const SZrMetadataTokenRecord *refRecord,
                                                      SZrString *actualModuleVersion) {
    if (refRecord == ZR_NULL ||
        refRecord->minModuleVersionInclusive == ZR_NULL ||
        refRecord->maxModuleVersionExclusive == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!metadata_runtime_string_is_semver(actualModuleVersion) ||
        !metadata_runtime_string_is_semver(refRecord->minModuleVersionInclusive) ||
        !metadata_runtime_string_is_semver(refRecord->maxModuleVersionExclusive)) {
        return ZR_TRUE;
    }

    return metadata_runtime_compare_semver(actualModuleVersion, refRecord->minModuleVersionInclusive) >= 0 &&
                   metadata_runtime_compare_semver(actualModuleVersion, refRecord->maxModuleVersionExclusive) < 0
           ? ZR_TRUE
           : ZR_FALSE;
}

static void metadata_runtime_fill_binding_report(
        SZrMetadataRuntimeBindingCompatibilityReport *report,
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion,
        EZrMetadataRuntimeBindingCompatibilityStatus status) {
    if (report == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(report, 0, sizeof(*report));
    report->status = status;
    report->actualModuleVersion = actualModuleVersion;
    if (refRecord != ZR_NULL) {
        report->expectedMinVersionInclusive = refRecord->minModuleVersionInclusive;
        report->expectedMaxVersionExclusive = refRecord->maxModuleVersionExclusive;
    }
    if (binding == ZR_NULL) {
        return;
    }

    report->expectedMetadataToken = binding->expectedMetadataToken;
    report->actualMetadataToken = binding->resolvedMetadataToken;
    report->expectedSignatureToken = binding->expectedSignatureToken;
    report->actualSignatureToken = binding->resolvedSignatureToken;
    report->expectedSignatureHash = binding->expectedSignatureHash;
    report->actualSignatureHash = binding->resolvedSignatureHash;
    report->expectedModuleSignatureHash = binding->expectedModuleSignatureHash;
    report->actualModuleSignatureHash = binding->resolvedModuleSignatureHash;
    report->expectedLayoutVersion = binding->expectedLayoutVersion;
    report->actualLayoutVersion = binding->resolvedLayoutVersion;
    report->expectedLayoutHash = binding->expectedLayoutHash;
    report->actualLayoutHash = binding->resolvedLayoutHash;
}

static TZrBool metadata_runtime_layout_identity_is_present(const SZrMetadataTokenBinding *binding) {
    return binding != ZR_NULL &&
           (binding->expectedLayoutVersion != 0u ||
            binding->expectedLayoutHash != 0u ||
            binding->resolvedLayoutVersion != 0u ||
            binding->resolvedLayoutHash != 0u)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static TZrBool metadata_runtime_binding_is_assembly_ref_to_module(const SZrMetadataTokenBinding *binding) {
    return binding != ZR_NULL &&
           ZR_METADATA_TOKEN_TABLE(binding->expectedMetadataToken) == ZR_METADATA_TABLE_ASSEMBLY_REF &&
           ZR_METADATA_TOKEN_TABLE(binding->resolvedMetadataToken) == ZR_METADATA_TABLE_MODULE
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static EZrMetadataRuntimeBindingCompatibilityStatus metadata_runtime_check_binding_status(
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion) {
    TZrBool isAssemblyRefToModule;

    if (binding == ZR_NULL) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT;
    }

    isAssemblyRefToModule = metadata_runtime_binding_is_assembly_ref_to_module(binding);
    if (!metadata_runtime_version_range_matches(refRecord, actualModuleVersion)) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_VERSION_MISMATCH;
    }
    if (binding->expectedModuleSignatureHash != 0u &&
        binding->expectedModuleSignatureHash != binding->resolvedModuleSignatureHash) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_MODULE_SIGNATURE_HASH_MISMATCH;
    }
    if (!isAssemblyRefToModule &&
        binding->expectedMetadataToken != 0u &&
        binding->expectedMetadataToken != binding->resolvedMetadataToken) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_METADATA_TOKEN_MISMATCH;
    }
    if (!isAssemblyRefToModule &&
        binding->expectedSignatureToken != 0u &&
        binding->expectedSignatureToken != binding->resolvedSignatureToken) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_TOKEN_MISMATCH;
    }
    if (binding->expectedSignatureHash != 0u &&
        binding->expectedSignatureHash != binding->resolvedSignatureHash) {
        return ZR_METADATA_RUNTIME_BINDING_STATUS_SIGNATURE_HASH_MISMATCH;
    }
    if (metadata_runtime_layout_identity_is_present(binding)) {
        if (binding->expectedLayoutVersion != binding->resolvedLayoutVersion) {
            return ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_VERSION_MISMATCH;
        }
        if (binding->expectedLayoutHash != binding->resolvedLayoutHash) {
            return ZR_METADATA_RUNTIME_BINDING_STATUS_LAYOUT_HASH_MISMATCH;
        }
    }

    return ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE;
}

EZrMetadataRuntimeBindingCompatibilityStatus ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(
        const SZrMetadataTokenBinding *binding,
        const SZrMetadataTokenRecord *refRecord,
        SZrString *actualModuleVersion,
        SZrMetadataRuntimeBindingCompatibilityReport *outReport) {
    EZrMetadataRuntimeBindingCompatibilityStatus status =
            metadata_runtime_check_binding_status(binding, refRecord, actualModuleVersion);

    metadata_runtime_fill_binding_report(outReport, binding, refRecord, actualModuleVersion, status);
    return status;
}

static const SZrMetadataTokenRecord *metadata_runtime_find_binding_ref_record(
        const SZrFunction *function,
        const SZrMetadataTokenBinding *binding) {
    const SZrMetadataTokenRecord *record;

    if (function == ZR_NULL || binding == ZR_NULL || binding->refToken == 0u) {
        return ZR_NULL;
    }

    record = ZrCore_Function_FindMetadataTokenRecord(function, binding->refToken);
    if (record != ZR_NULL) {
        return record;
    }
    return ZrCore_Function_FindModuleMetadataTokenRecord(function, binding->refToken);
}

EZrMetadataRuntimeBindingCompatibilityStatus ZrCore_MetadataRuntime_CheckFunctionTokenBindingsCompatibility(
        const SZrFunction *function,
        SZrString *actualModuleVersion,
        const SZrMetadataTokenBinding **outBinding,
        const SZrMetadataTokenRecord **outRefRecord,
        SZrMetadataRuntimeBindingCompatibilityReport *outReport) {
    EZrMetadataRuntimeBindingCompatibilityStatus status;
    SZrMetadataRuntimeBindingCompatibilityReport localReport;

    if (outBinding != ZR_NULL) {
        *outBinding = ZR_NULL;
    }
    if (outRefRecord != ZR_NULL) {
        *outRefRecord = ZR_NULL;
    }
    if (function == ZR_NULL) {
        metadata_runtime_fill_binding_report(outReport,
                                             ZR_NULL,
                                             ZR_NULL,
                                             actualModuleVersion,
                                             ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT);
        return ZR_METADATA_RUNTIME_BINDING_STATUS_INVALID_ARGUMENT;
    }

    for (TZrUInt32 index = 0u; index < function->moduleMetadataBindingLength; ++index) {
        const SZrMetadataTokenBinding *binding;
        const SZrMetadataTokenRecord *refRecord;

        if (function->moduleMetadataBindings == ZR_NULL) {
            break;
        }
        binding = &function->moduleMetadataBindings[index];
        refRecord = metadata_runtime_find_binding_ref_record(function, binding);
        status = ZrCore_MetadataRuntime_CheckTokenBindingCompatibility(binding,
                                                                       refRecord,
                                                                       actualModuleVersion,
                                                                       &localReport);
        if (status != ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE) {
            if (outBinding != ZR_NULL) {
                *outBinding = binding;
            }
            if (outRefRecord != ZR_NULL) {
                *outRefRecord = refRecord;
            }
            if (outReport != ZR_NULL) {
                *outReport = localReport;
            }
            return status;
        }
    }

    metadata_runtime_fill_binding_report(outReport,
                                         ZR_NULL,
                                         ZR_NULL,
                                         actualModuleVersion,
                                         ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE);
    return ZR_METADATA_RUNTIME_BINDING_STATUS_COMPATIBLE;
}
