#include "ffi_runtime_internal.h"

#include "zr_vm_common/zr_io_conf.h"

static const char *zr_ffi_contract_primitive_name(
        EZrFfiTypeKind kind,
        TZrUInt32 size) {
    switch (kind) {
        case ZR_FFI_CONTRACT_TYPE_VOID: return "void";
        case ZR_FFI_CONTRACT_TYPE_BOOL: return "bool";
        case ZR_FFI_CONTRACT_TYPE_I8: return "i8";
        case ZR_FFI_CONTRACT_TYPE_U8: return "u8";
        case ZR_FFI_CONTRACT_TYPE_I16: return "i16";
        case ZR_FFI_CONTRACT_TYPE_U16: return "u16";
        case ZR_FFI_CONTRACT_TYPE_I32: return "i32";
        case ZR_FFI_CONTRACT_TYPE_U32: return "u32";
        case ZR_FFI_CONTRACT_TYPE_I64: return "i64";
        case ZR_FFI_CONTRACT_TYPE_U64: return "u64";
        case ZR_FFI_CONTRACT_TYPE_F32: return "f32";
        case ZR_FFI_CONTRACT_TYPE_F64: return "f64";
        case ZR_FFI_CONTRACT_TYPE_USIZE:
            return size == 4u ? "u32" : "u64";
        case ZR_FFI_CONTRACT_TYPE_ISIZE:
            return size == 4u ? "i32" : "i64";
        case ZR_FFI_CONTRACT_TYPE_ENUM:
            if (size == 1u) return "i8";
            if (size == 2u) return "i16";
            if (size == 4u) return "i32";
            if (size == 8u) return "i64";
            return ZR_NULL;
        default:
            return ZR_NULL;
    }
}

static ZrFfiTypeLayout *zr_ffi_type_from_contract(
        const SZrFfiTypeContract *contractType,
        const SZrFfiSignatureContract *signatureContract,
        char *errorBuffer,
        TZrSize errorBufferSize);

#if ZR_VM_HAS_LIBFFI
static ffi_type *zr_ffi_union_abi_storage_type(
        const ZrFfiTypeLayout *type,
        char *errorBuffer,
        TZrSize errorBufferSize) {
    TZrBool hasIntegerClass = ZR_FALSE;
    TZrBool hasNonFloatingField = ZR_FALSE;
    TZrBool hasHeterogeneousFloatFields = ZR_FALSE;
    ZrFfiTypeKind homogeneousFloatKind = ZR_FFI_TYPE_VOID;

    if (type == ZR_NULL || type->kind != ZR_FFI_TYPE_UNION ||
        type->as.aggregate.fieldCount == 0u) {
        snprintf(errorBuffer, errorBufferSize,
                 "canonical union has no ABI-classifiable fields");
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < type->as.aggregate.fieldCount; index++) {
        ZrFfiTypeKind kind = type->as.aggregate.fields[index].type->kind;

        if (kind != ZR_FFI_TYPE_F32 && kind != ZR_FFI_TYPE_F64) {
            hasNonFloatingField = ZR_TRUE;
            break;
        }
        if (homogeneousFloatKind == ZR_FFI_TYPE_VOID) {
            homogeneousFloatKind = kind;
        } else if (kind != homogeneousFloatKind) {
            hasHeterogeneousFloatFields = ZR_TRUE;
        }
    }
#if defined(ZR_PLATFORM_WIN)
    hasIntegerClass = ZR_TRUE;
#elif defined(__aarch64__) || defined(__arm__) || defined(_M_ARM64) || defined(_M_ARM)
    hasIntegerClass =
            hasNonFloatingField || hasHeterogeneousFloatFields;
#else
    hasIntegerClass = hasNonFloatingField;
#endif
    if (!hasIntegerClass) {
        if ((homogeneousFloatKind == ZR_FFI_TYPE_F64 ||
             hasHeterogeneousFloatFields) &&
            type->size == sizeof(double) &&
            type->align == _Alignof(double)) {
            return &ffi_type_double;
        }
        if (homogeneousFloatKind == ZR_FFI_TYPE_F32 &&
            type->size == sizeof(float) &&
            type->align == _Alignof(float)) {
            return &ffi_type_float;
        }
    } else {
        if (type->size == sizeof(uint64_t) && type->align == _Alignof(uint64_t)) {
            return &ffi_type_uint64;
        }
        if (type->size == sizeof(uint32_t) && type->align == _Alignof(uint32_t)) {
            return &ffi_type_uint32;
        }
        if (type->size == sizeof(uint16_t) && type->align == _Alignof(uint16_t)) {
            return &ffi_type_uint16;
        }
        if (type->size == sizeof(uint8_t) && type->align == _Alignof(uint8_t)) {
            return &ffi_type_uint8;
        }
    }
    snprintf(
            errorBuffer,
            errorBufferSize,
            "canonical union ABI class is not representable by libffi");
    return ZR_NULL;
}
#endif

static ZrFfiTypeLayout *zr_ffi_aggregate_from_contract(
        const SZrFfiTypeContract *contractType,
        const SZrFfiSignatureContract *signatureContract,
        char *errorBuffer,
        TZrSize errorBufferSize) {
    ZrFfiTypeLayout *type;

    type = zr_ffi_new_type(
            contractType->typeKind == ZR_FFI_CONTRACT_TYPE_UNION
                    ? ZR_FFI_TYPE_UNION
                    : ZR_FFI_TYPE_STRUCT);
    if (type == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize,
                 "out of memory while lowering aggregate contract");
        return ZR_NULL;
    }
    type->name = zr_ffi_strdup(
            contractType->typeKind == ZR_FFI_CONTRACT_TYPE_UNION
                    ? "contract-union"
                    : "contract-struct");
    type->size = contractType->size;
    type->align = contractType->alignment;
    type->as.aggregate.fieldCount = contractType->aggregateFieldCount;
    type->as.aggregate.fields = (ZrFfiFieldLayout *)calloc(
            type->as.aggregate.fieldCount, sizeof(ZrFfiFieldLayout));
    if (type->as.aggregate.fields == ZR_NULL) {
        zr_ffi_destroy_type(type);
        snprintf(errorBuffer, errorBufferSize,
                 "out of memory while lowering aggregate fields");
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < type->as.aggregate.fieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &signatureContract->aggregateFields[
                        contractType->aggregateFieldStart + index];
        SZrFfiTypeContract fieldType = {0};

        fieldType.typeKind = field->typeKind;
        fieldType.size = field->size;
        fieldType.alignment = field->alignment;
        fieldType.flags = ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE;
        type->as.aggregate.fields[index].name = zr_ffi_strdup(field->name);
        type->as.aggregate.fields[index].offset = field->offset;
        type->as.aggregate.fields[index].type = zr_ffi_type_from_contract(
                &fieldType,
                signatureContract,
                errorBuffer,
                errorBufferSize);
        if (type->as.aggregate.fields[index].name == ZR_NULL ||
            type->as.aggregate.fields[index].type == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
    }
#if ZR_VM_HAS_LIBFFI
    {
        TZrSize ffiElementCount = type->kind == ZR_FFI_TYPE_UNION
                                         ? 1u
                                         : type->as.aggregate.fieldCount;

        type->ffiElements = (ffi_type **)calloc(
                ffiElementCount + 1u, sizeof(ffi_type *));
    }
    if (type->ffiElements == ZR_NULL) {
        zr_ffi_destroy_type(type);
        snprintf(errorBuffer, errorBufferSize,
                 "out of memory while lowering aggregate ABI elements");
        return ZR_NULL;
    }
    if (type->kind == ZR_FFI_TYPE_UNION) {
        type->ffiElements[0] = zr_ffi_union_abi_storage_type(
                type, errorBuffer, errorBufferSize);
        if (type->ffiElements[0] == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
    } else {
        for (TZrSize index = 0u;
             index < type->as.aggregate.fieldCount;
             index++) {
            type->ffiElements[index] =
                    type->as.aggregate.fields[index].type->ffiType;
        }
    }
    memset(&type->ffiAggregateType, 0, sizeof(type->ffiAggregateType));
    type->ffiAggregateType.size = 0u;
    type->ffiAggregateType.alignment = 0u;
    type->ffiAggregateType.type = FFI_TYPE_STRUCT;
    type->ffiAggregateType.elements = type->ffiElements;
    type->ffiType = &type->ffiAggregateType;
#endif
    return type;
}

#if ZR_VM_HAS_LIBFFI
static TZrBool zr_ffi_validate_contract_layout(
        ZrFfiTypeLayout *type,
        ffi_abi abi,
        char *errorBuffer,
        TZrSize errorBufferSize) {
    size_t *offsets;
    TZrSize ffiElementCount;
    ffi_status status;

    if (type == ZR_NULL) {
        return ZR_FALSE;
    }
    if (type->kind == ZR_FFI_TYPE_POINTER) {
        return zr_ffi_validate_contract_layout(
                type->as.pointer.pointee, abi, errorBuffer, errorBufferSize);
    }
    if (type->kind != ZR_FFI_TYPE_STRUCT &&
        type->kind != ZR_FFI_TYPE_UNION) {
        return ZR_TRUE;
    }

    ffiElementCount = type->kind == ZR_FFI_TYPE_UNION
            ? 1u
            : type->as.aggregate.fieldCount;
    offsets = (size_t *)calloc(ffiElementCount, sizeof(size_t));
    if (offsets == ZR_NULL) {
        snprintf(
                errorBuffer,
                errorBufferSize,
                "out of memory while validating canonical aggregate layout");
        return ZR_FALSE;
    }
    type->ffiAggregateType.size = 0u;
    type->ffiAggregateType.alignment = 0u;
    status = ffi_get_struct_offsets(abi, &type->ffiAggregateType, offsets);
    if (status != FFI_OK || type->ffiAggregateType.size != type->size ||
        type->ffiAggregateType.alignment != type->align) {
        snprintf(
                errorBuffer,
                errorBufferSize,
                "canonical aggregate layout size/alignment does not match libffi ABI");
        free(offsets);
        return ZR_FALSE;
    }
    if (type->kind == ZR_FFI_TYPE_STRUCT) {
        for (TZrSize index = 0u;
             index < type->as.aggregate.fieldCount;
             index++) {
            if (offsets[index] !=
                type->as.aggregate.fields[index].offset) {
                snprintf(
                        errorBuffer,
                        errorBufferSize,
                        "canonical aggregate field layout does not match libffi ABI");
                free(offsets);
                return ZR_FALSE;
            }
        }
    } else {
        for (TZrSize index = 0u;
             index < type->as.aggregate.fieldCount;
             index++) {
            if (type->as.aggregate.fields[index].offset != 0u) {
                snprintf(
                        errorBuffer,
                        errorBufferSize,
                        "canonical union field layout must start at offset zero");
                free(offsets);
                return ZR_FALSE;
            }
        }
    }
    free(offsets);
    return ZR_TRUE;
}
#endif

static ZrFfiTypeLayout *zr_ffi_type_from_contract(
        const SZrFfiTypeContract *contractType,
        const SZrFfiSignatureContract *signatureContract,
        char *errorBuffer,
        TZrSize errorBufferSize) {
    const char *primitiveName;
    ZrFfiTypeLayout *type;

    if (contractType == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize,
                 "missing canonical FFI type contract");
        return ZR_NULL;
    }
    primitiveName = zr_ffi_contract_primitive_name(
            contractType->typeKind, contractType->size);
    if (primitiveName != ZR_NULL) {
        type = zr_ffi_make_primitive_type(primitiveName);
        if (type == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize,
                     "failed to lower primitive FFI contract");
        }
        return type;
    }
    if (contractType->typeKind == ZR_FFI_CONTRACT_TYPE_POINTER) {
        ZrFfiTypeLayout *target = zr_ffi_make_primitive_type("void");

        type = zr_ffi_pointer_type_from_target(target);
        zr_ffi_destroy_type(target);
        if (type == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize,
                     "failed to lower pointer FFI contract");
        }
        return type;
    }
    if (contractType->typeKind == ZR_FFI_CONTRACT_TYPE_CALLBACK) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_FUNCTION);
        if (type == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize,
                     "failed to lower callback FFI contract");
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("contract-callback");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        type->canonicalSignatureHash = contractType->layoutHash;
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        if (type->name == ZR_NULL) {
            zr_ffi_destroy_type(type);
            snprintf(errorBuffer, errorBufferSize,
                     "out of memory while lowering callback FFI contract");
            return ZR_NULL;
        }
        return type;
    }
    if (contractType->typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
        contractType->typeKind == ZR_FFI_CONTRACT_TYPE_UNION) {
        return zr_ffi_aggregate_from_contract(
                contractType,
                signatureContract,
                errorBuffer,
                errorBufferSize);
    }
    snprintf(errorBuffer,
             errorBufferSize,
             "unsupported canonical FFI type kind %u",
             (unsigned)contractType->typeKind);
    return ZR_NULL;
}

static TZrBool zr_ffi_contract_is_available(
        const SZrNativeImportContract *contract) {
#if defined(ZR_PLATFORM_WIN)
    return (TZrBool)((contract->availability &
                      ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS) != 0u);
#else
    return (TZrBool)((contract->availability &
                      ZR_FFI_CONTRACT_AVAILABILITY_UNIX) != 0u);
#endif
}

ZrFfiSignature *zr_ffi_signature_from_contract(
        const SZrNativeImportContract *contract,
        char *errorBuffer,
        TZrSize errorBufferSize) {
    ZrFfiSignature *signature;
    const SZrFfiSignatureContract *source;

    if (errorBuffer != ZR_NULL && errorBufferSize > 0u) {
        errorBuffer[0] = '\0';
    }
    if (!ZrCommon_NativeImportContract_Validate(contract)) {
        snprintf(errorBuffer, errorBufferSize,
                 "invalid canonical native import contract");
        return ZR_NULL;
    }
    if (!zr_ffi_contract_is_available(contract)) {
        snprintf(errorBuffer, errorBufferSize,
                 "native import is unavailable on this platform");
        return ZR_NULL;
    }
    if ((contract->requiredCapabilities &
         ~ZR_VM_NATIVE_RUNTIME_CAPABILITIES) != 0u) {
        snprintf(errorBuffer, errorBufferSize,
                 "native import requires unavailable runtime capabilities");
        return ZR_NULL;
    }
    source = &contract->signature;
    if (source->targetPointerSize != sizeof(void *) ||
        source->targetEndianness !=
                (ZR_IO_IS_LITTLE_ENDIAN ? ZR_FFI_CONTRACT_ENDIAN_LITTLE
                                        : ZR_FFI_CONTRACT_ENDIAN_BIG)) {
        snprintf(errorBuffer, errorBufferSize,
                 "native import target ABI does not match this runtime");
        return ZR_NULL;
    }
    if (source->cleanupPolicy == ZR_FFI_CONTRACT_CLEANUP_REGISTERED) {
        snprintf(
                errorBuffer,
                errorBufferSize,
                "native cleanup policy requires an executable cleanup entry contract");
        return ZR_NULL;
    }
    if (source->errorPolicy == ZR_FFI_CONTRACT_ERROR_THROWS) {
        snprintf(
                errorBuffer,
                errorBufferSize,
                "native error policy requires an executable error contract");
        return ZR_NULL;
    }
#if !defined(ZR_PLATFORM_WIN)
    if (source->errorPolicy == ZR_FFI_CONTRACT_ERROR_LAST_ERROR) {
        snprintf(
                errorBuffer,
                errorBufferSize,
                "native last-error policy is only executable on Windows");
        return ZR_NULL;
    }
#endif
    if (source->callbackLifetime ==
                ZR_FFI_CONTRACT_CALLBACK_LIFETIME_SCOPED ||
        source->callbackLifetime ==
                ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC ||
        source->callbackThreadPolicy ==
                ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH ||
        source->callbackExceptionPolicy ==
                ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ERROR_RESULT) {
        snprintf(
                errorBuffer,
                errorBufferSize,
                "native callback policy is not executable in this runtime");
        return ZR_NULL;
    }

    signature = (ZrFfiSignature *)calloc(1u, sizeof(ZrFfiSignature));
    if (signature == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize,
                 "out of memory while lowering native import contract");
        return ZR_NULL;
    }
    signature->parameterCount = source->parameterCount;
    signature->isVarargs = source->isVariadic;
    signature->errorPolicy = source->errorPolicy;
    signature->cleanupPolicy = source->cleanupPolicy;
    signature->callbackLifetime = source->callbackLifetime;
    signature->callbackThreadPolicy = source->callbackThreadPolicy;
    signature->callbackExceptionPolicy = source->callbackExceptionPolicy;
    signature->returnType = zr_ffi_type_from_contract(
            &source->returnType,
            source,
            errorBuffer,
            errorBufferSize);
    if (signature->returnType == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
    if (signature->parameterCount > 0u) {
        signature->parameters = (ZrFfiParameter *)calloc(
                signature->parameterCount, sizeof(ZrFfiParameter));
    }
    if (signature->parameterCount > 0u &&
        signature->parameters == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize,
                 "out of memory while lowering native parameters");
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < signature->parameterCount; index++) {
        const SZrFfiParameterContract *parameter = &source->parameters[index];
        ZrFfiTypeLayout *parameterType = zr_ffi_type_from_contract(
                &parameter->type,
                source,
                errorBuffer,
                errorBufferSize);

        if (parameterType == ZR_NULL) {
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
        if (parameter->direction != ZR_FFI_CONTRACT_DIRECTION_IN) {
            ZrFfiTypeLayout *pointerType =
                    zr_ffi_pointer_type_from_target(parameterType);

            zr_ffi_destroy_type(parameterType);
            parameterType = pointerType;
            if (parameterType != ZR_NULL) {
                parameterType->as.pointer.direction =
                        parameter->direction == ZR_FFI_CONTRACT_DIRECTION_OUT
                                ? ZR_FFI_DIRECTION_OUT
                                : ZR_FFI_DIRECTION_INOUT;
            }
        }
        if (parameterType == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize,
                     "failed to lower ref/out FFI parameter");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
        signature->parameters[index].type = parameterType;
    }

    if (source->abi == ZR_FFI_CONTRACT_ABI_STDCALL) {
        signature->abi = zr_ffi_parse_abi(
                "stdcall", errorBuffer, errorBufferSize);
    } else {
        signature->abi = zr_ffi_parse_abi(
                "cdecl", errorBuffer, errorBufferSize);
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0u &&
        errorBuffer[0] != '\0') {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
#if ZR_VM_HAS_LIBFFI
    if (!zr_ffi_validate_contract_layout(
                signature->returnType,
                signature->abi,
                errorBuffer,
                errorBufferSize)) {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
    for (TZrSize index = 0u;
         index < signature->parameterCount;
         index++) {
        if (!zr_ffi_validate_contract_layout(
                    signature->parameters[index].type,
                    signature->abi,
                    errorBuffer,
                    errorBufferSize)) {
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    }
    if (signature->parameterCount > 0u) {
        signature->ffiParameterTypes = (ffi_type **)calloc(
                signature->parameterCount, sizeof(ffi_type *));
    }
    if (signature->parameterCount > 0u &&
        signature->ffiParameterTypes == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize,
                 "out of memory while lowering native ABI parameters");
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
    for (TZrSize index = 0u; index < signature->parameterCount; index++) {
        signature->ffiParameterTypes[index] =
                signature->parameters[index].type->ffiType;
    }
    if (signature->isVarargs) {
        signature->cifPrepared = (TZrBool)(
                ffi_prep_cif_var(
                        &signature->cif,
                        signature->abi,
                        (unsigned int)signature->parameterCount,
                        (unsigned int)signature->parameterCount,
                        signature->returnType->ffiType,
                        signature->ffiParameterTypes) == FFI_OK);
    } else {
        signature->cifPrepared = (TZrBool)(
                ffi_prep_cif(
                        &signature->cif,
                        signature->abi,
                        (unsigned int)signature->parameterCount,
                        signature->returnType->ffiType,
                        signature->ffiParameterTypes) == FFI_OK);
    }
    if (!signature->cifPrepared) {
        snprintf(errorBuffer, errorBufferSize,
                 "libffi rejected the canonical native signature");
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
#endif
    return signature;
}

TZrBool ZrVmLibFfi_ValidateNativeImportContract(
        const SZrNativeImportContract *contract,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    ZrFfiSignature *signature = zr_ffi_signature_from_contract(
            contract, errorBuffer, errorBufferSize);

    if (signature == ZR_NULL) {
        return ZR_FALSE;
    }
    zr_ffi_destroy_signature(signature);
    return ZR_TRUE;
}
