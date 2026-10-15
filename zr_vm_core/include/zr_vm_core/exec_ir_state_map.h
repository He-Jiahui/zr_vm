#ifndef ZR_VM_CORE_EXEC_IR_STATE_MAP_H
#define ZR_VM_CORE_EXEC_IR_STATE_MAP_H

#include "zr_vm_core/exec_ir.h"

/* A logical checkpoint is independent of any physical frame/register slot. */
typedef enum EZrExecIrStateMapPhase {
    ZR_EXEC_IR_STATE_BEFORE_EFFECT = 0,
    ZR_EXEC_IR_STATE_AFTER_EFFECT,
    ZR_EXEC_IR_STATE_CLEANUP_COMPLETE,
    ZR_EXEC_IR_STATE_PHASE_COUNT
} EZrExecIrStateMapPhase;

#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC ((TZrUInt32)1u << 0u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW ((TZrUInt32)1u << 1u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND ((TZrUInt32)1u << 2u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT ((TZrUInt32)1u << 3u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE ((TZrUInt32)1u << 4u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL ((TZrUInt32)1u << 5u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT ((TZrUInt32)1u << 6u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP ((TZrUInt32)1u << 7u)
#define ZR_EXEC_IR_STATE_MAP_BOUNDARY_KNOWN_MASK \
    (ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC | ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW | \
     ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND | ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT | \
     ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE | ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL | \
     ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT | ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP)
#define ZR_EXEC_IR_STATE_MAP_EXCEPTION_MASK \
    (ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW | ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND)

/* Ownership state is recorded by value ID, never by a host address. */
typedef enum EZrExecIrStateMapOwnerState {
    ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN = 0,
    ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
    ZR_EXEC_IR_STATE_MAP_OWNER_MOVED,
    ZR_EXEC_IR_STATE_MAP_OWNER_DROPPED,
    ZR_EXEC_IR_STATE_MAP_OWNER_UNINITIALIZED,
    /* Static classification only; concrete materialization resolves it. */
    ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL,
    ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT
} EZrExecIrStateMapOwnerState;

typedef struct SZrExecIrStateMapEntry {
    TZrExecIrSourceId sourceId;
    TZrExecIrInstructionId instructionId;
    TZrExecIrDeoptId deoptId;
    TZrUInt32 resumeId;
    TZrUInt32 cleanupState;
    TZrUInt32 boundaryFlags;
    EZrExecIrStateMapPhase phase;
    SZrExecIrRange liveValues;
    SZrExecIrRange rootValues;
    /* One owner-state entry corresponds to each liveValues entry. */
    SZrExecIrRange ownerStates;
    TZrExecIrEffectTokenId effectIn;
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrBlockId handlerBlockId;
    TZrUInt32 exceptionState;
} SZrExecIrStateMapEntry;

/* Function-owned logical state-map side table. All references are IDs/ranges. */
struct SZrExecIrStateMap {
    /* Stable identity prevents a map from one function being resumed in
     * another function that happens to share a generation number. */
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    SZrExecIrStateMapEntry *entries;
    TZrUInt32 entryCount;
    TZrUInt32 entryCapacity;
    TZrExecIrValueId *valuePool;
    TZrUInt32 valueCount;
    TZrUInt32 valueCapacity;
    TZrExecIrValueId *rootPool;
    TZrUInt32 rootCount;
    TZrUInt32 rootCapacity;
    TZrUInt32 *ownerStatePool;
    TZrUInt32 ownerStateCount;
    TZrUInt32 ownerStateCapacity;
    TZrUInt64 generation;
};

/* The target is caller-owned and is changed only after all preparation passes. */
typedef struct SZrExecIrMaterializedState {
    TZrExecIrSourceId sourceId;
    TZrUInt32 resumeId;
    TZrUInt32 cleanupState;
    EZrExecIrStateMapPhase phase;
    TZrUInt64 generation;
    TZrExecIrValueId *values;
    TZrUInt32 valueCount;
    TZrUInt32 valueCapacity;
    TZrExecIrValueId *roots;
    TZrUInt32 rootCount;
    TZrUInt32 rootCapacity;
    TZrUInt32 *ownerStates;
    TZrUInt32 ownerStateCount;
    TZrUInt32 ownerStateCapacity;
    TZrExecIrEffectTokenId effectIn;
    TZrExecIrEffectTokenId effectOut;
    TZrExecIrBlockId handlerBlockId;
    TZrUInt32 exceptionState;
    TZrUInt32 boundaryFlags;
    TZrExecIrDeoptId deoptId;
    SZrExecIrDeoptAggregate *aggregates;
    TZrUInt32 aggregateCount;
    TZrUInt32 aggregateCapacity;
    SZrExecIrDeoptAggregateField *aggregateFields;
    TZrUInt32 aggregateFieldCount;
    TZrUInt32 aggregateFieldCapacity;
} SZrExecIrMaterializedState;

typedef struct SZrExecIrResumeRequest {
    const SZrExecIrFunction *function;
    const SZrExecIrStateMap *map;
    TZrMetadataToken functionToken;
    TZrUInt64 generation;
    /* Optional extra identity check; zero means use the map/function pair. */
    TZrUInt64 signatureHash;
    TZrExecIrSourceId sourceId;
    TZrUInt32 resumeId;
    EZrExecIrStateMapPhase phase;
    SZrExecIrMaterializedState *target;
    /* Optional concrete witness, indexed by valueId - 1. Required to
     * materialize CONDITIONAL entries; NULL target validates metadata only. */
    const TZrUInt32 *ownerStates;
    TZrUInt32 ownerStateCount;
} SZrExecIrResumeRequest;

ZR_CORE_API void ZrCore_ExecIr_StateMapInit(SZrExecIrStateMap *map);
ZR_CORE_API void ZrCore_ExecIr_StateMapFree(SZrExecIrStateMap *map);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapStorageValid(const SZrExecIrStateMap *map);
/* Validates identity and every supplied checkpoint; does not require coverage
 * of every possible boundary in the function. */
ZR_CORE_API TZrBool ZrCore_ExecIr_ValidateStateMap(
        const SZrExecIrFunction *function, const SZrExecIrStateMap *map,
        SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapClone(const SZrExecIrStateMap *source,
                                                SZrExecIrStateMap *destination);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapBoundaryFlags(
        const SZrExecIrInstruction *instruction,
        TZrUInt32 *flags);
ZR_CORE_API const SZrExecIrStateMapEntry *ZrCore_ExecIr_StateMapFind(
        const SZrExecIrStateMap *map,
        TZrExecIrSourceId sourceId,
        TZrUInt32 resumeId,
        EZrExecIrStateMapPhase phase);
ZR_CORE_API void ZrCore_ExecIr_MaterializedStateInit(SZrExecIrMaterializedState *state);
ZR_CORE_API void ZrCore_ExecIr_MaterializedStateFree(SZrExecIrMaterializedState *state);
ZR_CORE_API TZrBool ZrCore_ExecIr_MaterializeState(
        const SZrExecIrResumeRequest *request,
        SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn void ZrCore_ExecIr_StateMapInit(SZrExecIrStateMap *map)
 *
 * ## Summary
 * 将尚未拥有资源的 Core state-map 记录初始化为空值状态，供构造新 map 或准备一个新 Clone destination 时调用。
 *
 * ## Parameters
 * @param[out] map 可为 NULL；非空时须指向可写 map 记录。
 *
 * ## Returns
 * 无 C 返回值；非空调用通过清零整条记录交付空状态，NULL 调用无操作。
 *
 * ## Constraints and lifecycle
 * 此接口不释放已有池，只能用于新鲜、零初始化或已释放记录。对仍拥有池的 map 调用会遗失池指针并泄漏资源。Core 不为同一 map 提供并发同步。
 *
 * ## References
 * 实现位于 exec_ir_state_map_storage.c；function clone 的新记录初始化见 exec_ir.c 443 行，Clone 的临时候选初始化见实现 188 行。
 */

/**
 * @fn void ZrCore_ExecIr_StateMapFree(SZrExecIrStateMap *map)
 *
 * ## Summary
 * 释放 Core state-map 记录的 entries、value、root、owner-state 池并清零记录，用于销毁或回收 map 所有权。
 *
 * ## Parameters
 * @param[in,out] map 可为 NULL；非空时须指向已初始化、可写记录，且每个非空池地址均可独立释放。
 *
 * ## Returns
 * 无 C 返回值；完成后记录全零，可再次 Free。NULL 输入无操作。
 *
 * ## Constraints and lifecycle
 * 调用方负责保证池指针有效且彼此独立；本接口不校验形状、分配来源或别名。Free 完成后旧池指针及其元素借用失效；map 记录本身继续有效且已清零。无内部锁。
 *
 * ## References
 * 实现位于 exec_ir_state_map_storage.c；Core function 清理在 exec_ir.c 181–183 行，Clone 失败候选及旧 destination 的释放在实现 206、213 行。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapClone(const SZrExecIrStateMap *source, SZrExecIrStateMap *destination)
 *
 * ## Summary
 * 深拷贝 Core state-map 的逻辑内容，供函数/投影复制等需要独立侧表所有权的调用方使用；不会共享四个池。
 *
 * ## Parameters
 * @param[in] source 非空源 map；当其地址不同于 destination 时，存储形状须有效且池在调用期间保持可读。
 * @param[in,out] destination 非空目标记录；同址路径不读取或释放池。地址不同于 source 时，目标须已初始化、现有池可独立释放，两个记录对象及源/目标池容量区间须互不重叠。
 *
 * ## Returns
 * @return source 与 destination 是同一记录地址时直接返回 ZR_TRUE，且不校验、不分配、不修改。其他情况下仅当形状检查及四池复制全部成功才返回 ZR_TRUE；参数为空、形状无效、容量区间重叠或分配失败返回 ZR_FALSE。失败时源和目标保持不变。
 *
 * ## Constraints and lifecycle
 * 以下资源、并发与替换约束适用于 source 与 destination 地址不同的路径。成功复制 functionToken、signatureHash、generation 和各池逻辑 count 个元素，destination 的容量变为对应 count；generation 不递增。所有复制在局部候选完成后才替换并释放旧 destination，失败候选由实现清理；成功时旧目标池的借用地址立即失效。接口按容量检查池区间，但不验证 map 的语义内容，也不检查记录对象重叠；不同地址的 source/destination 记录本身必须互不重叠。调用期间 source 记录及池不得被并发修改或释放；destination 须由调用方独占写入和释放，旧目标池借用须停止使用；Core 不提供内部同步。
 *
 * ## References
 * 实现位于 exec_ir_state_map_storage.c；直接生产调用包括 exec_ir.c 444 行与 exec_ir_lower_aot.c 74 行，fault-injection 测试由 tests/parser/test_ssa_state_map_clone_fault_cases.h 调用。
 */
