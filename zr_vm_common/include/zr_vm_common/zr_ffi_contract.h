#ifndef ZR_FFI_CONTRACT_H
#define ZR_FFI_CONTRACT_H

#include "zr_vm_common/zr_common_conf.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define ZR_FFI_CONTRACT_SCHEMA_VERSION ((TZrUInt32)3u)
#define ZR_FFI_CONTRACT_ABI_MODEL_VERSION ((TZrUInt32)3u)
#define ZR_FFI_CONTRACT_MAX_PARAMETERS ((TZrUInt32)32u)
#define ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS ((TZrUInt32)64u)
#define ZR_FFI_CONTRACT_MAX_IMPORTS_PER_FUNCTION ((TZrUInt32)1024u)
#define ZR_FFI_CONTRACT_LIBRARY_CAPACITY ((TZrSize)512u)
#define ZR_FFI_CONTRACT_ENTRY_CAPACITY ((TZrSize)128u)
#define ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY ((TZrSize)512u)
#define ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY ((TZrSize)32u)
#define ZR_FFI_CONTRACT_TARGET_TRIPLE_CAPACITY ((TZrSize)64u)

#define ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS ((TZrUInt32)1u << 0u)
#define ZR_FFI_CONTRACT_AVAILABILITY_UNIX ((TZrUInt32)1u << 1u)
#define ZR_FFI_CONTRACT_AVAILABILITY_ALL                                      \
    (ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS | ZR_FFI_CONTRACT_AVAILABILITY_UNIX)

/* Mirrors the public native-provider FFI runtime capability bit. */
#define ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME ((TZrUInt64)1u << 4u)

typedef enum EZrFfiAbi {
    ZR_FFI_CONTRACT_ABI_SYSTEM = 0,
    ZR_FFI_CONTRACT_ABI_C,
    ZR_FFI_CONTRACT_ABI_STDCALL
} EZrFfiAbi;

typedef enum EZrFfiDirection {
    ZR_FFI_CONTRACT_DIRECTION_IN = 0,
    ZR_FFI_CONTRACT_DIRECTION_REF,
    ZR_FFI_CONTRACT_DIRECTION_OUT
} EZrFfiDirection;

typedef enum EZrFfiTypeKind {
    ZR_FFI_CONTRACT_TYPE_VOID = 0,
    ZR_FFI_CONTRACT_TYPE_BOOL,
    ZR_FFI_CONTRACT_TYPE_I8,
    ZR_FFI_CONTRACT_TYPE_U8,
    ZR_FFI_CONTRACT_TYPE_I16,
    ZR_FFI_CONTRACT_TYPE_U16,
    ZR_FFI_CONTRACT_TYPE_I32,
    ZR_FFI_CONTRACT_TYPE_U32,
    ZR_FFI_CONTRACT_TYPE_I64,
    ZR_FFI_CONTRACT_TYPE_U64,
    ZR_FFI_CONTRACT_TYPE_F32,
    ZR_FFI_CONTRACT_TYPE_F64,
    ZR_FFI_CONTRACT_TYPE_USIZE,
    ZR_FFI_CONTRACT_TYPE_ISIZE,
    ZR_FFI_CONTRACT_TYPE_POINTER,
    ZR_FFI_CONTRACT_TYPE_STRUCT,
    ZR_FFI_CONTRACT_TYPE_UNION,
    ZR_FFI_CONTRACT_TYPE_ENUM,
    ZR_FFI_CONTRACT_TYPE_CALLBACK,
    ZR_FFI_CONTRACT_TYPE_UNSUPPORTED
} EZrFfiTypeKind;

typedef enum EZrFfiMarshallingKind {
    ZR_FFI_CONTRACT_MARSHALLING_DIRECT = 0,
    ZR_FFI_CONTRACT_MARSHALLING_PIN,
    ZR_FFI_CONTRACT_MARSHALLING_COPY,
    ZR_FFI_CONTRACT_MARSHALLING_REGISTERED
} EZrFfiMarshallingKind;

typedef enum EZrFfiParameterOwnership {
    ZR_FFI_CONTRACT_OWNERSHIP_BORROWED = 0,
    ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER,
    ZR_FFI_CONTRACT_OWNERSHIP_SHARED,
    ZR_FFI_CONTRACT_OWNERSHIP_PINNED
} EZrFfiParameterOwnership;

typedef enum EZrFfiCharset {
    ZR_FFI_CONTRACT_CHARSET_NONE = 0,
    ZR_FFI_CONTRACT_CHARSET_UTF8,
    ZR_FFI_CONTRACT_CHARSET_UTF16,
    ZR_FFI_CONTRACT_CHARSET_ANSI
} EZrFfiCharset;

typedef enum EZrFfiErrorPolicy {
    ZR_FFI_CONTRACT_ERROR_NONE = 0,
    ZR_FFI_CONTRACT_ERROR_RETURN_CODE,
    ZR_FFI_CONTRACT_ERROR_LAST_ERROR,
    ZR_FFI_CONTRACT_ERROR_ERRNO,
    ZR_FFI_CONTRACT_ERROR_THROWS
} EZrFfiErrorPolicy;

typedef enum EZrFfiCleanupPolicy {
    ZR_FFI_CONTRACT_CLEANUP_NONE = 0,
    ZR_FFI_CONTRACT_CLEANUP_CALLER,
    ZR_FFI_CONTRACT_CLEANUP_CALLEE,
    ZR_FFI_CONTRACT_CLEANUP_REGISTERED
} EZrFfiCleanupPolicy;

typedef enum EZrFfiCallbackLifetime {
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE = 0,
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL,
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_SCOPED,
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC
} EZrFfiCallbackLifetime;

typedef enum EZrFfiCallbackThreadPolicy {
    ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE = 0,
    ZR_FFI_CONTRACT_CALLBACK_THREAD_CALLER,
    ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH,
    ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN
} EZrFfiCallbackThreadPolicy;

typedef enum EZrFfiCallbackExceptionPolicy {
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE = 0,
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ABORT,
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_RETURN_DEFAULT,
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ERROR_RESULT
} EZrFfiCallbackExceptionPolicy;

typedef enum EZrFfiTargetEndianness {
    ZR_FFI_CONTRACT_ENDIAN_LITTLE = 0,
    ZR_FFI_CONTRACT_ENDIAN_BIG
} EZrFfiTargetEndianness;

#define ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE ((TZrUInt32)1u << 0u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_GC_REFERENCE ((TZrUInt32)1u << 1u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_REF_LIKE ((TZrUInt32)1u << 2u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_RESOURCE ((TZrUInt32)1u << 3u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_OWNER ((TZrUInt32)1u << 4u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_MASK ((TZrUInt32)0x1fu)

typedef struct SZrFfiAggregateFieldContract {
    TZrChar name[ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY];
    EZrFfiTypeKind typeKind;
    TZrUInt32 size;
    TZrUInt32 alignment;
    TZrUInt32 offset;
} SZrFfiAggregateFieldContract;

typedef struct SZrFfiTypeContract {
    EZrFfiTypeKind typeKind;
    TZrUInt32 size;
    TZrUInt32 alignment;
    TZrUInt64 canonicalTypeHash;
    TZrUInt64 layoutHash;
    TZrUInt32 flags;
    TZrUInt32 aggregateFieldStart;
    TZrUInt32 aggregateFieldCount;
} SZrFfiTypeContract;

typedef struct SZrFfiParameterContract {
    SZrFfiTypeContract type;
    EZrFfiDirection direction;
    EZrFfiMarshallingKind marshalling;
    EZrFfiParameterOwnership ownership;
    TZrBool isNullable;
    TZrUInt32 flags;
} SZrFfiParameterContract;

typedef struct SZrFfiSignatureContract {
    EZrFfiAbi abi;
    TZrUInt32 targetPointerSize;
    EZrFfiTargetEndianness targetEndianness;
    TZrChar targetTriple[ZR_FFI_CONTRACT_TARGET_TRIPLE_CAPACITY];
    TZrUInt64 targetAbiHash;
    EZrFfiCharset charset;
    EZrFfiErrorPolicy errorPolicy;
    EZrFfiCleanupPolicy cleanupPolicy;
    EZrFfiCallbackLifetime callbackLifetime;
    EZrFfiCallbackThreadPolicy callbackThreadPolicy;
    EZrFfiCallbackExceptionPolicy callbackExceptionPolicy;
    TZrBool isVariadic;
    TZrUInt32 parameterCount;
    SZrFfiTypeContract returnType;
    SZrFfiParameterContract parameters[ZR_FFI_CONTRACT_MAX_PARAMETERS];
    TZrUInt32 aggregateFieldCount;
    SZrFfiAggregateFieldContract
            aggregateFields[ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS];
    TZrUInt64 signatureHash;
} SZrFfiSignatureContract;

typedef struct SZrFfiSourceMapping {
    TZrChar document[ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY];
    TZrUInt64 startOffset;
    TZrUInt64 endOffset;
    TZrInt32 startLine;
    TZrInt32 startColumn;
    TZrInt32 endLine;
    TZrInt32 endColumn;
} SZrFfiSourceMapping;

typedef struct SZrNativeImportContract {
    TZrUInt32 schemaVersion;
    TZrChar libraryLocator[ZR_FFI_CONTRACT_LIBRARY_CAPACITY];
    TZrChar entryPoint[ZR_FFI_CONTRACT_ENTRY_CAPACITY];
    TZrUInt64 symbolId;
    TZrUInt64 declaringModuleId;
    TZrUInt64 callableContractHash;
    TZrUInt32 availability;
    TZrUInt64 requiredCapabilities;
    SZrFfiSourceMapping sourceMapping;
    SZrFfiSignatureContract signature;
} SZrNativeImportContract;

#define ZR_FFI_CONTRACT_FNV_OFFSET UINT64_C(1469598103934665603)
#define ZR_FFI_CONTRACT_FNV_PRIME UINT64_C(1099511628211)

static inline TZrUInt64 ZrCommon_FfiContract_HashByte(
        TZrUInt64 hash,
        TZrUInt8 value) {
    return (hash ^ value) * ZR_FFI_CONTRACT_FNV_PRIME;
}

static inline TZrUInt64 ZrCommon_FfiContract_HashU32(
        TZrUInt64 hash,
        TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; index++) {
        hash = ZrCommon_FfiContract_HashByte(
                hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static inline TZrUInt64 ZrCommon_FfiContract_HashU64(
        TZrUInt64 hash,
        TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; index++) {
        hash = ZrCommon_FfiContract_HashByte(
                hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

static inline const TZrChar *ZrCommon_FfiContract_GetHostTargetTriple(void) {
#if defined(_M_X64) || defined(__x86_64__)
#define ZR_FFI_HOST_ARCH "x86_64"
#elif defined(_M_ARM64) || defined(__aarch64__)
#define ZR_FFI_HOST_ARCH "aarch64"
#elif defined(_M_IX86) || defined(__i386__)
#define ZR_FFI_HOST_ARCH "i686"
#elif defined(_M_ARM) || defined(__arm__)
#define ZR_FFI_HOST_ARCH "arm"
#else
#define ZR_FFI_HOST_ARCH "unknown"
#endif

#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    return ZR_FFI_HOST_ARCH "-pc-windows-msvc";
#elif defined(__APPLE__)
    return ZR_FFI_HOST_ARCH "-apple-darwin";
#elif defined(__linux__)
    return ZR_FFI_HOST_ARCH "-unknown-linux-gnu";
#elif defined(ZR_PLATFORM_UNIX) || defined(__unix__)
    return ZR_FFI_HOST_ARCH "-unknown-unix";
#else
    return ZR_FFI_HOST_ARCH "-unknown-unknown";
#endif

#undef ZR_FFI_HOST_ARCH
}

static inline TZrUInt64 ZrCommon_FfiContract_HashText(
        TZrUInt64 hash,
        const TZrChar *text) {
    if (text == ZR_NULL) {
        return ZrCommon_FfiContract_HashByte(hash, 0u);
    }
    for (TZrSize index = 0u;; index++) {
        hash = ZrCommon_FfiContract_HashByte(
                hash, (TZrUInt8)text[index]);
        if (text[index] == '\0') {
            return hash;
        }
    }
}

static inline TZrUInt64 ZrCommon_FfiContract_HashType(
        TZrUInt64 hash,
        const SZrFfiTypeContract *type) {
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)type->typeKind);
    hash = ZrCommon_FfiContract_HashU32(hash, type->size);
    hash = ZrCommon_FfiContract_HashU32(hash, type->alignment);
    hash = ZrCommon_FfiContract_HashU64(hash, type->canonicalTypeHash);
    hash = ZrCommon_FfiContract_HashU64(hash, type->layoutHash);
    hash = ZrCommon_FfiContract_HashU32(hash, type->flags);
    hash = ZrCommon_FfiContract_HashU32(hash, type->aggregateFieldStart);
    return ZrCommon_FfiContract_HashU32(hash, type->aggregateFieldCount);
}

static inline TZrUInt64 ZrCommon_FfiContract_ComputeTargetAbiHash(
        EZrFfiAbi abi,
        TZrUInt32 pointerSize,
        EZrFfiTargetEndianness endianness,
        const TZrChar *targetTriple) {
    TZrUInt64 hash = ZR_FFI_CONTRACT_FNV_OFFSET;
    TZrUInt32 platformFamily = 0u;
    TZrUInt32 architecture = 0u;

#if defined(ZR_PLATFORM_WIN) || defined(_WIN32)
    platformFamily = 1u;
#elif defined(ZR_PLATFORM_UNIX) || defined(__unix__) || defined(__APPLE__)
    platformFamily = 2u;
#endif
#if defined(_M_X64) || defined(__x86_64__)
    architecture = 1u;
#elif defined(_M_ARM64) || defined(__aarch64__)
    architecture = 2u;
#elif defined(_M_IX86) || defined(__i386__)
    architecture = 3u;
#elif defined(_M_ARM) || defined(__arm__)
    architecture = 4u;
#endif

    hash = ZrCommon_FfiContract_HashU32(
            hash, ZR_FFI_CONTRACT_ABI_MODEL_VERSION);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)abi);
    hash = ZrCommon_FfiContract_HashU32(hash, pointerSize);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)endianness);
    hash = ZrCommon_FfiContract_HashText(hash, targetTriple);
    hash = ZrCommon_FfiContract_HashU32(hash, platformFamily);
    hash = ZrCommon_FfiContract_HashU32(hash, architecture);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(short));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)_Alignof(short));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(int));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)_Alignof(int));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(long));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)_Alignof(long));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(long long));
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)_Alignof(long long));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(float));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)_Alignof(float));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(double));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)_Alignof(double));
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)sizeof(long double));
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)_Alignof(long double));
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)sizeof(size_t));
#if defined(ZR_COMPILER_MSVC)
    /* MSVC C11 does not expose max_align_t; keep the runtime ABI alignment. */
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)ZR_ALIGN_SIZE);
#else
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)_Alignof(max_align_t));
#endif
    return ZrCommon_FfiContract_HashU32(
            hash, CHAR_MIN < 0 ? 1u : 0u);
}

static inline TZrUInt64 ZrCommon_FfiSignatureContract_ComputeHash(
        const SZrFfiSignatureContract *signature) {
    TZrUInt64 hash = ZR_FFI_CONTRACT_FNV_OFFSET;

    if (signature == ZR_NULL ||
        signature->parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS ||
        signature->aggregateFieldCount >
                ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS) {
        return 0u;
    }
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)signature->abi);
    hash = ZrCommon_FfiContract_HashU32(hash, signature->targetPointerSize);
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)signature->targetEndianness);
    hash = ZrCommon_FfiContract_HashText(hash, signature->targetTriple);
    hash = ZrCommon_FfiContract_HashU64(hash, signature->targetAbiHash);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)signature->charset);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)signature->errorPolicy);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)signature->cleanupPolicy);
    hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)signature->callbackLifetime);
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)signature->callbackThreadPolicy);
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)signature->callbackExceptionPolicy);
    hash = ZrCommon_FfiContract_HashU32(
            hash, signature->isVariadic ? 1u : 0u);
    hash = ZrCommon_FfiContract_HashU32(hash, signature->parameterCount);
    hash = ZrCommon_FfiContract_HashType(hash, &signature->returnType);
    for (TZrUInt32 index = 0u; index < signature->parameterCount; index++) {
        const SZrFfiParameterContract *parameter = &signature->parameters[index];

        hash = ZrCommon_FfiContract_HashType(hash, &parameter->type);
        hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)parameter->direction);
        hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)parameter->marshalling);
        hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)parameter->ownership);
        hash = ZrCommon_FfiContract_HashU32(
                hash, parameter->isNullable ? 1u : 0u);
        hash = ZrCommon_FfiContract_HashU32(hash, parameter->flags);
    }
    hash = ZrCommon_FfiContract_HashU32(
            hash, signature->aggregateFieldCount);
    for (TZrUInt32 index = 0u;
         index < signature->aggregateFieldCount;
         index++) {
        const SZrFfiAggregateFieldContract *field =
                &signature->aggregateFields[index];

        for (TZrSize nameIndex = 0u;
             nameIndex < sizeof(field->name) && field->name[nameIndex] != '\0';
             nameIndex++) {
            hash = ZrCommon_FfiContract_HashByte(
                    hash, (TZrUInt8)field->name[nameIndex]);
        }
        hash = ZrCommon_FfiContract_HashByte(hash, 0u);
        hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)field->typeKind);
        hash = ZrCommon_FfiContract_HashU32(hash, field->size);
        hash = ZrCommon_FfiContract_HashU32(hash, field->alignment);
        hash = ZrCommon_FfiContract_HashU32(hash, field->offset);
    }
    return hash;
}

static inline TZrBool ZrCommon_FfiTypeContract_Validate(
        const SZrFfiTypeContract *type,
        TZrBool allowVoid) {
    if (type == ZR_NULL ||
        (TZrUInt32)type->typeKind > (TZrUInt32)ZR_FFI_CONTRACT_TYPE_CALLBACK ||
        type->typeKind == ZR_FFI_CONTRACT_TYPE_UNSUPPORTED ||
        (type->flags & ~ZR_FFI_CONTRACT_TYPE_FLAG_MASK) != 0u ||
        (type->flags & (ZR_FFI_CONTRACT_TYPE_FLAG_GC_REFERENCE |
                        ZR_FFI_CONTRACT_TYPE_FLAG_REF_LIKE |
                        ZR_FFI_CONTRACT_TYPE_FLAG_RESOURCE |
                        ZR_FFI_CONTRACT_TYPE_FLAG_OWNER)) != 0u) {
        return ZR_FALSE;
    }
    if (type->typeKind == ZR_FFI_CONTRACT_TYPE_VOID) {
        return (TZrBool)(allowVoid && type->size == 0u && type->flags == 0u &&
                         type->aggregateFieldStart == 0u &&
                         type->aggregateFieldCount == 0u);
    }
    if (type->size == 0u || type->alignment == 0u ||
        (type->alignment & (type->alignment - 1u)) != 0u) {
        return ZR_FALSE;
    }
    if ((type->typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
         type->typeKind == ZR_FFI_CONTRACT_TYPE_UNION ||
         type->typeKind == ZR_FFI_CONTRACT_TYPE_CALLBACK) &&
        type->layoutHash == 0u) {
        return ZR_FALSE;
    }
    if (type->typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
        type->typeKind == ZR_FFI_CONTRACT_TYPE_UNION) {
        if (type->aggregateFieldCount == 0u ||
            type->aggregateFieldStart > ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS ||
            type->aggregateFieldCount >
                    ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS -
                            type->aggregateFieldStart) {
            return ZR_FALSE;
        }
    } else if (type->aggregateFieldStart != 0u ||
               type->aggregateFieldCount != 0u) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static inline TZrBool ZrCommon_FfiTypeAggregateRange_Validate(
        const SZrFfiSignatureContract *signature,
        const SZrFfiTypeContract *type) {
    if (signature == ZR_NULL || type == ZR_NULL ||
        type->aggregateFieldStart > signature->aggregateFieldCount ||
        type->aggregateFieldCount >
                signature->aggregateFieldCount - type->aggregateFieldStart) {
        return ZR_FALSE;
    }
    if (type->typeKind != ZR_FFI_CONTRACT_TYPE_STRUCT &&
        type->typeKind != ZR_FFI_CONTRACT_TYPE_UNION) {
        return ZR_TRUE;
    }
    for (TZrUInt32 index = 0u; index < type->aggregateFieldCount; index++) {
        const SZrFfiAggregateFieldContract *field =
                &signature->aggregateFields[type->aggregateFieldStart + index];

        if (memchr(field->name, '\0', sizeof(field->name)) == ZR_NULL ||
            field->name[0] == '\0' ||
            field->typeKind == ZR_FFI_CONTRACT_TYPE_VOID ||
            (TZrUInt32)field->typeKind > (TZrUInt32)ZR_FFI_CONTRACT_TYPE_CALLBACK ||
            field->typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
            field->typeKind == ZR_FFI_CONTRACT_TYPE_UNION ||
            field->typeKind == ZR_FFI_CONTRACT_TYPE_UNSUPPORTED ||
            field->size == 0u || field->alignment == 0u ||
            (field->alignment & (field->alignment - 1u)) != 0u ||
            field->offset > type->size ||
            field->size > type->size - field->offset) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static inline TZrBool ZrCommon_FfiReturnCodeType_Validate(
        const SZrFfiTypeContract *type) {
    if (type == ZR_NULL) {
        return ZR_FALSE;
    }
    return (TZrBool)(
            type->typeKind == ZR_FFI_CONTRACT_TYPE_BOOL ||
            (type->typeKind >= ZR_FFI_CONTRACT_TYPE_I8 &&
             type->typeKind <= ZR_FFI_CONTRACT_TYPE_U64) ||
            type->typeKind == ZR_FFI_CONTRACT_TYPE_USIZE ||
            type->typeKind == ZR_FFI_CONTRACT_TYPE_ISIZE ||
            type->typeKind == ZR_FFI_CONTRACT_TYPE_ENUM);
}

static inline TZrBool ZrCommon_NativeImportContract_Validate(
        const SZrNativeImportContract *contract) {
    TZrUInt64 expectedHash;

    if (contract == ZR_NULL ||
        contract->schemaVersion != ZR_FFI_CONTRACT_SCHEMA_VERSION ||
        memchr(contract->libraryLocator, '\0', sizeof(contract->libraryLocator)) == ZR_NULL ||
        memchr(contract->entryPoint, '\0', sizeof(contract->entryPoint)) == ZR_NULL ||
        memchr(contract->sourceMapping.document,
               '\0',
               sizeof(contract->sourceMapping.document)) == ZR_NULL ||
        contract->declaringModuleId == 0u ||
        contract->availability == 0u ||
        (contract->availability & ~ZR_FFI_CONTRACT_AVAILABILITY_ALL) != 0u ||
        (contract->requiredCapabilities &
         ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME) == 0u ||
        contract->sourceMapping.startOffset > contract->sourceMapping.endOffset ||
        contract->sourceMapping.startLine > contract->sourceMapping.endLine ||
        (TZrUInt32)contract->signature.abi > (TZrUInt32)ZR_FFI_CONTRACT_ABI_STDCALL ||
        (contract->signature.targetPointerSize != 4u &&
         contract->signature.targetPointerSize != 8u) ||
        (TZrUInt32)contract->signature.targetEndianness > (TZrUInt32)ZR_FFI_CONTRACT_ENDIAN_BIG ||
        memchr(contract->signature.targetTriple,
               '\0',
               sizeof(contract->signature.targetTriple)) == ZR_NULL ||
        strcmp(contract->signature.targetTriple,
               ZrCommon_FfiContract_GetHostTargetTriple()) != 0 ||
        contract->signature.targetAbiHash !=
                ZrCommon_FfiContract_ComputeTargetAbiHash(
                        contract->signature.abi,
                        contract->signature.targetPointerSize,
                        contract->signature.targetEndianness,
                        contract->signature.targetTriple) ||
        (TZrUInt32)contract->signature.charset > (TZrUInt32)ZR_FFI_CONTRACT_CHARSET_ANSI ||
        (TZrUInt32)contract->signature.errorPolicy > (TZrUInt32)ZR_FFI_CONTRACT_ERROR_THROWS ||
        contract->signature.errorPolicy == ZR_FFI_CONTRACT_ERROR_THROWS ||
        (TZrUInt32)contract->signature.cleanupPolicy > (TZrUInt32)ZR_FFI_CONTRACT_CLEANUP_REGISTERED ||
        contract->signature.cleanupPolicy == ZR_FFI_CONTRACT_CLEANUP_REGISTERED ||
        (TZrUInt32)contract->signature.callbackLifetime >
                (TZrUInt32)ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC ||
        (TZrUInt32)contract->signature.callbackThreadPolicy >
                (TZrUInt32)ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN ||
        (TZrUInt32)contract->signature.callbackExceptionPolicy >
                (TZrUInt32)ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ERROR_RESULT ||
        contract->signature.parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS ||
        contract->signature.aggregateFieldCount >
                ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS ||
        !ZrCommon_FfiTypeContract_Validate(
                 &contract->signature.returnType, ZR_TRUE) ||
        contract->signature.returnType.typeKind == ZR_FFI_CONTRACT_TYPE_UNION ||
        !ZrCommon_FfiTypeAggregateRange_Validate(
                &contract->signature,
                &contract->signature.returnType) ||
        (contract->signature.errorPolicy == ZR_FFI_CONTRACT_ERROR_RETURN_CODE &&
         !ZrCommon_FfiReturnCodeType_Validate(
                 &contract->signature.returnType))) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < contract->signature.parameterCount; index++) {
        const SZrFfiParameterContract *parameter = &contract->signature.parameters[index];

        if (!ZrCommon_FfiTypeContract_Validate(&parameter->type, ZR_FALSE) ||
            !ZrCommon_FfiTypeAggregateRange_Validate(
                     &contract->signature, &parameter->type) ||
            (TZrUInt32)parameter->direction > (TZrUInt32)ZR_FFI_CONTRACT_DIRECTION_OUT ||
            (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_UNION &&
             parameter->direction != ZR_FFI_CONTRACT_DIRECTION_IN) ||
            (TZrUInt32)parameter->marshalling > (TZrUInt32)ZR_FFI_CONTRACT_MARSHALLING_REGISTERED ||
            (TZrUInt32)parameter->ownership > (TZrUInt32)ZR_FFI_CONTRACT_OWNERSHIP_PINNED) {
            return ZR_FALSE;
        }
        if (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_CALLBACK &&
            (contract->signature.callbackLifetime ==
                     ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE ||
             contract->signature.callbackThreadPolicy ==
                     ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE ||
             contract->signature.callbackExceptionPolicy ==
                     ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE)) {
            return ZR_FALSE;
        }
    }
    expectedHash = ZrCommon_FfiSignatureContract_ComputeHash(&contract->signature);
    return (TZrBool)(expectedHash != 0u &&
                     expectedHash == contract->signature.signatureHash &&
                     expectedHash == contract->callableContractHash);
}

#endif // ZR_FFI_CONTRACT_H
