//
// 编译产物、动态加载器与元数据运行时共享此二进制布局；字段顺序变更需同步 ABI 版本。
//

#ifndef ZR_VM_COMMON_ZR_AOT_ABI_H
#define ZR_VM_COMMON_ZR_AOT_ABI_H

#include "zr_vm_common/zr_common_conf.h"

struct SZrFunction;
struct SZrState;
struct SZrTypeValue;
struct SZrAotMethodInfo;
struct SZrAotGcRootMap;
struct SZrTypeLayout;

/** @brief AOT 描述符的精确匹配版本，加载器据此拒绝不兼容的动态库。 */
#define ZR_VM_AOT_ABI_VERSION 17u

/** @brief 产物后端身份；加载器用它核对请求的是 C 还是 LLVM 动态库。 */
typedef enum EZrAotBackendKind {
    ZR_AOT_BACKEND_KIND_NONE = 0,
    ZR_AOT_BACKEND_KIND_C = 1,
    ZR_AOT_BACKEND_KIND_LLVM = 2
} EZrAotBackendKind;

/** @brief 生成输入的来源，供加载器决定应比对源文件还是二进制模块哈希。 */
typedef enum EZrAotInputKind {
    ZR_AOT_INPUT_KIND_NONE = 0,
    ZR_AOT_INPUT_KIND_SOURCE = 1,
    ZR_AOT_INPUT_KIND_BINARY = 2
} EZrAotInputKind;

/** @brief 反射元数据的生成级别；AOT 生成端据此选择写入的方法描述信息。 */
typedef enum EZrAotReflectionMetadataLevel {
    ZR_AOT_REFLECTION_METADATA_NONE = 0,
    ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING = 1,
    ZR_AOT_REFLECTION_METADATA_DESCRIPTION = 2
} EZrAotReflectionMetadataLevel;

/** @brief 生成函数的统一入口，加载器与核心模块按函数索引持有该表。 */
typedef TZrInt64 (*FZrAotEntryThunk)(struct SZrState *state);

/** @brief 通过方法元数据调用生成函数的桥接入口，参数与返回值由调用方提供。
 * @note 此 ABI 不传参数数量且返回 void；调用方须提供匹配签名的存储，
 * 调用入口返回不等于取得独立的执行成功状态。带数量的反射封装也可能在执行后拒绝返回标签。
 */
typedef void (*FZrAotReflectionInvoker)(struct SZrState *state,
                                        FZrAotEntryThunk target,
                                        const struct SZrAotMethodInfo *method,
                                        struct SZrTypeValue *self,
                                        struct SZrTypeValue *args,
                                        struct SZrTypeValue *outReturn);

/** @brief 闭合泛型字典中待解析项的类别，决定 resolved value 的有效成员。 */
typedef enum EZrAotGenericSlotKind {
    ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT = 0,
    ZR_AOT_GENERIC_SLOT_PROTOTYPE = 1,
    ZR_AOT_GENERIC_SLOT_METHOD = 2,
    ZR_AOT_GENERIC_SLOT_BOX_TYPE = 3,
    ZR_AOT_GENERIC_SLOT_SIZEOF = 4
} EZrAotGenericSlotKind;

/** @brief 泛型槽位的运行时结果；读取前须按对应槽位 kind 判别成员。 */
typedef union SZrAotGenericResolvedValue {
    const struct SZrTypeLayout *typeLayout;
    const void *prototype;
    FZrAotEntryThunk method;
    TZrSize sizeOfValue;
    const void *pointer;
} SZrAotGenericResolvedValue;

/** @brief 生成端发布的泛型需求；静态入口可减少运行时重新解析。 */
typedef struct SZrAotGenericSlot {
    TZrUInt32 kind;
    TZrUInt32 typeLayoutId;
    TZrUInt32 metadataToken;
    TZrUInt32 methodIndex;
    TZrUInt32 flags;
    const TZrChar *debugName;
    const struct SZrTypeLayout *staticTypeLayout;
    FZrAotEntryThunk staticMethod;
} SZrAotGenericSlot;

/** @brief 与需求槽位逐项对应的缓存；isResolved 标明联合体是否可读。 */
typedef struct SZrAotGenericResolvedSlot {
    TZrUInt32 kind;
    TZrUInt8 isResolved;
    TZrUInt8 reserved0;
    TZrUInt8 reserved1;
    TZrUInt8 reserved2;
    SZrAotGenericResolvedValue value;
} SZrAotGenericResolvedSlot;

/** @brief 单个生成方法的泛型解析上下文；两个数组均按 slotCount 对齐。
 * @note resolvedSlots 是借用的可写缓存，不因字典为 const 就成为不可变数据。
 * TODO: 当前生成端使用模块静态缓存；消费者命中时未区分 metadataRuntime，
 * 多运行时复用与并发写的隔离需以 GenericSlot_* 调用链核对，不能据此承诺线程安全。
 */
typedef struct SZrAotGenericDictionary {
    TZrUInt32 slotCount;
    const SZrAotGenericSlot *slots;
    SZrAotGenericResolvedSlot *resolvedSlots;
} SZrAotGenericDictionary;

/** @brief 反射调用使用的参数传递契约，区分值、借用、可写引用与输出。 */
typedef enum EZrAotParameterPassingMode {
    ZR_AOT_PARAMETER_PASSING_UNKNOWN = 0,
    ZR_AOT_PARAMETER_PASSING_VALUE = 1,
    ZR_AOT_PARAMETER_PASSING_IN = 2,
    ZR_AOT_PARAMETER_PASSING_REF = 3,
    ZR_AOT_PARAMETER_PASSING_REF_READONLY = 4,
    ZR_AOT_PARAMETER_PASSING_SCOPED_REF = 5,
    ZR_AOT_PARAMETER_PASSING_SCOPED_REF_READONLY = 6,
    ZR_AOT_PARAMETER_PASSING_OUT = 7
} EZrAotParameterPassingMode;

/** @brief AOT 方法签名中的单个类型描述；布局和所有权字段供反射边界校验。 */
typedef struct SZrAotSignatureType {
    TZrUInt16 baseType;
    TZrUInt16 staticCType;
    TZrUInt32 staticCTypeId;
    TZrUInt32 ownershipQualifier;
    TZrUInt16 elementBaseType;
    TZrUInt8 isNullable;
    TZrUInt8 isArray;
    TZrUInt8 passingMode;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrAotSignatureType;

/** @brief 方法返回值及参数数组；parameterTypes 长度由 parameterCount 决定。 */
typedef struct SZrAotSignature {
    TZrUInt32 parameterCount;
    const SZrAotSignatureType *returnType;
    const SZrAotSignatureType *parameterTypes;
    TZrUInt8 hasReturnValue;
    TZrUInt8 hasVarArgs;
} SZrAotSignature;

/** @brief 生成帧中 GC 根的寻址方式；扫描器据此解释偏移或局部地址。 */
typedef enum EZrAotGcRootLocationKind {
    ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET = 0,
    ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS = 1
} EZrAotGcRootLocationKind;

/** @brief 一个生成帧 GC 根的位置与类型布局；字段偏移必须与目标布局一致。 */
typedef struct SZrAotGcRootSlot {
    TZrUInt32 stackSlot;
    TZrUInt32 frameByteOffset;
    TZrUInt32 typeLayoutId;
    TZrUInt32 fieldByteOffset;
    TZrUInt8 locationKind;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrAotGcRootSlot;

/** @brief 生成方法提供给运行时扫描器的根数组及其长度。 */
typedef struct SZrAotGcRootMap {
    TZrUInt32 rootCount;
    const SZrAotGcRootSlot *roots;
} SZrAotGcRootMap;

/**
 * @brief 函数索引关联的帧、签名、GC 根与反射桥接信息。
 * @note 元数据运行时保留其中的表指针；所属 AOT 动态库须在模块使用期间保持加载。
 */
typedef struct SZrAotMethodInfo {
    TZrUInt32 functionIndex;
    const struct SZrFunction *metadataFunction;
    TZrUInt32 registerFrameBytes;
    const struct SZrAotGcRootMap *gcRootMap;
    const SZrAotSignature *signature;
    const SZrAotGenericDictionary *genericDictionary;
    FZrAotReflectionInvoker invoker;
    /* TODO: 生成端写入这两个策略字段，但当前主运行时未见读取点；
     * 核查反射能力是否另有约束，再决定它们是否仍属有效 ABI。 */
    TZrUInt8 observationPolicy;
    TZrUInt8 reflectionMetadataLevel;
    TZrUInt8 reserved0;
    TZrUInt8 reserved1;
} SZrAotMethodInfo;

/** @brief 类型布局中需扫描的引用字段偏移表，由 GC 与元数据注册共同消费。 */
typedef struct SZrAotGcDescriptor {
    TZrUInt32 typeLayoutId;
    TZrUInt32 gcFieldCount;
    const TZrUInt32 *gcFieldOffsets;
} SZrAotGcDescriptor;

/** @brief 源成员 token 到生成产物 token 的映射，模块挂载时用于修正导出元数据。 */
typedef struct SZrAotMemberTokenRemap {
    TZrUInt32 sourceToken;
    TZrUInt32 targetToken;
} SZrAotMemberTokenRemap;

/** @brief manifest 导出行的类别与有效 token 标志，供加载器和元数据查询共同解释。 */
#define ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_TYPE 1u
#define ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_METHOD 2u
#define ZR_AOT_MANIFEST_EXPORT_ENTRY_KIND_FIELD 3u

#define ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN 1u
#define ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN 2u
#define ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_KNOWN_MASK \
    (ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_TYPE_TOKEN | ZR_AOT_MANIFEST_EXPORT_ENTRY_FLAG_HAS_MEMBER_TOKEN)

/** @brief 可由运行时反射查找的导出项；flags 决定 typeToken/memberToken 哪个有效。 */
typedef struct SZrAotManifestExportEntry {
    TZrUInt32 kind;
    TZrUInt32 flags;
    const TZrChar *target;
    TZrUInt32 typeToken;
    TZrUInt32 memberToken;
} SZrAotManifestExportEntry;

/** @brief 单个生成函数在 nativeImportContracts 表中的连续切片。 */
typedef struct SZrAotNativeImportRange {
    TZrUInt32 contractStart;
    TZrUInt32 contractCount;
} SZrAotNativeImportRange;

/**
 * @brief 将生成代码表挂到核心模块的只读注册视图。
 * @note 表指针与外层 ZrAotCompiledModule 的对应成员须一致；加载器逐项核对后才挂载。
 * 该核对覆盖表形状及部分行契约，不替代签名、GC 根和所有指向数据的语义校验。
 * C 生成端按原 functionIndex 铺开 thunk、methodInfo 和 token 表；裁剪空洞保留为 NULL/0，
 * 因而对应计数表示索引空间长度，不能用非空方法的数量替代。
 */
typedef struct SZrAotCodeRegistration {
    TZrUInt32 functionCount;
    const FZrAotEntryThunk *functionPointers;
    const SZrAotMethodInfo *const *methodInfos;
    TZrUInt32 methodInfoCount;
    const TZrUInt32 *methodTokens;
    TZrUInt32 methodTokenCount;
    const SZrAotMemberTokenRemap *memberTokenRemaps;
    TZrUInt32 memberTokenRemapCount;
    const SZrAotManifestExportEntry *manifestExports;
    TZrUInt32 manifestExportCount;
    const FZrAotReflectionInvoker *invokers;
    TZrUInt32 invokerCount;
    const struct SZrTypeLayout *const *typeLayouts;
    TZrUInt32 typeLayoutCount;
    const TZrUInt32 *typeLayoutTokens;
    TZrUInt32 typeLayoutTokenCount;
    const SZrAotGcDescriptor *const *gcDescriptors;
    TZrUInt32 gcDescriptorCount;
    const struct SZrNativeImportContract *nativeImportContracts;
    TZrUInt32 nativeImportContractCount;
    const SZrAotNativeImportRange *nativeImportRanges;
    TZrUInt32 nativeImportRangeCount;
    const TZrByte *callBindingRows;
    TZrUInt32 callBindingRowCount;
    TZrUInt32 callBindingRowSize;
    const TZrUInt32 *callBindingTargetFunctionIndices;
} SZrAotCodeRegistration;

/**
 * @brief AOT 动态库导出的根描述符，连接文件身份、嵌入模块与代码注册表。
 * @note 加载器先检查 ABI、模块名、各表的指针与计数，再使用元数据；所有表需随动态库保持有效。
 */
typedef struct ZrAotCompiledModule {
    TZrUInt32 abiVersion;
    TZrUInt32 backendKind;
    const TZrChar *moduleName;
    TZrUInt32 inputKind;
    const TZrChar *inputHash;
    const TZrChar *const *runtimeContracts;
    const TZrByte *embeddedModuleBlob;
    TZrSize embeddedModuleBlobLength;
    const FZrAotEntryThunk *functionThunks;
    TZrUInt32 functionThunkCount;
    FZrAotEntryThunk entryThunk;
    const SZrAotMethodInfo *const *methodInfos;
    TZrUInt32 methodInfoCount;
    const TZrUInt32 *methodTokens;
    TZrUInt32 methodTokenCount;
    const SZrAotMemberTokenRemap *memberTokenRemaps;
    TZrUInt32 memberTokenRemapCount;
    const SZrAotManifestExportEntry *manifestExports;
    TZrUInt32 manifestExportCount;
    const struct SZrTypeLayout *const *typeLayouts;
    TZrUInt32 typeLayoutCount;
    const TZrUInt32 *typeLayoutTokens;
    TZrUInt32 typeLayoutTokenCount;
    const SZrAotGcDescriptor *const *gcDescriptors;
    TZrUInt32 gcDescriptorCount;
    const struct SZrNativeImportContract *nativeImportContracts;
    TZrUInt32 nativeImportContractCount;
    const SZrAotNativeImportRange *nativeImportRanges;
    TZrUInt32 nativeImportRangeCount;
    const SZrAotCodeRegistration *codeRegistration;
    const TZrByte *callBindingRows;
    TZrUInt32 callBindingRowCount;
    TZrUInt32 callBindingRowSize;
    const TZrUInt32 *callBindingTargetFunctionIndices;
} ZrAotCompiledModule;

/** @brief 动态加载器查找的符号签名，返回该库内静态存活的模块描述符。 */
typedef const ZrAotCompiledModule *(*FZrVmGetAotCompiledModule)(void);

/** @brief 使描述符查找入口在动态库外可见；生成端需用于 ZrVm_GetAotCompiledModule。 */
#if defined(ZR_PLATFORM_WIN)
#define ZR_VM_AOT_EXPORT __declspec(dllexport)
#else
#define ZR_VM_AOT_EXPORT __attribute__((visibility("default")))
#endif

#endif
