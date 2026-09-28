//
// Created by HeJiahui on 2025/8/7.
//

#ifndef ZR_VM_CORE_RAW_OBJECT_H
#define ZR_VM_CORE_RAW_OBJECT_H

#include "zr_vm_core/conf.h"

struct SZrState;
struct SZrRawObject;
struct SZrTypeValue;
struct SZrOwnershipControl;

typedef void (*FRawObjectScanMarkGc)(struct SZrState *state, struct SZrRawObject *parentThis); /**< 在 GC 扫描或对象释放路径调用的自定义回调。 */
typedef void (*FZrRawObjectGcValueVisitor)(struct SZrState *state,
                                           struct SZrTypeValue *value,
                                           TZrPtr userData); /**< 将外部保存的值槽交给 GC 标记或转发处理。 */
typedef void (*FRawObjectTraceGc)(struct SZrState *state,
                                  struct SZrRawObject *parentThis,
                                  FZrRawObjectGcValueVisitor visitor,
                                  TZrPtr userData); /**< 枚举对象外部存储的 GC 值槽。 */
/** @brief 资源对象从构造、可用到析构的生命周期状态；NONE 表示未进入资源生命周期。 */
typedef enum EZrResourceLifecycleState {
    ZR_RESOURCE_LIFECYCLE_NONE = 0,       /**< 普通对象或尚未初始化为资源对象。 */
    ZR_RESOURCE_LIFECYCLE_CONSTRUCTING = 1, /**< 资源构造尚未完成。 */
    ZR_RESOURCE_LIFECYCLE_ALIVE = 2,      /**< 资源可参与所有权操作。 */
    ZR_RESOURCE_LIFECYCLE_DROPPING = 3,   /**< drop 已开始，禁止重复进入。 */
    ZR_RESOURCE_LIFECYCLE_DROPPED = 4    /**< drop 已完成。 */
} EZrResourceLifecycleState;
/** @brief GC 管理对象共享的头部；派生运行时对象借用其类型、链表、标记与所有权状态。 */
struct ZR_STRUCT_ALIGN SZrRawObject {
    struct SZrRawObject *next; /**< GC 对象登记/释放链的链接，与扫描工作链 gcList 分开使用。 */
    EZrRawObjectType type; /**< 决定对象的标记、转发及释放分派。 */
    TZrBool isNative; /**< 保留分配时的 native 分类，GC 与调用绑定路径据此分流。 */
    TZrBool isGcBox; /**< 资源值暂交 GC box 管理时置位，释放路径再恢复资源根。 */
    TZrUInt8 resourceLifecycleState; /**< 资源对象生命周期状态；普通对象保持 NONE。 */
    SZrGarbageCollectionObjectMark garbageCollectMark; /**< 本对象的增量/分代、区域、移动与 pin 标记状态。 */
    struct SZrRawObject *gcList; /**< GC 扫描与待处理工作链链接，不替代对象登记链 next。 */
    FRawObjectTraceGc traceGcFunction; /**< 为 GC 枚举并更新对象外部保存的值槽。 */
    FRawObjectScanMarkGc scanMarkGcFunction; /**< TODO: 回调同时用于标记与释放；核实双阶段契约及可抛异常约束。 */
    TZrPtr finalizerData; /**< TODO: 仓内仅见初始化和 checkpoint 空值检查；核实外部 finalizer 是否使用此槽及其序列化契约。 */
    struct SZrOwnershipControl *ownershipControl; /**< 共享所有权控制块的反向关联；控制块释放时清空，非空时禁止 GC 移动。 */
    TZrUInt64 gcDomainId; /**< 所属 GC 域身份的 id，与 generation 成对校验。 */
    TZrUInt32 gcDomainGeneration; /**< 所属 GC 域身份的 generation，避免复用 id 被误认。 */
    TZrUInt32 ownershipRootIndex; /**< 所有权根槽索引；未注册时为 NONE 哨兵。 */
    TZrUInt32 ownershipRootGeneration; /**< 根槽代数，与索引成对校验槽位是否仍属于本对象。 */
    TZrUInt64 hash; /**< 对象值从此读取哈希；字符串创建路径另写入内容哈希。 */
};

typedef struct SZrRawObject SZrRawObject;
/** @brief 初始化新分配对象的公共 GC 头部，调用方随后完成域、代际及具体子对象初始化。 */
ZR_FORCE_INLINE void ZrCore_RawObject_Construct(SZrRawObject *super, EZrRawObjectType type) {
    super->next = ZR_NULL;
    super->type = type;
    super->isNative = ZR_FALSE;
    super->isGcBox = ZR_FALSE;
    super->resourceLifecycleState = ZR_RESOURCE_LIFECYCLE_NONE;
    super->garbageCollectMark.status = ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
    super->garbageCollectMark.generationalStatus = ZR_GARBAGE_COLLECT_GENERATIONAL_OBJECT_STATUS_NEW;
    super->garbageCollectMark.generation = ZR_GARBAGE_COLLECT_GENERATION_INVALID;
    super->garbageCollectMark.minorScanEpoch = 0u;
    super->garbageCollectMark.heapGenerationKind = ZR_GARBAGE_COLLECT_HEAP_GENERATION_KIND_YOUNG;
    super->garbageCollectMark.regionKind = ZR_GARBAGE_COLLECT_REGION_KIND_EDEN;
    super->garbageCollectMark.storageKind = ZR_GARBAGE_COLLECT_STORAGE_KIND_YOUNG_MOVABLE;
    super->garbageCollectMark.regionId = 0;
    super->garbageCollectMark.survivalAge = 0;
    super->garbageCollectMark.escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    super->garbageCollectMark.anchorScopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
    super->garbageCollectMark.pinFlags = ZR_GARBAGE_COLLECT_PIN_KIND_NONE;
    super->garbageCollectMark.promotionReason = ZR_GARBAGE_COLLECT_PROMOTION_REASON_NONE;
    super->garbageCollectMark.ignoredRegistryIndex = ZR_MAX_SIZE;
    super->garbageCollectMark.rememberedRegistryIndex = ZR_MAX_SIZE;
    super->garbageCollectMark.regionDescriptorIndex = ZR_MAX_SIZE;
    super->garbageCollectMark.forwardingAddress = ZR_NULL;
    super->garbageCollectMark.forwardingRefLocation = ZR_NULL;
    super->gcList = ZR_NULL;
    super->traceGcFunction = ZR_NULL;
    super->scanMarkGcFunction = ZR_NULL;
    super->finalizerData = ZR_NULL;
    super->ownershipControl = ZR_NULL;
    super->gcDomainId = 0u;
    super->gcDomainGeneration = 0u;
    super->ownershipRootIndex = ~(TZrUInt32)0u;
    super->ownershipRootGeneration = 0u;
    // BUG: `&super` 是构造器形参槽而非对象地址；对象键会读取该哈希，槽复用时不同对象可能挤入同一桶。
    super->hash = ((TZrUInt64) &super) / ZR_ALIGN_SIZE;
}
/** @brief 覆盖默认哈希；字符串创建路径使用此入口写入内容哈希。 */
ZR_FORCE_INLINE void ZrCore_RawObject_InitHash(SZrRawObject *super, TZrUInt64 hash) { super->hash = hash; }
/** @brief 判断对象标记状态是否为本轮初始态。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsMarkInited(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_INITED;
}
/** @brief 判断对象是否已排入待扫描状态。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsMarkWaitToScan(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_WAIT_TO_SCAN;
}
/** @brief 判断对象是否已被标记为可达。 */
ZR_FORCE_INLINE TZrBool ZrCore_RawObject_IsMarkReferenced(SZrRawObject *super) {
    return super->garbageCollectMark.status == ZR_GARBAGE_COLLECT_INCREMENTAL_OBJECT_STATUS_REFERENCED;
}


#endif // ZR_VM_CORE_RAW_OBJECT_H
