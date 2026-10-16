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

/**
 * @struct SZrExecIrMaterializedState
 *
 * ## Summary
 * 保存从一个 state-map checkpoint 复制出的逻辑恢复元数据及调用方拥有的池；它不是运行时值对象或直接的 GC 保活对象。
 *
 * ## Fields
 * sourceId、resumeId、phase、cleanupState、boundaryFlags 和 generation 共同描述选中的 checkpoint 及其清理/边界上下文；effectIn/effectOut、handlerBlockId、exceptionState 与 deoptId 携带 effect、异常和 deopt 元数据。该类型本身不含 functionToken 或 signatureHash；身份检查来自 ResumeRequest 与匹配的 function/map。
 * values/valueCount/valueCapacity、roots/rootCount/rootCapacity、ownerStates/ownerStateCount/ownerStateCapacity、aggregates/aggregateCount/aggregateCapacity、aggregateFields/aggregateFieldCount/aggregateFieldCapacity 分别组成五组池。ownerStates 依 values 的相同索引描述 owner 状态；roots 是独立的逻辑 ID 清单，不能将 root 下标直接当作 ownerStates 下标。
 *
 * ## Invariants and lifecycle
 * 每组 count 必须不大于对应 capacity；capacity 为零时指针必须为 NULL，capacity 非零时指针必须非 NULL。values 与 roots 是逻辑 TZrExecIrValueId 清单，roots 是根 ID 列表而非对象指针；ownerStates 是 owner-state 元数据，aggregates 与 aggregateFields 是 deopt recipe，不是普通 ValueId 数组。ID 和 checkpoint 元数据须与产生它们的匹配 function/map/generation 上下文配合解释；ID 不延长 function/map 或运行时对象的生命周期。五组非空池须来自彼此分离、可独立由 C free 释放的分配，记录与池的存储由调用方管理。Init 只清零无资源记录；Free 释放五池并清零字段，但不释放记录本身。Free 或成功替换后旧池元素借用失效；Free 后记录为空，成功替换后记录承载新结果，记录地址继续有效。该结构不提供内部同步。
 *
 * ## References
 * ZrCore_ExecIr_MaterializeState 在 exec_ir_materialize.c 728 行初始化局部候选、819 行发布结果；interpreter resume 在 exec_ir_interpreter_resume.c 315–321 行读取逻辑 ID 与对应 owner 状态来核对暂停状态。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapStorageValid(const SZrExecIrStateMap *map)
 *
 * ## Summary
 * 对外检查 state-map 四个池的容量存储形状，供 Core 函数克隆形状预检、AOT 逻辑 map 验证与 Parser projection 使用；StateMapClone 使用相同的内部 shape 契约。
 *
 * ## Parameters
 * @param[in] map 借用的 map；调用期间记录和池保持稳定。
 *
 * ## Returns
 * @return 仅当计数、容量、NULL 配对、字节尺寸和四池区间形状均有效时返回 ZR_TRUE；否则返回 ZR_FALSE，不生成诊断。
 *
 * ## Constraints and lifecycle
 * 该接口只检查存储形状，不验证 checkpoint 语义、池内存实际可读性或释放所有权，不分配、不修改输入。无内部同步。
 *
 * ## References
 * 实现位于 exec_ir_state_map_storage.c 152 行；生产调用包括 exec_ir.c 337 行、aot_ir.c 305 行、exec_ir_lower_aot.c 70 行及 exec_ir_projection_common.c 713 行。
 */

/**
 * @fn void ZrCore_ExecIr_MaterializedStateInit(SZrExecIrMaterializedState *state)
 *
 * ## Summary
 * 初始化无资源的 materialized-state 记录，以便由后续 materialization 写入或由 Free 清理。
 *
 * ## Parameters
 * @param[out] state 可为空；非空时须指向可写记录。
 *
 * ## Returns
 * 无 C 返回值；非空记录清零，NULL 输入无操作。
 *
 * ## Constraints and lifecycle
 * 函数不先释放已有池，调用方只能传入新鲜、零初始化或已释放的记录；否则旧池指针会丢失并泄漏。无内部同步。
 *
 * ## References
 * 实现位于 exec_ir_state_map_storage.c 218 行；MaterializeState 与 interpreter resume 的初始化调用分别位于 exec_ir_materialize.c 728 行和 exec_ir_interpreter_resume.c 308 行。
 */

/**
 * @fn void ZrCore_ExecIr_MaterializedStateFree(SZrExecIrMaterializedState *state)
 *
 * ## Summary
 * 释放 materialized-state 的五组池并清空记录字段，结束当前池借用生命周期。
 *
 * ## Parameters
 * @param[in,out] state 可为空；非空时须为可写记录，且五组非空池均须可独立由 C free 释放。
 *
 * ## Returns
 * 无 C 返回值；完成后记录为零状态，NULL 输入无操作。
 *
 * ## Constraints and lifecycle
 * Free 不验证池形状或别名；调用方负责确保各池独立且可传给 C free。释放后池指针及指向池元素的借用失效，state 记录本身仍有效且为空。无内部同步。
 *
 * ## References
 * 实现位于 exec_ir_state_map_storage.c 224 行；MaterializeState 回滚和替换路径位于 exec_ir_materialize.c 764 行与 818 行。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapBoundaryFlags(const SZrExecIrInstruction *instruction, TZrUInt32 *flags)
 *
 * ## Summary
 * 对外提供 state-map 边界位分类，供 Parser checkpoint builder 和 Core validator 共用。
 *
 * ## Parameters
 * @param[in] instruction 调用期间可读的 IR 指令。
 * @param[out] flags 独立可写的输出槽；不得与 instruction 存储重叠。
 *
 * ## Returns
 * @return 两参数非空、opcode schema 可查询且计算所得边界位非零时返回 ZR_TRUE；其他情况返回 ZR_FALSE。
 *
 * ## Constraints and lifecycle
 * 任一参数为空时在清零前返回 FALSE；两参数均非空后先清零。未知 opcode 或已知但无边界的 opcode 返回 FALSE 且 flags 为零。对 MAY_ALLOCATE、MAY_GC、MAY_THROW、MAY_SUSPEND，指令或 schema 任一来源具备相应属性就设置对应边界位。DEBUG_POLL 仅看指令标志；GUARD_EXIT 接受显式指令标志或非零 deoptId；DEOPT 由非零 deoptId 贡献；DROP/DROP_IF_INITIALIZED 设置 CLEANUP。instruction 只在调用期间借用，不保存；无内部同步，也不检查别名。
 *
 * ## References
 * 声明：zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:138；实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:91；Core 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:467；Parser 构造调用：zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:402。
 */
/**
 * @fn const SZrExecIrStateMapEntry *ZrCore_ExecIr_StateMapFind(const SZrExecIrStateMap *map, TZrExecIrSourceId sourceId, TZrUInt32 resumeId, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 按源码点 ID、恢复 ID 和逻辑 phase 查询首个 state-map entry。
 *
 * ## Parameters
 * @param[in] map 待查询的映射；仅轻量检查数组描述符。
 * @param[in] sourceId 精确匹配值；本接口不额外禁止零值。
 * @param[in] resumeId 非零恢复 ID。
 * @param[in] phase BEFORE_EFFECT、AFTER_EFFECT 或 CLEANUP_COMPLETE。
 *
 * ## Returns
 * @return 返回只读 entry 借用指针；轻量描述符检查失败或未命中返回 NULL，重复三元组返回数组首项。
 *
 * ## Constraints and lifecycle
 * 本接口不代替 StateMapStorageValid 或 ValidateStateMap，不检验完整 checkpoint 语义、底层真实分配 extent 或 entry 唯一性。返回指针直接别名 map.entries 中的元素，不是快照；调用方须在读取期间保持 map 描述符、entries 数组及对应条目稳定。修改条目会改变借用所见内容；替换、重分配或释放 entries 后借用指针失效。接口不转移所有权、不保存输入、无内部同步。
 *
 * ## References
 * 声明：zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:141；实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:139；Core 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:688；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c:38；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:117。其他直接测试入口见 tests/parser/ssa_state_map_verifier_cases.inc:60；tests/parser/ssa_pass_manager_state_maps_cases.inc:129；tests/parser/ssa_pass_manager_state_maps_cases.inc:146；tests/parser/test_ssa_state_maps.c:230；tests/parser/test_ssa_state_maps.c:235；tests/parser/test_ssa_state_maps.c:237；tests/parser/test_ssa_state_maps.c:843；tests/parser/test_ssa_state_maps.c:845；tests/parser/test_ssa_state_maps.c:1198；tests/parser/test_ssa_state_maps.c:1200；tests/parser/test_ssa_state_maps.c:1202；tests/parser/test_ssa_state_maps.c:1204；tests/parser/test_ssa_retained_builder_cfg.c:100；tests/parser/test_ssa_deopt_aggregates.c:116。
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ValidateStateMap(const SZrExecIrFunction *function, const SZrExecIrStateMap *map, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 对外提供 StateMap 相对指定 function 当前结构与 identity 的同步验证入口。
 *
 * ## Parameters
 * @param[in] function 可为空的验证基准 function；空值返回 ZR_FALSE。
 * @param[in] map 可为空的显式待验证映射；空值返回 ZR_FALSE；非空时允许不是 function.stateMap，但映射身份须匹配该 function。
 * @param[out] diagnostic 可为空；非空时先清零，之后按失败阶段写入诊断。
 *
 * ## Returns
 * @return function/map 非空且结构、身份和所有已提供 checkpoint 均通过验证时返回 ZR_TRUE，否则返回 ZR_FALSE；任一输入为空时，在非空 diagnostic 中记录 INVALID_ARGUMENT。
 *
 * ## Constraints and lifecycle
 * 不要求 map 覆盖 function 中所有潜在边界。function、map 及嵌套数组是调用期间的只读借用，调用方负责保持稳定可读；Core 不保存输入且不提供同步。diagnostic 必须可写且与两个输入及其嵌套存储不重叠，因为实现清零诊断早于读取输入。临时 owner/liveness 分析可能分配，调用返回前会按相应成功/失败路径清理。
 *
 * ## References
 * 实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556；NULL 拒绝与入口清零：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:565、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:566；临时分析释放：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:657、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:661。生产调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:910。测试直接调用：tests/parser/test_ssa_sccp_phi_copy_edges.c:256；tests/parser/test_ssa_retained_builder_cfg.c:60；tests/parser/test_ssa_licm_scalar_context.inc:136；tests/parser/test_ssa_licm_scalar_context.inc:202。这些引用不授予测试单元审查信用。
 */
