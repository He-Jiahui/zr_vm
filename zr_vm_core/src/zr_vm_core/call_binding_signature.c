#include "zr_vm_core/call_binding.h"

#include "artifact_schema_internal.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/string.h"

static TZrUInt64 signature_string_hash(struct SZrString *string) {
    return string == ZR_NULL ? 0u : ZrCore_Hash_CreateStable64(
            (const TZrByte *)ZrCore_String_GetNativeString(string), ZrCore_String_GetByteLength(string));
}

static TZrUInt64 signature_type_hash(const SZrFunctionTypedTypeRef *type) {
    TZrByte bytes[48] = {0};
    zr_artifact_write_u32(bytes, (TZrUInt32)type->baseType);
    zr_artifact_write_u32(bytes + 4u, type->isNullable ? 1u : 0u);
    zr_artifact_write_u32(bytes + 8u, type->ownershipQualifier);
    zr_artifact_write_u32(bytes + 12u, type->isArray ? 1u : 0u);
    zr_artifact_write_u64(bytes + 16u, signature_string_hash(type->typeName));
    zr_artifact_write_u32(bytes + 24u, (TZrUInt32)type->elementBaseType);
    zr_artifact_write_u32(bytes + 28u, (TZrUInt32)type->staticCType);
    zr_artifact_write_u32(bytes + 32u, type->staticCType == ZR_STATIC_C_TYPE_STRUCT
            ? type->staticCTypeId : ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE);
    zr_artifact_write_u64(bytes + 40u, signature_string_hash(type->elementTypeName));
    return ZrCore_Hash_CreateStable64(bytes, sizeof(bytes));
}

TZrUInt64 ZrCore_CallBinding_FunctionSignatureHash(const struct SZrFunction *function) {
    static const TZrByte prefix[] = {'Z', 'R', 'C', 'A', 'L', 'L', 1u};
    TZrByte bytes[32] = {0};
    TZrUInt64 hash;
    if (function == ZR_NULL || !function->hasCallableReturnType ||
        (function->parameterMetadataCount != 0u && function->parameterMetadata == ZR_NULL)) return 0u;
    zr_artifact_write_u32(bytes, function->parameterCount);
    zr_artifact_write_u32(bytes + 4u, function->hasVariableArguments ? 1u : 0u);
    zr_artifact_write_u32(bytes + 8u, function->parameterMetadataCount);
    zr_artifact_write_u64(bytes + 16u, signature_type_hash(&function->callableReturnType));
    hash = ZrCore_Hash_CreateStable64WithPrefix(prefix, sizeof(prefix), bytes, sizeof(bytes));
    for (TZrUInt32 index = 0u; index < function->parameterMetadataCount; ++index) {
        TZrUInt32 roles = 0u;
        for (TZrUInt32 local = 0u; local < function->typedLocalBindingLength; ++local) {
            if (function->typedLocalBindings[local].stackSlot == index) {
                roles |= function->typedLocalBindings[local].roleFlags;
            }
        }
        zr_artifact_write_u64(bytes, hash);
        zr_artifact_write_u64(bytes + 8u, signature_type_hash(&function->parameterMetadata[index].type));
        zr_artifact_write_u32(bytes + 16u, roles);
        zr_artifact_write_u32(bytes + 20u, index);
        zr_artifact_write_u64(bytes + 24u, 0u);
        hash = ZrCore_Hash_CreateStable64WithPrefix(prefix, sizeof(prefix), bytes, sizeof(bytes));
    }
    return hash;
}
