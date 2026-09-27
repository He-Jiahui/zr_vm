#ifndef ZR_VM_CORE_CALL_BINDING_H
#define ZR_VM_CORE_CALL_BINDING_H

#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/value.h"
#include "zr_vm_common/zr_aot_abi.h"

struct SZrFunction;

/** 持久化契约与重定位节的版本、空槽及定长编码尺寸；写入端和读取端必须使用同一组值。 */
#define ZR_CALL_BINDING_SCHEMA_VERSION ((TZrUInt32)1u)
#define ZR_CALL_BINDING_SLOT_NONE ((TZrUInt32)0xffffffffu)
#define ZR_CALL_BINDING_CONTRACT_ENCODED_SIZE ((TZrUInt32)64u)
/* TODO: 当前序列化调用点均直接使用固定偏移或 SECTION_ROW_SIZE，未引用此常量；
 * 核对 artifact 与 .zro 两种行格式后决定统一使用还是移除。 */
#define ZR_CALL_BINDING_LOCATION_ENCODED_SIZE ((TZrUInt32)16u)
#define ZR_CALL_BINDING_SECTION_MAGIC ((TZrUInt32)0x444e4243u)
#define ZR_CALL_BINDING_SECTION_ROW_SIZE ((TZrUInt32)84u)

/** 持久化目标来源；链接器据此决定在常量池、本模块、导入模块或 AOT 注册表中重建目标。 */
typedef enum EZrCallBindingRelocationKind {
    ZR_CALL_BINDING_RELOCATION_NONE = 0,
    ZR_CALL_BINDING_RELOCATION_CONSTANT = 1,
    ZR_CALL_BINDING_RELOCATION_MODULE = 2,
    ZR_CALL_BINDING_RELOCATION_AOT = 3,
    ZR_CALL_BINDING_RELOCATION_VM_MODULE = 4
} EZrCallBindingRelocationKind;

/**
 * 仅记录可重定位坐标，不包含进程内指针。AOT 常量投影可沿 ownerFunction
 * 使用非零 ownerDepth；core 本地链接当前只接受零，flags 当前必须为零。
 */
typedef struct SZrCallBindingLocation {
    TZrUInt32 kind;
    TZrUInt32 targetIndex;
    TZrUInt32 ownerDepth;
    TZrUInt32 flags;
} SZrCallBindingLocation;

/** 调用契约形态；虚调用与接口调用保留槽位，类型化函数值只绑定签名而不固定目标。 */
typedef enum EZrCallBindingKind {
    ZR_CALL_BINDING_NONE = 0,
    ZR_CALL_BINDING_DIRECT = 1,
    ZR_CALL_BINDING_VIRTUAL = 2,
    ZR_CALL_BINDING_INTERFACE = 3,
    ZR_CALL_BINDING_TYPED_FUNCTION = 4
} EZrCallBindingKind;

/** 区分调用、属性访问和元方法，防止同一 token 被错误的指令族消费。 */
typedef enum EZrCallBindingOperation {
    ZR_CALL_BINDING_OPERATION_CALL = 0,
    ZR_CALL_BINDING_OPERATION_GET = 1,
    ZR_CALL_BINDING_OPERATION_SET = 2,
    ZR_CALL_BINDING_OPERATION_META = 3
} EZrCallBindingOperation;

/**
 * @brief 编译器、二进制 IO 与产物投影共享的持久化调用契约。
 * @note 写入时逐字段编码；不得序列化包含运行时目标的 SZrCallBinding。
 * ownerTypeToken 非零时 layoutVersion 与 layoutHash 必须同时有效；VIRTUAL 与 INTERFACE
 * 必须有 dispatchSlot，DIRECT 与 TYPED_FUNCTION 不占槽。
 */
typedef struct SZrCallBindingContract {
    TZrUInt32 bindingKind;
    TZrMetadataToken targetMetadataToken;
    TZrMetadataToken signatureToken;
    TZrMetadataToken ownerTypeToken;
    TZrUInt64 signatureHash;
    TZrUInt64 moduleSignatureHash;
    TZrUInt32 layoutVersion;
    TZrUInt32 dispatchSlot;
    TZrUInt64 layoutHash;
    TZrUInt32 operation;
    TZrUInt32 reserved0;
    TZrUInt64 reserved1;
} SZrCallBindingContract;

/** 已解析目标的进程内表示，不进入持久化契约。 */
typedef enum EZrCallBindingTargetKind {
    ZR_CALL_BINDING_TARGET_NONE = 0,
    ZR_CALL_BINDING_TARGET_VM = 1,
    ZR_CALL_BINDING_TARGET_NATIVE = 2,
    ZR_CALL_BINDING_TARGET_AOT = 3
} EZrCallBindingTargetKind;

/**
 * @brief 链接或运行时选择的目标见证，供 VM、native 与 AOT 分派使用。
 * @note 目标函数、闭包和拥有者原型依赖函数缓存的 GC 扫描及写屏障；代际变更后必须失效。
 */
typedef struct SZrCallBindingTarget {
    TZrUInt32 targetKind;
    TZrUInt32 dispatchSlotCount;
    TZrUInt64 ownerLayoutGeneration;
    TZrUInt64 targetGeneration;
    union {
        struct { struct SZrFunction *function; } vm;
        struct { FZrNativeFunction function; } native;
        struct {
            FZrAotEntryThunk thunk;
            const struct SZrAotMethodInfo *methodInfo;
            FZrAotReflectionInvoker invoker;
        } aot;
    };
    /* callableObject 是 GC 追踪的借用引用；闭包上下文由原值持有，不能在此释放或持久化。 */
    struct SZrRawObject *callableObject;
} SZrCallBindingTarget;

/** 一个调用点的持久化契约及可丢弃的运行时目标；失效仅清除后两者中的代际和目标。 */
typedef struct SZrCallBinding {
    SZrCallBindingContract contract;
    TZrUInt64 generation;
    SZrCallBindingTarget target;
} SZrCallBinding;

/** 链接器提供的候选目标；Resolve 在唯一 token 命中且完整契约一致后才发布。 */
typedef struct SZrCallBindingCandidate {
    SZrCallBindingContract contract;
    TZrUInt64 generation;
    SZrCallBindingTarget target;
} SZrCallBindingCandidate;

/** 链接和调用前验证的结构化结果，供上层生成错误而非退回按名称寻找目标。 */
typedef enum EZrCallBindingStatus {
    ZR_CALL_BINDING_OK = 0,
    ZR_CALL_BINDING_INVALID_ARGUMENT,
    ZR_CALL_BINDING_MISSING_CONTRACT,
    ZR_CALL_BINDING_INVALID_TOKEN,
    ZR_CALL_BINDING_TARGET_NOT_FOUND,
    ZR_CALL_BINDING_AMBIGUOUS_TARGET,
    ZR_CALL_BINDING_SIGNATURE_MISMATCH,
    ZR_CALL_BINDING_MODULE_MISMATCH,
    ZR_CALL_BINDING_LAYOUT_MISMATCH,
    ZR_CALL_BINDING_INVALID_SLOT,
    ZR_CALL_BINDING_STALE_GENERATION,
    ZR_CALL_BINDING_TARGET_KIND_MISMATCH,
    ZR_CALL_BINDING_INVALID_RELOCATION
} EZrCallBindingStatus;

/**
 * @brief 最近一次绑定失败的结构化位置和期望/实际值。
 * @note 不同检查只填充适用字段；调用方应结合 status 解读 expected 与 actual。
 */
typedef struct SZrCallBindingDiagnostic {
    EZrCallBindingStatus status;
    TZrMetadataToken targetMetadataToken;
    TZrUInt32 instructionIndex;
    TZrUInt32 candidateIndex;
    TZrUInt64 expected;
    TZrUInt64 actual;
} SZrCallBindingDiagnostic;

/** @brief 检查独立契约的 token、布局、槽位及保留字段，供序列化和链接入口共用。 */
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_CheckContract(
        const SZrCallBindingContract *contract, SZrCallBindingDiagnostic *diagnostic);
/** @brief 校验编译期预期与当前 provider 候选是否完全一致，并分类报告漂移字段。 */
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_CompareContracts(
        const SZrCallBindingContract *expected, const SZrCallBindingContract *actual,
        SZrCallBindingDiagnostic *diagnostic);
/**
 * @brief 从候选集中唯一确定目标并安装到调用点；失败时目标保持失效。
 * @pre binding 可写；candidates 在 count 非零时可读；generation 是所属函数当前非零代际。
 */
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_Resolve(
        const SZrCallBindingContract *expected, const SZrCallBindingCandidate *candidates,
        TZrUInt32 count, TZrUInt64 generation, SZrCallBinding *binding,
        SZrCallBindingDiagnostic *diagnostic);
/** @brief 调用前检查契约、所属函数代际与目标代际；失败会清除运行时见证。 */
ZR_CORE_API EZrCallBindingStatus ZrCore_CallBinding_Validate(
        SZrCallBinding *binding, TZrUInt64 generation, SZrCallBindingDiagnostic *diagnostic);
/** @brief 只丢弃运行时目标和代际，保留可供重新链接的持久化契约。 */
ZR_CORE_API void ZrCore_CallBinding_Invalidate(SZrCallBinding *binding);
/** @brief 模块移除或重载时递增整张函数图的代际并失效目标；指令映射由下次链接重建。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_AdvanceGeneration(struct SZrFunction *function);
/** 函数图访问回调；返回 false 会停止遍历并向调用方传递失败。 */
typedef TZrBool (*FZrCallBindingFunctionVisitor)(struct SZrFunction *function, void *context);
/** @brief 先收集子函数与函数常量形成的完整去重图，再逐个调用 visitor。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_VisitFunctions(
        struct SZrFunction *root, FZrCallBindingFunctionVisitor visitor, void *context);
/** @brief 将结构化状态映射为运行错误使用的稳定短名称。 */
ZR_CORE_API const char *ZrCore_CallBinding_StatusName(EZrCallBindingStatus status);
/** @brief 按固定宽度逐字段编码已验证的契约；输出缓冲区必须恰好为 64 字节。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_EncodeContract(
        const SZrCallBindingContract *contract, TZrByte *bytes, TZrSize length);
/** @brief 解码并验证持久化契约；失败时将输出契约清零。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_DecodeContract(
        const TZrByte *bytes, TZrSize length, SZrCallBindingContract *contract);
/** @brief 由结构化参数、返回值与传参角色计算跨编译器和运行时共用的签名哈希；信息不足返回零。 */
ZR_CORE_API TZrUInt64 ZrCore_CallBinding_FunctionSignatureHash(const struct SZrFunction *function);
/** @brief 为函数图建立指令到缓存的映射，并重定位本地、native provider 与延迟模块调用点。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_LinkFunction(
        struct SZrState *state, struct SZrFunction *function, SZrCallBindingDiagnostic *diagnostic);
/** @brief 计算编译原型布局契约哈希，供编译期记录与导入时校验；无效原型返回零。 */
ZR_CORE_API TZrUInt64 ZrCore_CallBinding_PrototypeLayoutHash(
        const struct SZrFunction *function, TZrUInt32 prototypeIndex);
/** @brief 沿原型上下文及函数所有权链查找保存 prototypeData 的函数。 */
ZR_CORE_API struct SZrFunction *ZrCore_CallBinding_PrototypeOwner(struct SZrFunction *function);
/** @brief 按接收者验证拥有者布局并选择成员目标；成功时写入可调用值。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_PrepareMember(
        struct SZrState *state, struct SZrFunction *function, TZrUInt32 cacheIndex,
        const SZrTypeValue *receiver, SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic);
/** @brief 解释器已知调用指令的报错包装入口；验证失败会抛出运行错误。 */
ZR_CORE_API void ZrCore_CallBinding_PrepareKnownCall(
        struct SZrState *state, struct SZrFunction *function, TZrUInt32 instructionIndex,
        SZrTypeValue *callable);
/** @brief 为解释器或 AOT 已知调用检查静态绑定；无绑定记录时原值保持不变并返回 true。 */
ZR_CORE_API TZrBool ZrCore_CallBinding_TryPrepareKnownCall(
        struct SZrState *state, struct SZrFunction *function, TZrUInt32 instructionIndex,
        SZrTypeValue *callable, SZrCallBindingDiagnostic *diagnostic);

#endif
