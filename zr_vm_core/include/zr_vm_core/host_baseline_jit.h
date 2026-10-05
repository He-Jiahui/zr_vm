#ifndef ZR_VM_CORE_HOST_BASELINE_JIT_H
#define ZR_VM_CORE_HOST_BASELINE_JIT_H

/**
 * @brief 主机 JIT 的 core 元数据校验与记录生命周期边界。
 * 目标与身份摘要不包含可执行地址；manifest、manager 和 handle 仍借用进程内指针，不可原样持久化。
 * 输入在调用期间保持稳定，可写输出与输入、manager 及记录数组应为独立存储；共享诊断和输出的并发使用由调用方串行化。
 * 可选 facade 的 provider、状态图登记和地址资源不由此接口拥有；SDK service 的 SZrExecutionCodeHandle 是另一套句柄协议。
 */
/*
 * Host baseline JIT boundary.
 *
 * Scalar witnesses carry no executable address. An optional ORC or
 * JITLink adapter may keep its process-local executable address privately,
 * but the address never enters this contract, an artifact, or a persistent
 * binding.  Keeping this layer in core lets a build without LLVM retain the
 * ordinary AOT/ExecBC route.
 */

#include "zr_vm_core/conf.h"
#include "zr_vm_core/execution_contract.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 目标、选项与清单共用的声明 schema；它与执行 ABI 版本分别校验。 */
#define ZR_HOST_JIT_CONTRACT_SCHEMA_VERSION ((TZrUInt32)1u)

/** @brief 主机架构声明；只有编译目标匹配的 x86-64/AArch64 可被接受。 */
typedef enum EZrHostJitArchitecture {
    ZR_HOST_JIT_ARCH_NONE = 0,
    ZR_HOST_JIT_ARCH_X86_64 = 1,
    ZR_HOST_JIT_ARCH_AARCH64 = 2
} EZrHostJitArchitecture;

/** @brief 契约中的平台标签；当前验证只接受 HOST，其余标签用于明确拒绝。 */
typedef enum EZrHostJitPlatform {
    /* TODO: 当前首方后缀检索未见此预留枚举的正向消费；需确认仓库外 ABI 用途。 */
    ZR_HOST_JIT_PLATFORM_NONE = 0,
    ZR_HOST_JIT_PLATFORM_HOST = 1,
    ZR_HOST_JIT_PLATFORM_ANDROID = 2,
    /* TODO: 当前首方后缀检索未见此预留枚举的正向消费；需确认仓库外 ABI 用途。 */
    ZR_HOST_JIT_PLATFORM_IOS = 3,
    ZR_HOST_JIT_PLATFORM_WASM = 4
} EZrHostJitPlatform;

/** @brief 目标字节序声明；当前接口只接受 LITTLE，不做运行时端序探测。 */
typedef enum EZrHostJitEndian {
    /* TODO: 当前首方后缀检索未见此预留枚举的正向消费；需确认仓库外 ABI 用途。 */
    ZR_HOST_JIT_ENDIAN_NONE = 0,
    ZR_HOST_JIT_ENDIAN_LITTLE = 1,
    /* TODO: 当前首方后缀检索未见此预留枚举的正向消费；需确认仓库外 ABI 用途。 */
    ZR_HOST_JIT_ENDIAN_BIG = 2
} EZrHostJitEndian;

/** @brief core 契约与记录操作的拒绝分类；上层 facade 另有状态映射，不可混用枚举。 */
typedef enum EZrHostJitStatus {
    ZR_HOST_JIT_STATUS_OK = 0,
    ZR_HOST_JIT_STATUS_INVALID_ARGUMENT,
    ZR_HOST_JIT_STATUS_SCHEMA_MISMATCH,
    ZR_HOST_JIT_STATUS_TARGET_UNSUPPORTED,
    ZR_HOST_JIT_STATUS_TARGET_MISMATCH,
    ZR_HOST_JIT_STATUS_ABI_MISMATCH,
    ZR_HOST_JIT_STATUS_LAYOUT_MISMATCH,
    ZR_HOST_JIT_STATUS_INVALID_FLAGS,
    ZR_HOST_JIT_STATUS_IMPORT_FORBIDDEN,
    ZR_HOST_JIT_STATUS_IMPORT_INVALID,
    /* TODO: 当前 core 未返回此状态，仅有名称与上层映射；需核清未来或仓库外生产协议。 */
    ZR_HOST_JIT_STATUS_OPERATION_UNSUPPORTED,
    ZR_HOST_JIT_STATUS_REGISTRATION_INCOMPLETE,
    ZR_HOST_JIT_STATUS_WX_REQUIRED,
    ZR_HOST_JIT_STATUS_CODE_INVALID,
    ZR_HOST_JIT_STATUS_CAPACITY,
    ZR_HOST_JIT_STATUS_NOT_PREPARED,
    ZR_HOST_JIT_STATUS_STALE_HANDLE,
    /* TODO: 当前 core 未返回此状态；facade 的关闭状态属另一枚举，需核清本枚举的未来或外部生产协议。 */
    ZR_HOST_JIT_STATUS_ACTIVE_LEASE,
    ZR_HOST_JIT_STATUS_INVALID_STATE,
    ZR_HOST_JIT_STATUS_OVERFLOW
} EZrHostJitStatus;

/** @brief 调用方拥有的可选错误见证；数值与 hash 的解释由失败分支决定，不保存资源或指针。 */
typedef struct SZrHostJitDiagnostic {
    /** @brief 本次拒绝分类；成功调用清零后为 OK。 */
    EZrHostJitStatus status;
    /** @brief 失败分支对应的清单或记录下标；非统一源码位置。 */
    TZrUInt32 sourceIndex;
    /** @brief 失败分支的期望数值。 */
    TZrUInt32 expected;
    /** @brief 失败分支的实际数值。 */
    TZrUInt32 actual;
    /** @brief 保留完整宽度的期望身份或摘要。 */
    TZrUInt64 expectedHash;
    /** @brief 保留完整宽度的实际身份或摘要。 */
    TZrUInt64 actualHash;
} SZrHostJitDiagnostic;

/** @brief 用于比较的目标摘要；ABI 版本须匹配 core，非零 triple/layout hash 本身不证明真实宿主布局。 */
typedef struct SZrHostJitTargetContract {
    /** @brief 目标声明的 schema，须为当前版本。 */
    TZrUInt32 schemaVersion;
    /** @brief 须同时属于允许架构并匹配编译目标。 */
    EZrHostJitArchitecture architecture;
    /** @brief 须为 HOST；不能用移动端标签取得 host 发布资格。 */
    EZrHostJitPlatform platform;
    /** @brief 须匹配本 core 的指针字节宽度。 */
    TZrUInt32 pointerSize;
    /** @brief 须声明 LITTLE；验证器不探测运行时端序。 */
    EZrHostJitEndian endianness;
    /** @brief 须等于 execution contract 的 ABI 版本。 */
    TZrUInt32 abiVersion;
    /** @brief 调用方提供的非零目标摘要；此层不重算。 */
    TZrUInt64 targetTripleHash;
    /** @brief 调用方提供的非零布局摘要；发布事实须与此值一致。 */
    TZrUInt64 layoutHash;
} SZrHostJitTargetContract;

/* ENABLE 只决定继续校验启用配置；ALLOW_FALLBACK 由 facade 解释，core 不选择执行后端。 */
#define ZR_HOST_JIT_OPTION_FLAG_ENABLE ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_OPTION_FLAG_KNOWN_MASK \
    (ZR_HOST_JIT_OPTION_FLAG_ENABLE | ZR_HOST_JIT_OPTION_FLAG_ALLOW_FALLBACK)

/** @brief 启用记录管理前的配置声明；预算由 facade 使用，core 校验不分配机器码缓存。 */
typedef struct SZrHostJitOptions {
    /** @brief 配置声明的 schema，禁用时仍须匹配。 */
    TZrUInt32 schemaVersion;
    /** @brief 仅容许已知位；ENABLE 决定是否继续核对目标与预算。 */
    TZrUInt32 flags;
    /** @brief 启用时借此核对主机与 ABI。 */
    SZrHostJitTargetContract target;
    /** @brief 启用时须非零；facade 用于计算记录容量，不是 core 分配的机器码页。 */
    TZrUInt32 codeCacheBytes;
    /** @brief 启用时须非零；实际导入数量上限由 facade 查询路径另核。 */
    TZrUInt32 maxImports;
} SZrHostJitOptions;

/** @brief 导入来源标签；RUNTIME/NATIVE 都要求符号身份与签名，标签不授予调用能力。 */
typedef enum EZrHostJitImportKind {
    /* TODO: 当前首方后缀检索未见此预留枚举的正向消费；需确认仓库外 ABI 用途。 */
    ZR_HOST_JIT_IMPORT_NONE = 0,
    ZR_HOST_JIT_IMPORT_RUNTIME = 1,
    ZR_HOST_JIT_IMPORT_NATIVE = 2
} EZrHostJitImportKind;

/** @brief 一个白名单符号的声明；不携带解析地址，也不拥有宿主函数。 */
typedef struct SZrHostJitImport {
    /** @brief 白名单内唯一且非零的符号身份。 */
    TZrUInt64 symbolId;
    /** @brief 须非零并与查询签名一致；不等于函数地址。 */
    TZrUInt64 signatureHash;
    /** @brief 仅允许 RUNTIME 或 NATIVE；不表达授权结果。 */
    EZrHostJitImportKind kind;
    /** @brief 协议预留，当前须为零。 */
    TZrUInt32 reserved;
} SZrHostJitImport;

/** @brief 调用期间借用的符号白名单；数组和摘要必须保持稳定，core 不缓存数组或重算 allowedSymbolHash。 */
typedef struct SZrHostJitImportManifest {
    /** @brief 清单声明的 schema。 */
    TZrUInt32 schemaVersion;
    /** @brief 借用数组的条目数；调用方保证真实长度。 */
    TZrUInt32 count;
    /** @brief 借用数组地址；非空清单在整个校验期间不得失效或改变。 */
    const SZrHostJitImport *imports;
    /** @brief 调用方提供的非零白名单摘要；此层不校验其与条目内容对应。 */
    TZrUInt64 allowedSymbolHash;
} SZrHostJitImportManifest;

/* 操作位用于声明覆盖范围；SupportsOperation 只接受单个已知位，不保证真实机器码支持。 */
#define ZR_HOST_JIT_OPERATION_TYPED_SCALAR ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_OPERATION_CONTROL ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_OPERATION_DIRECT_CALL ((TZrUInt32)1u << 2u)
#define ZR_HOST_JIT_OPERATION_SIMPLE_MEMBER ((TZrUInt32)1u << 3u)
#define ZR_HOST_JIT_OPERATION_ARRAY ((TZrUInt32)1u << 4u)
#define ZR_HOST_JIT_OPERATION_KNOWN_MASK \
    (ZR_HOST_JIT_OPERATION_TYPED_SCALAR | ZR_HOST_JIT_OPERATION_CONTROL | \
     ZR_HOST_JIT_OPERATION_DIRECT_CALL | ZR_HOST_JIT_OPERATION_SIMPLE_MEMBER | \
     ZR_HOST_JIT_OPERATION_ARRAY)

/* 机器码与 W^X 均为发布者声明；core 不检查内存页权限。 */
#define ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_PUBLICATION_FLAG_WX ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_PUBLICATION_FLAG_KNOWN_MASK \
    (ZR_HOST_JIT_PUBLICATION_FLAG_MACHINE_CODE | ZR_HOST_JIT_PUBLICATION_FLAG_WX)

/* 四类注册位必须同时具备；core 只核标记与非零摘要，不执行图登记。 */
#define ZR_HOST_JIT_REGISTRATION_ROOTS ((TZrUInt32)1u << 0u)
#define ZR_HOST_JIT_REGISTRATION_UNWIND ((TZrUInt32)1u << 1u)
#define ZR_HOST_JIT_REGISTRATION_DEBUG ((TZrUInt32)1u << 2u)
#define ZR_HOST_JIT_REGISTRATION_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_HOST_JIT_REGISTRATION_KNOWN_MASK \
    (ZR_HOST_JIT_REGISTRATION_ROOTS | ZR_HOST_JIT_REGISTRATION_UNWIND | \
     ZR_HOST_JIT_REGISTRATION_DEBUG | ZR_HOST_JIT_REGISTRATION_DEOPT)

/** @brief 发布者提供的完整声明；页权限、四图注册与 provider 的实际资源仍由提交者负责。 */
typedef struct SZrHostJitPublicationFacts {
    /** @brief 本次发布的目标声明；不是已注册 facade target 的自动绑定。 */
    SZrHostJitTargetContract target;
    /** @brief 须声明 MACHINE_CODE 与 WX，未知位拒绝。 */
    TZrUInt32 flags;
    /** @brief 须声明四类图已注册；此层不做平台登记。 */
    TZrUInt32 registrationFlags;
    /** @brief 须有已知操作位且无未知位；不代表 provider 已实现操作。 */
    TZrUInt32 operationMask;
    /** @brief 协议预留，当前须为零。 */
    TZrUInt32 reserved;
    /** @brief 非零签名摘要，复制到准备记录。 */
    TZrUInt64 signatureHash;
    /** @brief 须与 target 的 layoutHash 相同，复制到准备记录。 */
    TZrUInt64 layoutHash;
    /** @brief 非零 ABI 摘要；只作记录身份信息，不重算。 */
    TZrUInt64 abiHash;
    /** @brief 仅校验已知 capability 位；不授予所声明权限。 */
    TZrUInt32 requiredCapabilities;
    /** @brief 仅校验已知 effect 位；不证明实际机器码行为。 */
    TZrUInt32 declaredEffects;
    /** @brief 非零 GC 图摘要；core 不保存或读取图内容。 */
    TZrUInt64 gcMapHash;
    /** @brief 非零 unwind 图摘要；core 不执行平台 unwind 登记。 */
    TZrUInt64 unwindMapHash;
    /** @brief 非零 debug 图摘要；core 不拥有调试登记。 */
    TZrUInt64 debugMapHash;
    /** @brief 非零 deopt 图摘要；core 不执行反优化。 */
    TZrUInt64 deoptMapHash;
    /** @brief 须非零；不据此访问或分配任何代码页。 */
    TZrUInt32 codeSize;
    /** @brief 须非零且不得与当前非空记录重复；不是执行地址。 */
    TZrUInt64 codeIdentity;
    /** @brief 可为空的借用清单；只在本次校验使用，不随记录持有。 */
    const SZrHostJitImportManifest *imports;
} SZrHostJitPublicationFacts;

/** @brief 记录生命周期状态；退休不等于立即回收，lease 决定空槽可重用的时点。 */
typedef enum EZrHostJitCodeState {
    ZR_HOST_JIT_CODE_FREE = 0,
    ZR_HOST_JIT_CODE_PREPARED,
    ZR_HOST_JIT_CODE_PUBLISHED,
    ZR_HOST_JIT_CODE_RETIRED
} EZrHostJitCodeState;

/** @brief manager 借用数组中的一条元数据记录；字段由 manager 锁保护，不保存可执行地址。 */
typedef struct SZrHostJitCodeRecord {
    /** @brief 非空记录的唯一身份；空槽必须为零。 */
    TZrUInt64 codeIdentity;
    /** @brief 准备时复制的非零签名摘要。 */
    TZrUInt64 signatureHash;
    /** @brief 准备时复制的非零布局摘要。 */
    TZrUInt64 layoutHash;
    /** @brief 准备时复制的非零 ABI 摘要。 */
    TZrUInt64 abiHash;
    /** @brief 持锁维护的 FREE/PREPARED/PUBLISHED/RETIRED 数值。 */
    TZrUInt32 state;
    /** @brief 持锁维护的 lease 计数；volatile 不允许绕过锁访问。 */
    volatile TZrUInt32 leaseCount;
} SZrHostJitCodeRecord;

/**
 * @brief 借用固定容量记录数组的互斥管理器；调用方拥有 manager/数组存储并保证其地址稳定。
 * 初始化与最终释放须由所有者独占；内部自旋锁只保护正常记录操作，禁止直接并发改字段。
 */
typedef struct SZrHostJitCodeManager {
    /** @brief 内部原子自旋锁字；调用方不得独立改写。 */
    volatile TZrUInt32 lock;
    /** @brief 唯一 PUBLISHED 记录的借用指针，或者空。 */
    SZrHostJitCodeRecord *active;
    /** @brief 调用方拥有的固定记录数组；排空且解除借用后才可释放。 */
    SZrHostJitCodeRecord *records;
    /** @brief 真实数组容量，初始化后在借用期间保持不变。 */
    TZrUInt32 capacity;
    /** @brief 非 FREE 记录数；必须与整数组扫描一致。 */
    TZrUInt32 count;
} SZrHostJitCodeManager;

/**
 * @brief 本 manager 的进程内记录引用；prepared 是未租用引用，acquired 代表一份须配对归还的 lease。
 * leased 句柄须按单份拥有权使用；复制不能增加 lease，也不得跨 manager 或在数组释放后使用。
 * TODO: 空槽回收后允许相同 codeIdentity 复用，尚无独立 generation；需确认外部调用方如何禁止旧准备句柄重新匹配。
 */
typedef struct SZrHostJitCodeHandle {
    /** @brief 借用数组内的记录位置；不得跨 manager 或在数组失效后使用。 */
    SZrHostJitCodeRecord *record;
    /** @brief 与当前位置记录核对的身份；尚无独立 generation。 */
    TZrUInt64 codeIdentity;
    /** @brief 是否代表已取得的一份 lease；不能由调用方置位伪造或靠复制增加拥有权。 */
    TZrBool leased;
} SZrHostJitCodeHandle;

/** @brief Resolve 返回的标量快照；没有记录指针、执行地址或独立 lease，不能据此延长任何资源生命周期。 */
typedef struct SZrHostJitCodeView {
    /** @brief 本次持锁快照的记录身份。 */
    TZrUInt64 codeIdentity;
    /** @brief 本次持锁快照的签名摘要。 */
    TZrUInt64 signatureHash;
    /** @brief 本次持锁快照的布局摘要。 */
    TZrUInt64 layoutHash;
    /** @brief 本次持锁快照的 ABI 摘要。 */
    TZrUInt64 abiHash;
    /** @brief 本次快照状态，可能已经 RETIRED。 */
    EZrHostJitCodeState state;
    /** @brief 本次快照计数，不是此 view 持有的 lease。 */
    TZrUInt32 leaseCount;
} SZrHostJitCodeView;

/**
 * @brief 确认目标声明符合本 core 的主机、ABI 与布局摘要约束。
 * @note 只读借用输入；hash 仅要求非零，不重算宿主布局或证明 provider 可用。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidateTarget(
        const SZrHostJitTargetContract *target,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 为启用主机记录管理的配置建立前置契约。
 * @note 未设 ENABLE 时只核 schema 和已知选项位；不校验 target/预算，也不分配缓存。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidateOptions(
        const SZrHostJitOptions *options,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 按符号身份与签名共同限制一次导入查询。
 * @note 调用期间借用完整清单和数组；拒绝缺项或签名不符，不返回可调用地址。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidateImports(
        const SZrHostJitImportManifest *manifest,
        TZrUInt64 symbolId,
        TZrUInt64 signatureHash,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 核对发布者提交的目标、注册与代码摘要声明是否自洽。
 * @note MACHINE_CODE/WX/四图位与非零 hash 都是提交者的声明；本层不检查页权限、图内容或机器码。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_ValidatePublication(
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 检查一个已知操作位是否包含在给定声明掩码中。
 * @note operation 必须恰有一个已知位；不校验 mask 的其他位，也不查询机器码能力。
 */
ZR_CORE_API TZrBool ZrCore_HostJit_SupportsOperation(
        TZrUInt32 operation,
        TZrUInt32 operationMask);

/**
 * @brief 把调用方的固定记录数组接入新 manager 并建立全空状态。
 * @note 借用并清零整个数组，不分配或释放存储；初始化须独占且不可覆盖尚有记录或 lease 的实例。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_CodeManager_Init(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeRecord *records,
        TZrUInt32 capacity,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 仅在完整且已排空的 manager 上解除对数组的借用。
 * @note void 入口在有记录或外壳失效时保留原状态；调用方须确认 records 已为空后才释放数组。
 */
ZR_CORE_API void ZrCore_HostJit_CodeManager_Deinit(
        SZrHostJitCodeManager *manager);
/**
 * @brief 把已验证发布声明的身份与三个 hash 放入空记录。
 * @note 输出是未租用的准备句柄；不保存 imports/状态图，不生成机器码；调用方须提供无活跃 lease 的独立输出。
 * facts/manager 前置校验失败时输出可能仍为原值；进入记录操作前才清零，失败时不能把输出当作新准备句柄。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Prepare(
        SZrHostJitCodeManager *manager,
        const SZrHostJitPublicationFacts *facts,
        SZrHostJitCodeHandle *outHandle,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 使准备记录成为唯一 active，并让旧发布记录进入退休状态。
 * @note 仅接受本 manager 的未租用 PREPARED 句柄；不会取得 lease 或清空 prepared，也不发布可执行地址。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Publish(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *prepared,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 为当前已发布记录取得一份须归还的 lease。
 * @note 输出不得覆盖已有 lease；退休可与既有 lease 并存，记录数组必须保持有效直到归还。
 * 空 manager 等前置失败不保证清空输出；只有成功返回才表示取得新 lease。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_AcquireActive(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *outHandle,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 按身份退休记录，阻止其继续作为 active 被获取。
 * @note PREPARED 也可退休；已有 lease 不减少，实际回收由 CollectRetired 在归还后完成。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Evict(
        SZrHostJitCodeManager *manager,
        TZrUInt64 codeIdentity,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 在 lease 有效期间取得记录元数据的同次持锁快照。
 * @note RETIRED 仍可解析；const manager 仍会修改锁，须来自可写实例；view 不是入口地址或新 lease。
 * 无效输入的早期失败不保证清空 view；仅成功快照可供调用方观察。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Resolve(
        const SZrHostJitCodeManager *manager,
        const SZrHostJitCodeHandle *handle,
        SZrHostJitCodeView *outView,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 归还传入句柄代表的一份 lease，并清空该句柄。
 * @note 成功后该句柄不可再用；不自动回收退休记录，复制 leased 句柄不能产生另一份拥有权。
 * 失败路径保留传入句柄；不能把失败当作 lease 已归还。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_Release(
        SZrHostJitCodeManager *manager,
        SZrHostJitCodeHandle *handle,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 在最后一份 lease 归还后把退休记录恢复为空槽。
 * @note 只清除 core 元数据并报告数量；外部 proof 与可执行资源由其所有者另行管理。
 * 非空 outCollected 在 manager 校验前置零；成功计数仅表示本层清零的记录数。
 */
ZR_CORE_API EZrHostJitStatus ZrCore_HostJit_Code_CollectRetired(
        SZrHostJitCodeManager *manager,
        TZrUInt32 *outCollected,
        SZrHostJitDiagnostic *diagnostic);
/**
 * @brief 为 core 状态提供借用的静态诊断名称。
 * @note 未知值返回通用名称，调用方不得释放返回字符串；TODO: 当前全后缀检索无正向调用，需确认仓库外公开 ABI 消费者。
 */
ZR_CORE_API const TZrChar *ZrCore_HostJit_StatusName(EZrHostJitStatus status);

#ifdef __cplusplus
}
#endif

#endif
