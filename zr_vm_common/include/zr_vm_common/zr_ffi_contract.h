#ifndef ZR_FFI_CONTRACT_H
#define ZR_FFI_CONTRACT_H

#include "zr_vm_common/zr_common_conf.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/** @brief parser、AOT 产物与 FFI 运行时共同识别的契约格式版本。 */
#define ZR_FFI_CONTRACT_SCHEMA_VERSION ((TZrUInt32)4u)
/** @brief 目标 ABI 指纹算法的版本；变更布局参与项时需同步生产端与校验端。 */
#define ZR_FFI_CONTRACT_ABI_MODEL_VERSION ((TZrUInt32)3u)
/** @brief 固定数组容量，也是生成端与校验端接受的参数上限。 */
#define ZR_FFI_CONTRACT_MAX_PARAMETERS ((TZrUInt32)32u)
/** @brief 单个签名可描述的聚合字段上限，生成端展开与验证端共用。 */
#define ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS ((TZrUInt32)64u)
/** @brief 单个函数可携带的导入契约上限；核心模块反序列化与运行时注册均据此拒绝超量数据。 */
#define ZR_FFI_CONTRACT_MAX_IMPORTS_PER_FUNCTION ((TZrUInt32)1024u)
/** @brief 定长契约文本容量；生产端写入时须保留终止符，校验端会检查。 */
#define ZR_FFI_CONTRACT_LIBRARY_CAPACITY ((TZrSize)512u)
#define ZR_FFI_CONTRACT_ENTRY_CAPACITY ((TZrSize)128u)
#define ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY ((TZrSize)512u)
#define ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY ((TZrSize)32u)
#define ZR_FFI_CONTRACT_TARGET_TRIPLE_CAPACITY ((TZrSize)64u)

/** @brief 导入声明允许的平台集合；运行时在真正解析符号前核对当前宿主。 */
#define ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS ((TZrUInt32)1u << 0u)
#define ZR_FFI_CONTRACT_AVAILABILITY_UNIX ((TZrUInt32)1u << 1u)
#define ZR_FFI_CONTRACT_AVAILABILITY_ALL                                      \
    (ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS | ZR_FFI_CONTRACT_AVAILABILITY_UNIX)

/* 与原生 provider 公开能力位一致；编译端要求它，运行时再核对授权。 */
#define ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME ((TZrUInt64)1u << 4u)

/** @brief 外部函数采用的调用约定；ABI 指纹和 libffi 调用共同消费。 */
typedef enum EZrFfiAbi {
    ZR_FFI_CONTRACT_ABI_SYSTEM = 0,
    ZR_FFI_CONTRACT_ABI_C,
    ZR_FFI_CONTRACT_ABI_STDCALL
} EZrFfiAbi;

/** @brief 外部参数的读写方向，与 callable 的传递形式共同校验。 */
typedef enum EZrFfiDirection {
    ZR_FFI_CONTRACT_DIRECTION_IN = 0,
    ZR_FFI_CONTRACT_DIRECTION_REF,
    ZR_FFI_CONTRACT_DIRECTION_OUT
} EZrFfiDirection;

/** @brief 语言调用点的 value/in/ref/out 形式，供编译端与契约校验端对齐。 */
typedef enum EZrFfiCallablePassingForm {
    ZR_FFI_CALLABLE_PASSING_VALUE = 0,
    ZR_FFI_CALLABLE_PASSING_IN,
    ZR_FFI_CALLABLE_PASSING_REF,
    ZR_FFI_CALLABLE_PASSING_REF_READONLY,
    ZR_FFI_CALLABLE_PASSING_OUT
} EZrFfiCallablePassingForm;

/** @brief 参数引用允许逃逸的最大作用域；阻止 native 调用越过语言所有权边界。 */
typedef enum EZrFfiCallableEscapeUpperBound {
    ZR_FFI_CALLABLE_ESCAPE_BLOCK = 0,
    ZR_FFI_CALLABLE_ESCAPE_FUNCTION,
    ZR_FFI_CALLABLE_ESCAPE_CALLER,
    ZR_FFI_CALLABLE_ESCAPE_HEAP_STATIC,
    ZR_FFI_CALLABLE_ESCAPE_UNKNOWN
} EZrFfiCallableEscapeUpperBound;

/** @brief 调用前的初始化要求，out 参数可在入口未初始化。 */
typedef enum EZrFfiCallableEntryInitialization {
    ZR_FFI_CALLABLE_ENTRY_INITIALIZED = 0,
    ZR_FFI_CALLABLE_ENTRY_UNINITIALIZED
} EZrFfiCallableEntryInitialization;

/** @brief 调用返回后的初始化保证，供 out 参数的静态检查使用。 */
typedef enum EZrFfiCallableExitInitialization {
    ZR_FFI_CALLABLE_EXIT_UNCHANGED = 0,
    ZR_FFI_CALLABLE_EXIT_DEFINITELY_INITIALIZED
} EZrFfiCallableExitInitialization;

/** @brief 源语言调用点必须出现的 ref/out 标记。 */
typedef enum EZrFfiCallableCallSiteMarker {
    ZR_FFI_CALLABLE_CALL_SITE_NONE = 0,
    ZR_FFI_CALLABLE_CALL_SITE_REF,
    ZR_FFI_CALLABLE_CALL_SITE_OUT
} EZrFfiCallableCallSiteMarker;

/** @brief 调用对接收者的可写性承诺，进入 callable 哈希。 */
typedef enum EZrFfiCallableReceiverEffect {
    ZR_FFI_CALLABLE_RECEIVER_NONE = 0,
    ZR_FFI_CALLABLE_RECEIVER_READONLY,
    ZR_FFI_CALLABLE_RECEIVER_MUTABLE
} EZrFfiCallableReceiverEffect;

/** @brief 可调用体副作用位，编译端生成后由校验端限制为已知集合。 */
#define ZR_FFI_CALLABLE_EFFECT_NONE ((TZrUInt32)0u)
#define ZR_FFI_CALLABLE_EFFECT_THROWS ((TZrUInt32)1u << 0u)
#define ZR_FFI_CALLABLE_EFFECT_ASYNC ((TZrUInt32)1u << 1u)
#define ZR_FFI_CALLABLE_EFFECT_GENERATOR ((TZrUInt32)1u << 2u)
#define ZR_FFI_CALLABLE_EFFECT_MASK                                      \
    (ZR_FFI_CALLABLE_EFFECT_THROWS | ZR_FFI_CALLABLE_EFFECT_ASYNC |      \
     ZR_FFI_CALLABLE_EFFECT_GENERATOR)

/** @brief 可投影到目标 C ABI 的类型分类；UNSUPPORTED 在契约校验中被拒绝。 */
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

/** @brief 值跨 VM/native 边界时的封送策略，运行时按类型与方向进一步判定。 */
typedef enum EZrFfiMarshallingKind {
    ZR_FFI_CONTRACT_MARSHALLING_DIRECT = 0,
    ZR_FFI_CONTRACT_MARSHALLING_PIN,
    ZR_FFI_CONTRACT_MARSHALLING_COPY,
    ZR_FFI_CONTRACT_MARSHALLING_REGISTERED
} EZrFfiMarshallingKind;

/** @brief 参数指针在调用前后的所有权说明，不能仅由 C 类型推导。 */
typedef enum EZrFfiParameterOwnership {
    ZR_FFI_CONTRACT_OWNERSHIP_BORROWED = 0,
    ZR_FFI_CONTRACT_OWNERSHIP_TRANSFER,
    ZR_FFI_CONTRACT_OWNERSHIP_SHARED,
    ZR_FFI_CONTRACT_OWNERSHIP_PINNED
} EZrFfiParameterOwnership;

/** @brief 文本参数的编码协议，编译端写入签名供运行时选择转换。 */
typedef enum EZrFfiCharset {
    ZR_FFI_CONTRACT_CHARSET_NONE = 0,
    ZR_FFI_CONTRACT_CHARSET_UTF8,
    ZR_FFI_CONTRACT_CHARSET_UTF16,
    ZR_FFI_CONTRACT_CHARSET_ANSI
} EZrFfiCharset;

/** @brief native 错误如何映射到语言层；当前校验明确拒绝 THROWS。 */
typedef enum EZrFfiErrorPolicy {
    ZR_FFI_CONTRACT_ERROR_NONE = 0,
    ZR_FFI_CONTRACT_ERROR_RETURN_CODE,
    ZR_FFI_CONTRACT_ERROR_LAST_ERROR,
    ZR_FFI_CONTRACT_ERROR_ERRNO,
    ZR_FFI_CONTRACT_ERROR_THROWS
} EZrFfiErrorPolicy;

/** @brief 外部资源由哪一侧清理；当前校验明确拒绝 REGISTERED。 */
typedef enum EZrFfiCleanupPolicy {
    ZR_FFI_CONTRACT_CLEANUP_NONE = 0,
    ZR_FFI_CONTRACT_CLEANUP_CALLER,
    ZR_FFI_CONTRACT_CLEANUP_CALLEE,
    ZR_FFI_CONTRACT_CLEANUP_REGISTERED
} EZrFfiCleanupPolicy;

/** @brief 回调指针允许存活的期限；回调类型要求同时给出线程与异常策略。 */
typedef enum EZrFfiCallbackLifetime {
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE = 0,
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL,
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_SCOPED,
    ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC
} EZrFfiCallbackLifetime;

/** @brief native 回调进入 VM 时允许的线程关系。 */
typedef enum EZrFfiCallbackThreadPolicy {
    ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE = 0,
    ZR_FFI_CONTRACT_CALLBACK_THREAD_CALLER,
    ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH,
    ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN
} EZrFfiCallbackThreadPolicy;

/** @brief 回调异常跨 native 栈时的处理契约。 */
typedef enum EZrFfiCallbackExceptionPolicy {
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE = 0,
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ABORT,
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_RETURN_DEFAULT,
    ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ERROR_RESULT
} EZrFfiCallbackExceptionPolicy;

/** @brief 目标字节序，参与 ABI 指纹以阻止跨目标误用契约。 */
typedef enum EZrFfiTargetEndianness {
    ZR_FFI_CONTRACT_ENDIAN_LITTLE = 0,
    ZR_FFI_CONTRACT_ENDIAN_BIG
} EZrFfiTargetEndianness;

/** @brief 类型投影属性；GC 引用、ref-like 与资源所有权位在直接 FFI 契约中不可接受。 */
#define ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE ((TZrUInt32)1u << 0u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_GC_REFERENCE ((TZrUInt32)1u << 1u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_REF_LIKE ((TZrUInt32)1u << 2u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_RESOURCE ((TZrUInt32)1u << 3u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_OWNER ((TZrUInt32)1u << 4u)
#define ZR_FFI_CONTRACT_TYPE_FLAG_MASK ((TZrUInt32)0x1fu)

/** @brief 聚合类型的字段布局；offset/size 按宿主 ABI 核对且 name 必须终止。 */
typedef struct SZrFfiAggregateFieldContract {
    TZrChar name[ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY];
    EZrFfiTypeKind typeKind;
    TZrUInt32 size;
    TZrUInt32 alignment;
    TZrUInt32 offset;
} SZrFfiAggregateFieldContract;

/** @brief 单个 native 类型的大小、对齐和稳定身份；聚合字段由下标范围引用。 */
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

/** @brief 语言参数与 native 参数之间的方向、封送及所有权约定。 */
typedef struct SZrFfiParameterContract {
    SZrFfiTypeContract type;
    EZrFfiDirection direction;
    EZrFfiMarshallingKind marshalling;
    EZrFfiParameterOwnership ownership;
    TZrBool isNullable;
    TZrUInt32 flags;
} SZrFfiParameterContract;

/**
 * @brief 完整 native 函数签名，含目标 ABI 指纹和参数/聚合固定容量数组。
 * @note parameterCount 与 aggregateFieldCount 必须在各自上限内，signatureHash 须由有效字段计算。
 */
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

/** @brief 源语言可调用参数的逃逸、初始化与调用点标记契约。 */
typedef struct SZrFfiCallableParameterContract {
    TZrUInt64 canonicalTypeHash;
    EZrFfiCallablePassingForm passingForm;
    EZrFfiCallableEscapeUpperBound escapeUpperBound;
    EZrFfiCallableEntryInitialization entryInitialization;
    EZrFfiCallableExitInitialization exitInitialization;
    TZrBool acceptsTemporary;
    EZrFfiCallableCallSiteMarker callSiteMarker;
} SZrFfiCallableParameterContract;

/** @brief 源语言调用形态的摘要，供 parser、FFI 运行时复核同一声明。 */
typedef struct SZrFfiCallableContract {
    TZrUInt32 parameterCount;
    SZrFfiCallableParameterContract parameters[ZR_FFI_CONTRACT_MAX_PARAMETERS];
    TZrUInt64 returnTypeHash;
    EZrFfiCallableReceiverEffect receiverEffect;
    TZrUInt32 effectFlags;
    TZrBool isVariadic;
    TZrUInt64 contractHash;
} SZrFfiCallableContract;

/** @brief 原始声明的源码位置，验证和诊断均以此定位问题。 */
typedef struct SZrFfiSourceMapping {
    TZrChar document[ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY];
    TZrUInt64 startOffset;
    TZrUInt64 endOffset;
    TZrInt32 startLine;
    TZrInt32 startColumn;
    TZrInt32 endLine;
    TZrInt32 endColumn;
} SZrFfiSourceMapping;

/**
 * @brief 由 parser 产生、AOT 或解释执行端消费的完整 native 导入契约。
 * @note 验证只确认格式和宿主 ABI 匹配；动态库可用性与符号解析由运行时另行处理。
 */
typedef struct SZrNativeImportContract {
    TZrUInt32 schemaVersion;
    TZrChar libraryLocator[ZR_FFI_CONTRACT_LIBRARY_CAPACITY];
    TZrChar entryPoint[ZR_FFI_CONTRACT_ENTRY_CAPACITY];
    TZrUInt64 symbolId;
    TZrUInt64 declaringModuleId;
    SZrFfiCallableContract callable;
    TZrUInt32 availability;
    TZrUInt64 requiredCapabilities;
    SZrFfiSourceMapping sourceMapping;
    SZrFfiSignatureContract signature;
} SZrNativeImportContract;

/** @brief 契约身份哈希的稳定种子；生成与加载两侧必须使用同一字节序列。 */
#define ZR_FFI_CONTRACT_FNV_OFFSET UINT64_C(1469598103934665603)
/** @brief 契约哈希的乘数，与种子一起固定产消两侧的身份算法。 */
#define ZR_FFI_CONTRACT_FNV_PRIME UINT64_C(1099511628211)

/** @brief 契约哈希的字节级公共基元；生产端和校验端应按相同顺序组合字段。 */
static inline TZrUInt64 ZrCommon_FfiContract_HashByte(
        TZrUInt64 hash,
        TZrUInt8 value) {
    return (hash ^ value) * ZR_FFI_CONTRACT_FNV_PRIME;
}

/** @brief 以固定字节顺序纳入 32 位值，避免主机字节序改变契约身份。 */
static inline TZrUInt64 ZrCommon_FfiContract_HashU32(
        TZrUInt64 hash,
        TZrUInt32 value) {
    for (TZrUInt32 index = 0u; index < 4u; index++) {
        hash = ZrCommon_FfiContract_HashByte(
                hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

/** @brief 以固定字节顺序纳入 64 位值，供类型与 callable 身份计算复用。 */
static inline TZrUInt64 ZrCommon_FfiContract_HashU64(
        TZrUInt64 hash,
        TZrUInt64 value) {
    for (TZrUInt32 index = 0u; index < 8u; index++) {
        hash = ZrCommon_FfiContract_HashByte(
                hash, (TZrUInt8)(value >> (index * 8u)));
    }
    return hash;
}

/**
 * @brief 计算源语言可调用契约的身份，parser 写入后由导入校验重新计算。
 * @pre contract 非空且 parameterCount 不超过固定容量；无效输入返回 0。
 */
static inline TZrUInt64 ZrCommon_FfiCallableContract_ComputeHash(
        const SZrFfiCallableContract *contract) {
    TZrUInt64 hash = ZR_FFI_CONTRACT_FNV_OFFSET;

    if (contract == ZR_NULL ||
        contract->parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS) {
        return 0u;
    }
    hash = ZrCommon_FfiContract_HashU32(hash, contract->parameterCount);
    for (TZrUInt32 index = 0u; index < contract->parameterCount; index++) {
        const SZrFfiCallableParameterContract *parameter =
                &contract->parameters[index];

        hash = ZrCommon_FfiContract_HashU64(hash, parameter->canonicalTypeHash);
        hash = ZrCommon_FfiContract_HashU32(hash, (TZrUInt32)parameter->passingForm);
        hash = ZrCommon_FfiContract_HashU32(
                hash, (TZrUInt32)parameter->escapeUpperBound);
        hash = ZrCommon_FfiContract_HashU32(
                hash, (TZrUInt32)parameter->entryInitialization);
        hash = ZrCommon_FfiContract_HashU32(
                hash, (TZrUInt32)parameter->exitInitialization);
        hash = ZrCommon_FfiContract_HashU32(
                hash, parameter->acceptsTemporary ? 1u : 0u);
        hash = ZrCommon_FfiContract_HashU32(
                hash, (TZrUInt32)parameter->callSiteMarker);
    }
    hash = ZrCommon_FfiContract_HashU64(hash, contract->returnTypeHash);
    hash = ZrCommon_FfiContract_HashU32(
            hash, (TZrUInt32)contract->receiverEffect);
    hash = ZrCommon_FfiContract_HashU32(hash, contract->effectFlags);
    return ZrCommon_FfiContract_HashU32(
            hash, contract->isVariadic ? 1u : 0u);
}

/** @brief 返回当前编译宿主的稳定 target triple，供导入契约绑定目标平台。 */
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

/** @brief 将以 NUL 结束的目标文本纳入哈希；调用方须先保证字符串可终止。 */
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

/** @brief 将类型的 ABI 可观察字段纳入签名指纹，聚合字段另由签名遍历。 */
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

/**
 * @brief 为 native 导入计算宿主 ABI 指纹，阻止用另一平台或 C 数据模型的契约调用。
 * @pre targetTriple 为可终止的目标字符串；调用端与运行时需以同一目标工具链编译。
 */
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

/**
 * @brief 计算 native 签名指纹，parser 写入后由校验器检测布局或策略是否被改动。
 * @pre 各计数不超过固定容量，targetTriple 含 NUL；无效计数返回 0。
 */
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
    /* TODO: 此独立哈希入口只检查计数，不先检查 targetTriple 终止符；
     * NativeImportContract_Validate 会先检查，但直接调用者须自行保证字符串有效。 */
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

/** @brief 过滤不能直接投影到 native ABI 的类型和未受支持的所有权位。 */
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

/** @brief 核对类型引用的聚合字段切片与每个字段的基本布局边界。 */
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

/** @brief 限定 return-code 错误策略的返回类型为可判定的整数类。 */
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

/** @brief 验证 value/in/ref/out 对逃逸、初始化与调用标记的组合约束。 */
static inline TZrBool ZrCommon_FfiCallableParameterContract_Validate(
        const SZrFfiCallableParameterContract *parameter) {
    if (parameter == ZR_NULL || parameter->canonicalTypeHash == 0u ||
        (TZrUInt32)parameter->passingForm >
                (TZrUInt32)ZR_FFI_CALLABLE_PASSING_OUT ||
        (TZrUInt32)parameter->escapeUpperBound >
                (TZrUInt32)ZR_FFI_CALLABLE_ESCAPE_UNKNOWN ||
        (TZrUInt32)parameter->entryInitialization >
                (TZrUInt32)ZR_FFI_CALLABLE_ENTRY_UNINITIALIZED ||
        (TZrUInt32)parameter->exitInitialization >
                (TZrUInt32)ZR_FFI_CALLABLE_EXIT_DEFINITELY_INITIALIZED ||
        (TZrUInt32)parameter->callSiteMarker >
                (TZrUInt32)ZR_FFI_CALLABLE_CALL_SITE_OUT) {
        return ZR_FALSE;
    }
    switch (parameter->passingForm) {
        case ZR_FFI_CALLABLE_PASSING_VALUE:
            return (TZrBool)(
                    parameter->escapeUpperBound ==
                            ZR_FFI_CALLABLE_ESCAPE_FUNCTION &&
                    parameter->entryInitialization ==
                            ZR_FFI_CALLABLE_ENTRY_INITIALIZED &&
                    parameter->exitInitialization ==
                            ZR_FFI_CALLABLE_EXIT_UNCHANGED &&
                    parameter->acceptsTemporary &&
                    parameter->callSiteMarker ==
                            ZR_FFI_CALLABLE_CALL_SITE_NONE);
        case ZR_FFI_CALLABLE_PASSING_IN:
            return (TZrBool)(
                    parameter->escapeUpperBound ==
                            ZR_FFI_CALLABLE_ESCAPE_FUNCTION &&
                    parameter->entryInitialization ==
                            ZR_FFI_CALLABLE_ENTRY_INITIALIZED &&
                    parameter->exitInitialization ==
                            ZR_FFI_CALLABLE_EXIT_UNCHANGED &&
                    parameter->acceptsTemporary &&
                    parameter->callSiteMarker ==
                            ZR_FFI_CALLABLE_CALL_SITE_NONE);
        case ZR_FFI_CALLABLE_PASSING_REF:
        case ZR_FFI_CALLABLE_PASSING_REF_READONLY:
            return (TZrBool)(
                    (parameter->escapeUpperBound ==
                             ZR_FFI_CALLABLE_ESCAPE_FUNCTION ||
                     parameter->escapeUpperBound ==
                             ZR_FFI_CALLABLE_ESCAPE_CALLER) &&
                    parameter->entryInitialization ==
                            ZR_FFI_CALLABLE_ENTRY_INITIALIZED &&
                    parameter->exitInitialization ==
                            ZR_FFI_CALLABLE_EXIT_UNCHANGED &&
                    !parameter->acceptsTemporary &&
                    parameter->callSiteMarker ==
                            ZR_FFI_CALLABLE_CALL_SITE_REF);
        case ZR_FFI_CALLABLE_PASSING_OUT:
            return (TZrBool)(
                    parameter->escapeUpperBound ==
                            ZR_FFI_CALLABLE_ESCAPE_FUNCTION &&
                    parameter->entryInitialization ==
                            ZR_FFI_CALLABLE_ENTRY_UNINITIALIZED &&
                    parameter->exitInitialization ==
                            ZR_FFI_CALLABLE_EXIT_DEFINITELY_INITIALIZED &&
                    !parameter->acceptsTemporary &&
                    parameter->callSiteMarker ==
                            ZR_FFI_CALLABLE_CALL_SITE_OUT);
        default:
            return ZR_FALSE;
    }
}

/** @brief 复核源语言 callable 形态及其哈希，防止与 native 签名仅按位置硬绑定。 */
static inline TZrBool ZrCommon_FfiCallableContract_Validate(
        const SZrFfiCallableContract *contract) {
    if (contract == ZR_NULL ||
        contract->parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS ||
        contract->returnTypeHash == 0u ||
        (TZrUInt32)contract->receiverEffect >
                (TZrUInt32)ZR_FFI_CALLABLE_RECEIVER_MUTABLE ||
        (contract->effectFlags & ~ZR_FFI_CALLABLE_EFFECT_MASK) != 0u) {
        return ZR_FALSE;
    }
    for (TZrUInt32 index = 0u; index < contract->parameterCount; index++) {
        if (!ZrCommon_FfiCallableParameterContract_Validate(
                    &contract->parameters[index])) {
            return ZR_FALSE;
        }
    }
    return (TZrBool)(
            contract->contractHash != 0u &&
            contract->contractHash ==
                    ZrCommon_FfiCallableContract_ComputeHash(contract));
}

/**
 * @brief 在解析动态库符号前复核导入契约格式、宿主 ABI、callable 和签名。
 * @return 真表示结构适合交给运行时进一步解析；并不保证库、符号或调用一定成功。
 */
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
        !ZrCommon_FfiCallableContract_Validate(&contract->callable) ||
        contract->callable.parameterCount !=
                contract->signature.parameterCount ||
        contract->callable.isVariadic != contract->signature.isVariadic ||
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
        const SZrFfiCallableParameterContract *callableParameter =
                &contract->callable.parameters[index];

        if (!ZrCommon_FfiTypeContract_Validate(&parameter->type, ZR_FALSE) ||
            !ZrCommon_FfiTypeAggregateRange_Validate(
                     &contract->signature, &parameter->type) ||
            (TZrUInt32)parameter->direction > (TZrUInt32)ZR_FFI_CONTRACT_DIRECTION_OUT ||
            (parameter->type.typeKind == ZR_FFI_CONTRACT_TYPE_UNION &&
             parameter->direction != ZR_FFI_CONTRACT_DIRECTION_IN) ||
            (TZrUInt32)parameter->marshalling > (TZrUInt32)ZR_FFI_CONTRACT_MARSHALLING_REGISTERED ||
            (TZrUInt32)parameter->ownership > (TZrUInt32)ZR_FFI_CONTRACT_OWNERSHIP_PINNED ||
            (parameter->direction == ZR_FFI_CONTRACT_DIRECTION_IN &&
             callableParameter->passingForm != ZR_FFI_CALLABLE_PASSING_VALUE &&
             callableParameter->passingForm != ZR_FFI_CALLABLE_PASSING_IN) ||
            (parameter->direction == ZR_FFI_CONTRACT_DIRECTION_REF &&
             callableParameter->passingForm != ZR_FFI_CALLABLE_PASSING_REF &&
             callableParameter->passingForm !=
                     ZR_FFI_CALLABLE_PASSING_REF_READONLY) ||
            (parameter->direction == ZR_FFI_CONTRACT_DIRECTION_OUT &&
             callableParameter->passingForm != ZR_FFI_CALLABLE_PASSING_OUT)) {
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
                     expectedHash == contract->signature.signatureHash);
}

#endif // ZR_FFI_CONTRACT_H
