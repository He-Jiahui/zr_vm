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

/**
 * @fn TZrBool ZrCore_ExecIr_MaterializeState(const SZrExecIrResumeRequest *request, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 同步验证并选择 ExecIR 的逻辑 checkpoint，供解释器恢复或后续对象物化使用。request->target 为 NULL 时只验证 metadata 与适用的 owner witness；非空时先准备全部逻辑结果，再一次替换旧 target。
 * 该接口返回逻辑 ID、owner 状态和 deopt recipe，不构造运行时对象，也不注册或保活 GC root。
 *
 * ## Parameters
 * @param[in] request 调用期间借用的请求描述符；NULL 或其 function 为 NULL 时失败，未知 phase 也失败。map 可为 NULL，以选用 function->stateMap；非空输入及嵌套数组必须真实有效且在读取期间稳定。target 是该只读描述符中指向独立可写输出的借用地址。
 * @param[out] diagnostic 可选的独立可写诊断对象；NULL 表示不接收诊断。非空时在 request 检查前先清零，之后可能收到失败诊断；不得与请求、function/map、witness 或 target 及其池重叠。
 *
 * ## Returns
 * @return 验证所选 checkpoint 和 owner witness 成功返回 ZR_TRUE；target 非空时还表示候选已提交。失败返回 ZR_FALSE；在有效、独立存储及旧池所有权前提下，旧 target 记录及其池保持可用，diagnostic 可能改变。空 target 的成功不产生物化输出，也不保证调用中没有临时分配。
 *
 * ## Constraints and lifecycle
 * 先完整验证 function/map 对，再按 sourceId/resumeId/phase 查找 entry，最后比较请求中非零的 functionToken、generation、signatureHash。零只跳过对应的额外请求身份比较；不会放宽 function/map 本身的 token、generation、signature 一致性要求。
 * ownerStates 为 NULL 时 ownerStateCount 必须为零；非空 witness 按 valueId - 1 索引，数量必须等于 function->valueCount，且每个被使用的状态须符合 checkpoint 的静态 owner mask。非空 target 所选 entry 中的 CONDITIONAL owner 状态需要具体 witness；空 target 可以仅验证此类 metadata。
 * 非空 target 应先用 ZrCore_ExecIr_MaterializedStateInit 或等价方式零初始化，或者持有满足五池形状、分离关系且可由 C free 独立释放的旧结果。提交会释放旧池并使其借用失效，随后把新池所有权交给 target；调用方最终用 ZrCore_ExecIr_MaterializedStateFree 释放。
 * target 校验不证明 allocation provenance、地址可读性或 target 记录本身对输入的分离；request、diagnostic 与 target 记录须相互独立；target 记录及旧池还须与 request、function/map、被读池和 owner witness 分离，这是调用方前提。不能把提交后仍需读取的请求或输入置于旧 target 池内。本接口不承诺任意别名安全。
 * 请求、function/map 和 witness 只在同步调用中借用，不被保存或保活。结果池拥有其复制的元数据存储，但逻辑 ID 和 token 的含义仍依赖对应函数版本；它们不会延长运行时对象或函数的生命周期。调用方负责避免并发修改或释放输入及 target，本接口没有内部锁。
 *
 * ## References
 * 实现、早退、空输出分支与提交点：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:666；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:674；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:718；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 * 配套输出类型和请求类型：zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:82；zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:111；初始化及释放实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:218；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:224；target 有限区间检查：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:308。
 * 解释器先验证再物化：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c:31；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c:80；对象物化的局部 logical 候选：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_objects.c:684。
 * 有限未知 phase 早退断言与条件 witness 场景：tests/parser/test_ssa_state_maps.c:242；tests/parser/test_ssa_conditional_cleanup.c:268；tests/parser/test_ssa_conditional_cleanup.c:293。
 */

/**
 * @enum EZrExecIrStateMapPhase
 *
 * ## Summary
 * 标识逻辑 checkpoint 相对指令副作用的位置；与 sourceId/resumeId 一起选择条目，不描述物理帧或寄存器。
 *
 * ## Values
 * - BEFORE_EFFECT（0）：指令副作用之前；对应 cleanupState 0。
 * - AFTER_EFFECT（1）：指令副作用之后；对应 cleanupState 1。
 * - CLEANUP_COMPLETE（2）：清理完成阶段的 checkpoint；对应 cleanupState 2。Builder 仅在 THROW、SUSPEND 或 CLEANUP 边界补此阶段，完整校验也限制该阶段的边界种类。
 * - PHASE_COUNT（3）是上界哨兵，不是可恢复阶段；负值及其他未知枚举值也不合法。
 *
 * ## Constraints and lifecycle
 * 这是互斥阶段枚举，不是位掩码。完整校验允许同一 sourceId/resumeId 在不同阶段各有一个条目，但拒绝相同三元组重复。当前 ownership/liveness 查询把非 BEFORE 的两个阶段都按 after 数据处理；CLEANUP_COMPLETE 不单独证明运行时对象已释放。枚举自身不拥有资源、不延长 checkpoint、函数或池的借用期限，也不提供同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:7
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:79
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:492
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:496
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:646
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:447
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:461
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC
 *
 * ## Summary
 * 标识指令可能触发 GC 的逻辑边界。
 *
 * ## Value
 * TZrUInt32 的第 0 位；值为 1u << 0u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在已知 opcode 上分别读取指令 MAY_GC 标志与 opcode schema 的 MAY_GC 属性，任一成立即加入此位。
 *
 * ## Constraints and lifecycle
 * 它描述可能的 GC 边界，不证明此次执行发生收集，也不表示 rootPool 已注册成运行时 GC 根。GC 与 ALLOCATE 等位独立组合。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:14
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:109
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:65
 * - tests/parser/test_ssa_state_maps.c:311
 * - tests/parser/test_ssa_state_maps.c:325
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_THROW
 *
 * ## Summary
 * 标识指令可能抛异常的逻辑边界。
 *
 * ## Value
 * TZrUInt32 的第 1 位；值为 1u << 1u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在已知 opcode 上从指令 MAY_THROW 标志或 opcode schema 的 MAY_THROW 属性产生此位；它同时属于 EXCEPTION_MASK。
 *
 * ## Constraints and lifecycle
 * 此位允许 Builder 补 CLEANUP_COMPLETE checkpoint，并参与 handler 选择与 exceptionState 编码；不是本次已抛异常的标志。handler 按 successor 数组顺序选首个有效 EXCEPTION/CLEANUP 目标；未找到时双方 INVALID 可相等通过，并非强制存在 handler。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:15
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:113
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:419
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:452
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:456
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:70
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_SUSPEND
 *
 * ## Summary
 * 标识指令可能挂起的逻辑边界。
 *
 * ## Value
 * TZrUInt32 的第 2 位；值为 1u << 2u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在已知 opcode 上从指令 MAY_SUSPEND 标志或 opcode schema 的 MAY_SUSPEND 属性产生此位；它同时属于 EXCEPTION_MASK。
 *
 * ## Constraints and lifecycle
 * Builder 可据此生成 CLEANUP_COMPLETE checkpoint，live ValueId 的跨挂起借用限制由 Builder/完整校验另行检查。此位不执行挂起、不保存运行时对象，也不替代匹配 exceptionState 或具体 owner 见证。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:117
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:494
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:505
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:265
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:457
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:66
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEOPT
 *
 * ## Summary
 * 标识附带非零 deoptId 的逻辑反优化边界。
 *
 * ## Value
 * TZrUInt32 的第 3 位；值为 1u << 3u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在确认 opcode 有效后，仅在 instruction.deoptId 非零时加入此位，并由同一条件同时加入 GUARD_EXIT。
 *
 * ## Constraints and lifecycle
 * 完整 checkpoint 校验要求该位对应非零且匹配指令的 deoptId，另有 deopt 内容/活性校验。位值本身不证明 recipe 有效，也不构造对象或执行反优化；GUARD_EXIT 单独存在不反推 DEOPT。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:17
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:124
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:128
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:499
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:544
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:75
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_ALLOCATE
 *
 * ## Summary
 * 标识指令可能分配内存的逻辑边界。
 *
 * ## Value
 * TZrUInt32 的第 4 位；值为 1u << 4u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在已知 opcode 上从指令 MAY_ALLOCATE 标志或 opcode schema 的 MAY_ALLOCATE 属性产生此位。
 *
 * ## Constraints and lifecycle
 * 分配边界与 GC、THROW 等边界分别计算，不因该位自动推定其他位存在；它自身不进行分配或给出分配成功保证。ExecBC fusion 将它纳入 safepoint 边界分类。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:18
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:105
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:67
 * - tests/parser/test_ssa_state_maps.c:181
 * - tests/parser/test_ssa_state_maps.c:1193
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_DEBUG_POLL
 *
 * ## Summary
 * 标识指令动态请求的调试轮询边界。
 *
 * ## Value
 * TZrUInt32 的第 5 位；值为 1u << 5u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在确认 opcode 有效后，仅从 instruction.flags 的 DEBUG_POLL 位产生此位，不从 opcode schema 推导。
 *
 * ## Constraints and lifecycle
 * Builder 可围绕此逻辑边界记录 before/after checkpoint，ExecBC fusion 将它归入 debug 边界。此位不执行调试器轮询，也不单独许可 CLEANUP_COMPLETE 阶段。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:19
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:121
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:62
 * - tests/parser/test_ssa_state_maps.c:197
 * - tests/parser/test_ssa_state_maps.c:1202
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_GUARD_EXIT
 *
 * ## Summary
 * 标识显式 guard-exit 或带 deoptId 的逻辑退出边界。
 *
 * ## Value
 * TZrUInt32 的第 6 位；值为 1u << 6u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在确认 opcode 有效后，若 instruction.flags 含 GUARD_EXIT 或 instruction.deoptId 非零时加入此位；后者也产生 DEOPT。
 *
 * ## Constraints and lifecycle
 * 独立 GUARD_EXIT 可在 deoptId 为零时出现，不能据此推定存在 deopt recipe 或本次实际退出。ExecBC fusion 将它与 DEOPT 一起归入 reentrant 边界。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:20
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:124
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:128
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:76
 * - tests/parser/test_ssa_state_maps.c:665
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_CLEANUP
 *
 * ## Summary
 * 标识 DROP 或 DROP_IF_INITIALIZED 指令的逻辑清理边界。
 *
 * ## Value
 * TZrUInt32 的第 7 位；值为 1u << 7u，可与其他 boundary 位按位或。
 *
 * ## Construction and consumers
 * StateMapBoundaryFlags 在确认 opcode 有效后，仅在 opcode 为 DROP 或 DROP_IF_INITIALIZED 时加入此位；它不是 CFG successor 的 CLEANUP 标志。
 *
 * ## Constraints and lifecycle
 * Builder 可据此增加 CLEANUP_COMPLETE checkpoint。该位不会执行 drop，也不能证明某 owner 已是 DROPPED：同一指令的 BEFORE checkpoint 也带该边界集合，状态须结合 phase 与 ownerStates 判断。ExecBC fusion 将它纳入 exception 边界。 常量本身没有输入、返回值、分配或资源寿命；entry 合法性仍由匹配指令、phase、身份与完整校验共同决定。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:21
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:131
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:495
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:458
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_match.c:71
 * - tests/parser/test_ssa_state_maps.c:630
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_BOUNDARY_KNOWN_MASK
 *
 * ## Summary
 * 列出本 schema 认识的全部八种 boundary 位，供完整 entry 校验与 ExecBC fusion 合同拒绝未知位。
 *
 * ## Value
 * GC、THROW、SUSPEND、DEOPT、ALLOCATE、DEBUG_POLL、GUARD_EXIT、CLEANUP 的按位并集，即 TZrUInt32 的低八位（0xFF）。
 *
 * ## Constraints and lifecycle
 * KNOWN_MASK 只界定已知位集合。entry.boundaryFlags 还必须非零，并与该指令实际查询所得的集合完全相同；已知位的任意组合不自动构成合法 checkpoint。它不校验阶段、handler、identity、范围或 ownership，不分配资源，也不提供运行时保活或同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:22
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:484
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:485
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:490
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc/exec_ir_fusion_contract.c:301
 */

/**
 * @def ZR_EXEC_IR_STATE_MAP_EXCEPTION_MASK
 *
 * ## Summary
 * 提取 checkpoint 的 THROW/SUSPEND 边界分类，用于 entry.exceptionState 的一致编码。
 *
 * ## Value
 * BOUNDARY_THROW | BOUNDARY_SUSPEND，即 TZrUInt32 的第 1、2 位（0x06）。
 *
 * ## Construction and consumers
 * Builder 令 entry.exceptionState = boundaryFlags & EXCEPTION_MASK；完整 entry 校验要求精确相等。这是两种可能边界的位集合，不是当前异常对象、异常码或运行时已发生异常的布尔值。
 *
 * ## Constraints and lifecycle
 * SUSPEND 也属于此集合，但 handler 是否需要匹配只由 THROW 分支与 successor 搜索决定；不能从 exceptionState 非零推定存在 handler。该 mask 不替代完整 boundary/phase/identity/ownership 校验，不拥有资源或延长借用寿命。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:27
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:296
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:486
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:452
 */

/**
 * @enum EZrExecIrStateMapOwnerState
 *
 * ## Summary
 * 记录逻辑 ValueId 在 checkpoint 的 ownership 状态；它描述可用性与清理分类，不是宿主地址或运行时对象本身。
 *
 * ## Values
 * - UNKNOWN（0）：ownership 分类为 UNKNOWN 的值可使用的状态，不等同于 UNINITIALIZED。
 * - INITIALIZED（1）：已初始化；MOVED（2）、DROPPED（3）、UNINITIALIZED（4）分别区分已移出、已丢弃和尚未初始化。
 * - CONDITIONAL（5）：静态分析在合流点保留的条件分类，不是具体执行见证。StateMapOwnerAt 还结合语义存活与 UNIQUE/SHARED ownership 限制决定是否能编码此类状态。
 * - OWNER_STATE_COUNT（6）是无效/上界哨兵，不得写成有效状态；这些整数也不是 ownership bitmask。
 *
 * ## Constraints and lifecycle
 * 条目 ownerStates 范围与 liveValues 一一对应，按 liveValues 的顺序索引；ResumeRequest.ownerStates 则按 valueId - 1 索引，二者布局不同。实际输出 CONDITIONAL 条目时必须提供相容的具体见证，相关值的见证状态须小于 CONDITIONAL；target 为 NULL 且不提供见证时可仅验证静态条件元数据。物化后的 roots 只保留解析为 INITIALIZED 的值。状态码及见证不会证明 payload 可读、取得对象所有权或注册 GC 根；这些职责属于运行时调用方。读取期间关联函数、状态图及见证须保持有效，无内部同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:31
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:218
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:445
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:290
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:108
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:113
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:121
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:133
 */

/**
 * @struct SZrExecIrStateMapEntry
 *
 * ## Summary
 * 保存一个指令在一个逻辑阶段的 checkpoint 描述；条目只含标量身份和范围，不内嵌运行时值、对象或独立分配。
 *
 * ## Fields
 * - sourceId/instructionId/resumeId/phase 绑定源码身份、1-based 指令编号、恢复身份与逻辑阶段；sourceId 使用指令 sourceId，指令未标注时回退为 instructionId。完整校验要求 sourceId、resumeId、instructionId 非零。
 * - deoptId/effectIn/effectOut 必须与对应指令相符；cleanupState 按 BEFORE/AFTER/CLEANUP_COMPLETE 分别为 0/1/2。
 * - boundaryFlags 必须非零、无未知位，并等于查询所得的该指令边界集合；exceptionState 精确保存 boundaryFlags & EXCEPTION_MASK，不是异常对象或运行结果。
 * - handlerBlockId 以 THROW 边界与 CFG successor 数组顺序确定首个有效 EXCEPTION 或 CLEANUP 目标；无 THROW 时为 INVALID。THROW 时若未找到目标，expected 与条目双方为 INVALID 也可相等通过。
 * - liveValues 索引 map.valuePool，rootValues 索引 map.rootPool，ownerStates 索引 map.ownerStatePool；ownerStates.count 等于 liveValues.count，其第 i 项对应第 i 个 live ValueId。
 *
 * ## Invariants
 * 完整校验分别要求 live/root 清单无重复，roots 是 live 的子集，根值 ownership 可为 GC、UNIQUE 或 SHARED，并检查所有 supplied checkpoint 的 ownership/liveness/deopt 约束。同一 instruction 必须固定 source/resume，同一 resume 必须固定 instruction/source；允许同一身份的不同 phase，禁止重复 (sourceId,resumeId,phase)，不要求 sourceId 全局唯一。单独构造、复制条目或 StateMapFind 命中都不等于完成这些校验。
 *
 * ## Constraints and lifecycle
 * 字段中的 ID/range 只能与匹配 function/map/generation 上下文解释；roots 是逻辑 ID，不是已经注册的运行时 GC 根。Find 返回 entries 元素的借用地址；字段修改立即改变所见元数据，entries 被替换、重分配或释放后旧条目借用失效。复制条目只复制范围及身份，不复制其引用的池，也不延长池寿命；条目本身没有单独 Free。读取期元数据须稳定，无内部同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:42
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:155
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:410
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:447
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:475
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:521
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:532
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:624
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:37
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:282
 */

/**
 * @struct SZrExecIrStateMap
 *
 * ## Summary
 * 承载逻辑 checkpoint 表及其四个池；exec_ir.h 提供同名不透明 typedef，本处给出记录布局。它是 function identity/epoch 下的逻辑侧表，不是可执行帧或物理 GC root map。
 *
 * ## Fields
 * - functionToken/signatureHash/generation 绑定所选 function 的身份、签名及 contract.generation；完整校验要求非零 functionToken 匹配，并比较签名和 generation。
 * - entries/entryCount/entryCapacity 保存 checkpoint；valuePool/valueCount/valueCapacity 保存 live ValueId；rootPool/rootCount/rootCapacity 保存 root ValueId；ownerStatePool/ownerStateCount/ownerStateCapacity 保存与 live 顺序对应的状态码。
 * - 四组 count 不大于 capacity；capacity 为零当且仅当对应指针为 NULL，count 为零可仍保留非零容量。容量乘元素大小须可表示，四个容量区间不得重叠。
 *
 * ## Invariants
 * StorageValid 只检查上述存储形状与区间，不能证明地址真实可读、分配来源、各池可独立 free 或 checkpoint 语义。完整 ValidateStateMap 还校验 identity、entry、CFG/ownership/liveness 等条件，但不要求表覆盖函数全部可能边界。Init 后的全零记录可作为构造起点；是否能给特定函数使用仍取决于完整校验。
 *
 * ## Ownership and lifecycle
 * - 普通独立记录的池由该记录的唯一所有者管理；StateMapFree 要求四个非空池均兼容 C free 且可独立释放。它释放池并清零记录，记录对象地址本身仍有效；函数释放路径还会释放其 heap map 记录。
 * - 记录也可嵌于 Parser AOT projection，或由 Core AOT owned-module 将记录与池打包在单一 arena。AOT function.logicalStateMap 是模块寿命内的借用，不得对 arena 内的借用记录调用 StateMapFree；相应 owner 统一释放或成功替换后借用终止。
 * - 浅拷贝记录会共享池，不能据此创建两个独立释放者。StateMapClone 异址成功复制 used counts 及身份元数据，并在全部准备成功后替换目标池；不做 checkpoint 语义校验，也不增加 generation。
 * - BuildStateMaps 成功可在 function pools 和 contract.generation 不变时替换旧 map 记录。旧 map/entry/pool 借用会因所属 owner 的替换、重分配或释放失效；generation 相同不保活旧地址。Init 不释放已有资源，不能用它覆盖仍拥有池的记录。
 *
 * ## Constraints and lifecycle
 * 所有借用者须按实际 owner 的寿命使用记录和池；读操作期间源记录/池不得被修改或释放，写入和释放须由调用方协调。此结构体没有引用计数、内部同步、payload 所有权或运行时 GC 保活能力。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:61
 * - zr_vm_core/include/zr_vm_core/exec_ir.h:62
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:75
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:162
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_storage.c:189
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:575
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c:181
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:489
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c:36
 * - zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c:143
 * - zr_vm_core/src/zr_vm_core/aot/aot_ir_owned_module.c:268
 */

/**
 * @struct SZrExecIrResumeRequest
 *
 * ## Summary
 * 借用 function、可选显式 map 和 checkpoint 身份，向 Core 逻辑物化入口提交查询/输出请求；backend 的 opaque 回调也携带此记录，但 backend 调度层不会代替 Core 校验状态图。
 *
 * ## Fields
 * - function 在 Core MaterializeState 中必须非空；map 为 NULL 时选择 function.stateMap，否则使用显式 map。所选 map 必须先通过与 function 的完整校验，之后才按 sourceId/resumeId/phase 查找 checkpoint。
 * - functionToken、generation、signatureHash 是额外身份检查：各字段非零时分别要求匹配 function/map；零只跳过该项 request 检查，不免除 map 与 function 的 identity/epoch 校验。sourceId/resumeId/phase 组成查找键。
 * - target 为 NULL 时只验证 checkpoint/ownership 元数据，不发布 MaterializedState；该路径仍可为校验分配临时分析内存。非空 target 指向调用方独占的、已初始化或持有可正常释放旧池的结果记录。
 * - ownerStates 为可选具体 ownership 见证，按 valueId - 1 索引，与 map 内按 live 顺序编码的 ownerStates 范围不同。NULL 与 count 0 必须配对；非空时 count 必须等于 function.valueCount。入口只检查当前 checkpoint 所需 live ValueId 的具体状态，不承诺逐项验证未被读取的见证槽。
 *
 * ## Invariants
 * 具体见证在被使用的 live ID 上须小于 CONDITIONAL、属于分析允许的状态集合，并与非条件 map 状态一致。实际输出遇到 CONDITIONAL 时必须有相容见证；target 为 NULL 且没有见证时可接受合法的静态条件元数据。见证只陈述状态，不证明实际 payload 存在，也不持有对象或注册 GC 根。
 *
 * ## Constraints and lifecycle
 * request、function、显式/回退 map 及被读池和见证在入口/回调执行期间都必须稳定有效，不转移源所有权。非空 target 的旧池须独立且兼容 C free；target 记录及旧池须与 request、function/map、被读池和见证分离；diagnostic 须独立可写且不得覆盖这些输入或 target/旧池，以维持只读输入和失败保留旧输出的前提；检查 helper 不证明分配来源，也不覆盖所有这些别名关系。Core 先清 diagnostic、完整校验与准备，再于唯一提交点释放旧 target 池并发布新结果；该类型自身不提供事务或同步。输出 values/roots 是逻辑 ID 清单，不延长源函数、map 或运行时对象寿命；成功替换会使旧 target 池借用失效，target 记录继续有效。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:111
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:674
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:684
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:697
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:718
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:95
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:113
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:121
 * - zr_vm_core/include/zr_vm_core/execution_backend.h:439
 * - zr_vm_core/src/zr_vm_core/execution/execution_backend.c:1632
 */
