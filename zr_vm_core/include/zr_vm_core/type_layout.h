//
// Created by Codex on 2026/5/16.
//

#ifndef ZR_VM_CORE_TYPE_LAYOUT_H
#define ZR_VM_CORE_TYPE_LAYOUT_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"

/* 布局递归共用的防环上限：初始化、复制、释放和 GC 扫描均受其约束。 */
#define ZR_TYPE_LAYOUT_MAX_NESTING_DEPTH ((TZrUInt32)64u)

struct SZrState;

/* 哈希与运行时校验使用同一布局模式版本，跨版本描述符不可直接消费。 */
#define ZR_TYPE_LAYOUT_SCHEMA_VERSION ((TZrUInt32)2u)

/** @brief 区分完整值槽、顺序结构与按 tag 选择活动成员的联合布局。 */
typedef enum EZrTypeLayoutKind {
    ZR_TYPE_LAYOUT_KIND_VALUE = 0,
    ZR_TYPE_LAYOUT_KIND_STRUCT = 1,
    ZR_TYPE_LAYOUT_KIND_UNION = 2
} EZrTypeLayoutKind;

/** @brief 决定复制是否可按字节、需逐字段处理，或必须拒绝复制。 */
typedef enum EZrTypeLayoutCopyKind {
    ZR_TYPE_LAYOUT_COPY_KIND_BITWISE = 0,
    ZR_TYPE_LAYOUT_COPY_KIND_POD = ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
    ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE = 1,
    ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY = ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
    ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY = 2
} EZrTypeLayoutCopyKind;

/** @brief 决定释放时是否遍历字段，以及是否先执行宿主清理回调。 */
typedef enum EZrTypeLayoutDropKind {
    ZR_TYPE_LAYOUT_DROP_KIND_NONE = 0,
    ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE = 1,
    ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP = ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
    ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS = 2
} EZrTypeLayoutDropKind;

/** @brief 表示 GC 扫描策略；具体可跳过条件还须由 CanSkipGcScan 判定。 */
typedef enum EZrTypeLayoutGcScanKind {
    ZR_TYPE_LAYOUT_GC_SCAN_FREE = 0,
    ZR_TYPE_LAYOUT_GC_SCAN_MAPPED = 1,
    ZR_TYPE_LAYOUT_GC_SCAN_BARRIERED = 2
} EZrTypeLayoutGcScanKind;

/** @brief 跨域转移的语义承诺，须与摘要和 provider 约束一起校验。 */
typedef enum EZrDomainTransferKind {
    ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN = 0,
    ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY = 1,
    ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE = 2,
    ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE = 3,
    ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE = 4
} EZrDomainTransferKind;

/** @brief 标记值槽的 GC/所有权/引用角色或对子布局的借用引用。 */
typedef enum EZrTypeLayoutFieldFlags {
    ZR_TYPE_LAYOUT_FIELD_FLAG_NONE = 0,
    ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT = 1u << 0u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE = 1u << 1u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE = 1u << 2u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE = 1u << 3u,
    ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT = 1u << 4u
} EZrTypeLayoutFieldFlags;

/** @brief 描述内联字段的字节区间；联合成员以 activeTag 参与选择。 */
typedef struct SZrTypeLayoutField {
    TZrUInt32 byteOffset;
    TZrUInt32 byteSize;
    TZrUInt32 typeLayoutIndex;
    TZrUInt32 flags;
    TZrUInt32 activeTag;
} SZrTypeLayoutField;

/** @brief 旧式构建入口的可选元数据；偏移表由调用方保有。 */
typedef struct SZrTypeLayoutMetadata {
    TZrUInt32 cTypeId;
    const TZrUInt32 *gcFieldOffsets;
    const TZrUInt32 *ownershipFieldOffsets;
} SZrTypeLayoutMetadata;

/** @brief 完整释放时的宿主清理钩子；不得替代随后执行的字段所有权释放。 */
typedef void (*FZrTypeLayoutCustomDrop)(struct SZrState *state,
                                       TZrPtr storage,
                                       TZrPtr userData);

/**
 * @brief 构建布局时附加 GC、跨域转移及自定义释放契约。
 * @note 三种偏移表和回调 userData 只被布局借用；必须存活至最后一次消费布局。
 */
typedef struct SZrTypeLayoutContract {
    TZrUInt32 cTypeId;
    EZrTypeLayoutGcScanKind gcScanKind;
    const TZrUInt32 *gcFieldOffsets;
    TZrUInt32 gcFieldCount;
    const TZrUInt32 *ownershipFieldOffsets;
    TZrUInt32 ownershipFieldCount;
    const TZrUInt32 *refFieldOffsets;
    TZrUInt32 refFieldCount;
    TZrBool hasDomainTransferContract;
    EZrDomainTransferKind domainTransferKind;
    TZrUInt32 domainTransferSchemaVersion;
    TZrUInt64 domainTransferSchemaHash;
    TZrUInt32 domainTransferProviderToken;
    TZrUInt64 domainTransferProviderContractHash;
    FZrTypeLayoutCustomDrop customDrop;
    TZrPtr customDropUserData;
} SZrTypeLayoutContract;

/**
 * @brief 供函数内联存储、对象数组、GC 和跨域转移共同消费的布局快照。
 * @note fields、各偏移表及 customDropUserData 均由构建方持有；构建后保持内容稳定，
 *       并在消费前执行 Validate。layoutHash 用于一致性校验，不是安全认证摘要。
 */
typedef struct SZrTypeLayout {
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt8 kind;
    TZrUInt8 copyKind;
    TZrUInt8 dropKind;
    TZrUInt8 gcScanKind;
    const SZrTypeLayoutField *fields;
    TZrUInt32 fieldCount;
    TZrUInt32 gcFieldCount;
    TZrUInt32 ownershipFieldCount;
    TZrUInt32 refFieldCount;
    TZrUInt32 tagOffset;
    TZrUInt32 tagSize;
    TZrBool blittable;
    TZrUInt8 domainTransferKind;
    TZrUInt8 reserved2;
    TZrUInt8 reserved3;
    TZrUInt32 cTypeId;
    TZrUInt32 layoutVersion;
    TZrUInt32 domainTransferSchemaVersion;
    TZrUInt32 domainTransferProviderToken;
    TZrUInt64 layoutHash;
    TZrUInt64 domainTransferSchemaHash;
    TZrUInt64 domainTransferProviderContractHash;
    const TZrUInt32 *gcFieldOffsets;
    const TZrUInt32 *ownershipFieldOffsets;
    const TZrUInt32 *refFieldOffsets;
    FZrTypeLayoutCustomDrop customDrop;
    TZrPtr customDropUserData;
} SZrTypeLayout;

/** @brief 将嵌套索引映射为借用布局；数组及目标布局须在消费方持有期间保持稳定。 */
typedef struct SZrTypeLayoutRegistryView {
    const SZrTypeLayout *const *layouts;
    TZrUInt32 count;
} SZrTypeLayoutRegistryView;

/** @brief 帧布局候选槽；BuildSequential 在失败时可能已改写前几个 byteOffset。 */
typedef struct SZrStackFrameLayoutSlot {
    const SZrTypeLayout *typeLayout;
    TZrUInt32 byteOffset;
} SZrStackFrameLayoutSlot;

/** @brief 顺序帧布局的大小、最大对齐和槽数，仅在完整构建成功后发布。 */
typedef struct SZrStackFrameLayout {
    TZrUInt32 byteSize;
    TZrUInt32 maxAlign;
    TZrUInt32 slotCount;
} SZrStackFrameLayout;

/** @brief GC 遍历遇到活值槽时调用；回调不得使当前布局或存储失效。 */
typedef void (*FZrTypeLayoutGcValueVisitor)(struct SZrState *state,
                                            SZrTypeValue *value,
                                            TZrPtr userData);

/** @brief 构建完整 SZrTypeValue 槽的标准布局与版本摘要。 */
ZR_CORE_API void ZrCore_TypeLayout_InitValue(SZrTypeLayout *layout);

/** @brief 构建不带附加映射的结构布局；fields 保留调用方所有权。 */
ZR_CORE_API void ZrCore_TypeLayout_InitStruct(SZrTypeLayout *layout,
                                              TZrUInt32 byteSize,
                                              TZrUInt32 byteAlign,
                                              EZrTypeLayoutCopyKind copyKind,
                                              EZrTypeLayoutDropKind dropKind,
                                              const SZrTypeLayoutField *fields,
                                              TZrUInt32 fieldCount);

/** @brief 用旧式元数据构建结构布局；metadata 本身可临时存在，内部表不可。 */
ZR_CORE_API void ZrCore_TypeLayout_InitStructWithMetadata(SZrTypeLayout *layout,
                                                          TZrUInt32 byteSize,
                                                          TZrUInt32 byteAlign,
                                                          EZrTypeLayoutCopyKind copyKind,
                                                          EZrTypeLayoutDropKind dropKind,
                                                          const SZrTypeLayoutField *fields,
                                                          TZrUInt32 fieldCount,
                                                          const SZrTypeLayoutMetadata *metadata);

/**
 * @brief 封存结构字段和可选跨层契约，供帧、对象和 FFI 共享。
 * @note 此入口计算摘要但不验证尺寸、偏移和映射；消费前须调用 Validate。
 */
ZR_CORE_API void ZrCore_TypeLayout_InitStructWithContract(SZrTypeLayout *layout,
                                                          TZrUInt32 byteSize,
                                                          TZrUInt32 byteAlign,
                                                          EZrTypeLayoutCopyKind copyKind,
                                                          EZrTypeLayoutDropKind dropKind,
                                                          const SZrTypeLayoutField *fields,
                                                          TZrUInt32 fieldCount,
                                                          const SZrTypeLayoutContract *contract);

/** @brief 构建无附加契约的联合布局；需按复制/释放策略提供可读活动 tag。 */
ZR_CORE_API void ZrCore_TypeLayout_InitUnion(SZrTypeLayout *layout,
                                             TZrUInt32 byteSize,
                                             TZrUInt32 byteAlign,
                                             TZrUInt32 tagOffset,
                                             TZrUInt32 tagSize,
                                             EZrTypeLayoutCopyKind copyKind,
                                             EZrTypeLayoutDropKind dropKind,
                                             const SZrTypeLayoutField *fields,
                                             TZrUInt32 fieldCount);

/** @brief 封存联合布局及契约；逐字段复制/释放/扫描依赖有效的活动 tag。 */
ZR_CORE_API void ZrCore_TypeLayout_InitUnionWithContract(SZrTypeLayout *layout,
                                                         TZrUInt32 byteSize,
                                                         TZrUInt32 byteAlign,
                                                         TZrUInt32 tagOffset,
                                                         TZrUInt32 tagSize,
                                                         EZrTypeLayoutCopyKind copyKind,
                                                         EZrTypeLayoutDropKind dropKind,
                                                         const SZrTypeLayoutField *fields,
                                                         TZrUInt32 fieldCount,
                                                         const SZrTypeLayoutContract *contract);

/** @brief 从布局语义字段和借用的字段/映射表计算一致性摘要。 */
ZR_CORE_API TZrUInt64 ZrCore_TypeLayout_ComputeHash(const SZrTypeLayout *layout);

/** @brief 校验布局版本、摘要、字段范围和部分契约；通过后才能交给存储消费者。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_Validate(const SZrTypeLayout *layout);

/** @brief 按 GC 映射序号取得值槽偏移；调用方先持有有效且稳定的布局。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_TryGetGcFieldOffset(const SZrTypeLayout *layout,
                                                          TZrUInt32 fieldIndex,
                                                          TZrUInt32 *outOffset);

/** @brief 按所有权映射序号取得值槽偏移；不转移输出或布局所有权。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_TryGetOwnershipFieldOffset(const SZrTypeLayout *layout,
                                                                 TZrUInt32 fieldIndex,
                                                                 TZrUInt32 *outOffset);

/** @brief 按引用映射序号取得值槽偏移；无映射表时回退至字段标记。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_TryGetRefFieldOffset(const SZrTypeLayout *layout,
                                                           TZrUInt32 fieldIndex,
                                                           TZrUInt32 *outOffset);

/** @brief 查询构建时记录的可逐字节复制承诺；本接口不重新验证描述符。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_CanRawCopy(const SZrTypeLayout *layout);

/** @brief 仅当结构已验证且无 GC、所有权、引用或嵌套字段时允许跳过扫描。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_CanSkipGcScan(const SZrTypeLayout *layout);

/** @brief 复制无嵌套引用的内联值；失败可能已经改写目的存储。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_CopyInline(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 TZrPtr destination,
                                                 const void *source);

/**
 * @brief 借助布局注册表递归复制内联字段，值槽交给 ZrCore_Value_Copy 保持值与所有权语义。
 * @pre 布局及源/目的存储有效；目的值槽已初始化，嵌套索引在 registry 内。
 * @return 失败时目的存储不保证保持原样，调用方负责后续清理。
 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_CopyInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr destination,
        const void *source);
/**
 * @brief 为帧或对象的内联存储建立可释放的空值状态，递归处理嵌套字段。
 * @note 先清零整个存储；失败时可能只完成部分字段，调用方不可视为完整实例。
 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_InitializeStorageWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage);
/** @brief 初始化无嵌套引用的内联存储；带嵌套字段请使用 registry 版本。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_InitializeStorage(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        TZrPtr storage);

/** @brief 释放无嵌套引用的完整内联存储；此兼容入口不报告遍历失败。 */
ZR_CORE_API void ZrCore_TypeLayout_DropInline(struct SZrState *state,
                                              const SZrTypeLayout *layout,
                                              TZrPtr storage);

/** @brief 逆序释放完整内联字段；registry 解析失败会报告 false。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_DropInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage);

/**
 * @brief 仅释放位图标记已初始化的字段，供帧构建失败回滚。
 * @note 部分释放跳过 customDrop；位图以字段索引而非字节偏移编址。
 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_DropPartialInlineWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        const TZrUInt64 *initializedFieldWords,
        TZrUInt32 initializedFieldWordCount);

/** @brief 遍历无嵌套引用的 GC 值槽；此兼容入口不报告遍历失败。 */
ZR_CORE_API void ZrCore_TypeLayout_VisitGcValues(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 TZrPtr storage,
                                                 FZrTypeLayoutGcValueVisitor visitor,
                                                 TZrPtr userData);

/** @brief 按活动联合成员和嵌套布局遍历 GC 值槽，失败由调用方处理。 */
ZR_CORE_API TZrBool ZrCore_TypeLayout_VisitGcValuesWithRegistry(
        struct SZrState *state,
        const SZrTypeLayout *layout,
        const SZrTypeLayoutRegistryView *registry,
        TZrPtr storage,
        FZrTypeLayoutGcValueVisitor visitor,
        TZrPtr userData);

/**
 * @brief 按各类型对齐要求排列帧槽，并在成功时发布帧大小和最大对齐。
 * @note 槽偏移在循环中逐个写入；失败时 frameLayout 未发布，但前缀槽可能已变。
 */
ZR_CORE_API TZrBool ZrCore_StackFrameLayout_BuildSequential(SZrStackFrameLayout *frameLayout,
                                                            SZrStackFrameLayoutSlot *slots,
                                                            TZrUInt32 slotCount);

#endif // ZR_VM_CORE_TYPE_LAYOUT_H
