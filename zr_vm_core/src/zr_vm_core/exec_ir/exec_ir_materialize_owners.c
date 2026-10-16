#include "exec_ir_materialize_owners.h"

#include <string.h>

static TZrBool zr_owners_size_valid(TZrUInt32 count, size_t elementSize) {
    return (TZrBool)(elementSize != 0u &&
            (TZrUInt64)count <= (TZrUInt64)(SIZE_MAX / elementSize));
}

static TZrBool zr_owners_fail(const SZrExecIrFunction *function,
                              const SZrExecIrStateMapEntry *entry,
                              SZrExecIrDiagnostic *diagnostic) {
    if (diagnostic != ZR_NULL) {
        TZrUInt32 index;
        memset(diagnostic, 0, sizeof(*diagnostic));
        diagnostic->code = ZR_EXEC_IR_DIAGNOSTIC_STATE_MAP_INVALID;
        diagnostic->functionToken = function->functionToken;
        diagnostic->instructionId = entry->instructionId;
        diagnostic->sourceId = entry->sourceId;
        for (index = 0u; index < function->blockCount; ++index) {
            SZrExecIrRange range = function->blocks[index].instructionRange;
            if (entry->instructionId - 1u >= range.start &&
                entry->instructionId - 1u - range.start < range.count) {
                diagnostic->blockId = function->blocks[index].id;
                break;
            }
        }
    }
    return ZR_FALSE;
}

TZrBool zr_state_map_owners_valid(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic) {
    TZrUInt32 index, at;
    if (entry->ownerStates.start > map->ownerStateCount ||
        entry->ownerStates.count > map->ownerStateCount - entry->ownerStates.start ||
        entry->ownerStates.count != entry->liveValues.count ||
        (entry->ownerStates.count != 0u && map->ownerStatePool == ZR_NULL))
        return zr_owners_fail(function, entry, diagnostic);
    for (at = 0u; at < entry->liveValues.count; ++at) {
        TZrExecIrValueId value = map->valuePool[entry->liveValues.start + at];
        if (value == 0u || value > function->valueCount)
            return zr_owners_fail(function, entry, diagnostic);
    }
    /* Every required value occurs exactly once. Conservative extra values
     * are safe only when statically available; an inactive or conditional
     * value must correspond to a real cleanup obligation. */
    for (index = 0u; index < function->valueCount; ++index) {
        TZrUInt32 occurrences = 0u, roots = 0u, slot = 0u;
        TZrBool live = ZrCore_ExecIr_StateMapLivenessContains(liveness,
                entry->instructionId, index + 1u,
                (TZrBool)(entry->phase != ZR_EXEC_IR_STATE_BEFORE_EFFECT));
        EZrExecIrOwnership kind = function->values[index].ownership;
        for (at = 0u; at < entry->liveValues.count; ++at) {
            if (map->valuePool[entry->liveValues.start + at] == index + 1u) {
                ++occurrences;
                slot = at;
            }
        }
        if ((live && occurrences != 1u) || (!live && occurrences > 1u))
            return zr_owners_fail(function, entry, diagnostic);
        if (occurrences != 0u) {
            EZrExecIrStateMapOwnerState expected = ZrCore_ExecIr_StateMapOwnerAt(
                    function, ownership, liveness, entry->instructionId, index + 1u, entry->phase);
            TZrUInt8 mask = ZrCore_ExecIr_StateMapOwnerMaskAt(
                    function, ownership, liveness, entry->instructionId, index + 1u, entry->phase);
            TZrBool rooted = (TZrBool)((kind == ZR_EXEC_IR_OWNERSHIP_GC ||
                    kind == ZR_EXEC_IR_OWNERSHIP_UNIQUE || kind == ZR_EXEC_IR_OWNERSHIP_SHARED) &&
                    (mask & ZR_EXEC_IR_OWNER_STATE_BIT(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED)) != 0u);
            if (expected == ZR_EXEC_IR_STATE_MAP_OWNER_STATE_COUNT ||
                (!live && expected != ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED &&
                          expected != ZR_EXEC_IR_STATE_MAP_OWNER_UNKNOWN) ||
                map->ownerStatePool[entry->ownerStates.start + slot] != (TZrUInt32)expected)
                return zr_owners_fail(function, entry, diagnostic);
            for (at = 0u; at < entry->rootValues.count; ++at)
                if (map->rootPool[entry->rootValues.start + at] == index + 1u) ++roots;
            if (roots != (rooted ? 1u : 0u)) return zr_owners_fail(function, entry, diagnostic);
        }
    }
    return ZR_TRUE;
}

TZrBool zr_state_map_resolve_owners(
        const SZrExecIrResumeRequest *request, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared,
        SZrExecIrDiagnostic *diagnostic) {
    SZrExecIrOwnerAnalysis ownership = {0};
    SZrStateMapLiveness liveness = {0};
    const SZrExecIrFunction *function = request->function;
    TZrUInt32 index;
    TZrBool valid = ZR_FALSE;
    EZrExecutionDiagnosticCode code;
    if (!zr_owners_size_valid(request->ownerStateCount, sizeof(*request->ownerStates)) ||
        (request->ownerStates == ZR_NULL) != (request->ownerStateCount == 0u) ||
        (request->ownerStates != ZR_NULL && request->ownerStateCount != function->valueCount))
        return zr_owners_fail(function, entry, diagnostic);
    if (request->ownerStates != ZR_NULL) {
        if (!ZrCore_ExecIr_OwnerAnalysisBuild(function, &ownership, diagnostic)) return ZR_FALSE;
        code = ZrCore_ExecIr_StateMapLivenessBuild(function, &liveness);
        if (code != ZR_EXECUTION_DIAGNOSTIC_NONE) {
            if (diagnostic != ZR_NULL) diagnostic->code = code;
            goto finish;
        }
    }
    for (index = 0u; index < entry->liveValues.count; ++index) {
        TZrExecIrValueId value = map->valuePool[entry->liveValues.start + index];
        TZrUInt32 state = map->ownerStatePool[entry->ownerStates.start + index];
        if (request->ownerStates != ZR_NULL) {
            TZrUInt8 mask = ZrCore_ExecIr_StateMapOwnerMaskAt(function, &ownership,
                    &liveness, entry->instructionId, value, entry->phase);
            TZrUInt32 concrete = request->ownerStates[value - 1u];
            if (concrete >= ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL ||
                (mask & ZR_EXEC_IR_OWNER_STATE_BIT(concrete)) == 0u ||
                (state != ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL && concrete != state)) {
                zr_owners_fail(function, entry, diagnostic);
                goto finish;
            }
            state = concrete;
        } else if (state == ZR_EXEC_IR_STATE_MAP_OWNER_CONDITIONAL && prepared != ZR_NULL) {
            zr_owners_fail(function, entry, diagnostic);
            goto finish;
        }
        if (prepared != ZR_NULL) prepared->ownerStates[index] = state;
    }
    if (prepared != ZR_NULL) {
        TZrUInt32 rootCount = 0u;
        for (index = 0u; index < prepared->rootCount; ++index) {
            TZrUInt32 live;
            for (live = 0u; live < prepared->valueCount; ++live) {
                if (prepared->values[live] == prepared->roots[index] &&
                    prepared->ownerStates[live] == ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED) {
                    prepared->roots[rootCount++] = prepared->roots[index];
                    break;
                }
            }
        }
        prepared->rootCount = rootCount;
    }
    valid = ZR_TRUE;
finish:
    ZrCore_ExecIr_OwnerAnalysisFree(&ownership);
    ZrCore_ExecIr_StateMapLivenessFree(&liveness);
    return valid;
}

/**
 * @fn static TZrBool zr_owners_size_valid(TZrUInt32 count, size_t elementSize)
 *
 * ## Summary
 * 在读取 owner witness 前检查元素个数与单元素字节数相乘是否能由 size_t 表示，供后续长度协议验证使用。
 *
 * ## Parameters
 * @param[in] count 待验证的元素个数；零元素也要服从 elementSize 非零的约束。
 * @param[in] elementSize 单元素的实际字节数；零值直接判为无效。
 *
 * ## Returns
 * @return elementSize 非零且 count 不大于 SIZE_MAX / elementSize 时返回 ZR_TRUE，否则返回 ZR_FALSE。零 count 配合非零 elementSize 返回真。
 *
 * ## Constraints and lifecycle
 * 只检查数值可表示性，不检查指针、实际分配 extent、可读性或 count 与 function.valueCount 的关系。
 * 调用是同步纯数值判断，不分配、不保存参数、不转移所有权；失败没有输出副作用。
 *
 * ## References
 * 唯一直接调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:95；指针/数量协议由同一调用者的 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:96 和 zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:97 另行检查。
 */

/**
 * @fn static TZrBool zr_owners_fail(const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 统一返回 owner-state 协议无效，并在提供诊断输出时重建 STATE_MAP_INVALID 的函数、指令、source 和首个包含该指令的 block 位置。
 *
 * ## Parameters
 * @param[in] function 提供 functionToken 和 blocks 的借用对象；diagnostic 非空时须有效，非空 blocks 范围须可读且稳定。
 * @param[in] entry 提供 instructionId 和 sourceId 的借用记录；正常调用已保证 instructionId 为有效的一基指令 ID，本函数不重新验证。
 * @param[in,out] diagnostic 可为 ZR_NULL；非空时须为独立可写记录，先整条清零，再填写错误和位置，不得与 function、entry、blocks 等输入重叠。
 *
 * ## Returns
 * @return 始终返回 ZR_FALSE；没有包含指令的 block 时，清零后的 blockId 保持零。
 *
 * ## Constraints and lifecycle
 * 仅在 diagnostic 非空时读取 function 和 entry；诊断为空时直接返回假，当前真实调用仍传入有效的 function/entry。
 * 使用 instructionId - 1 和 range.start/count 查首个匹配 block，不验证输入的真实 backing extent、block 数组 shape 或有效指令范围。
 * 只在调用期间借用输入，不保存、释放或修改其存储；不分配。清零和写诊断没有别名保护，不能据此承诺任意重叠输入保持不变。
 *
 * ## References
 * 直接调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:41；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:45；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:63；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:76；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:79；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:98；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:117；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:122。
 * 有限诊断断言示例：tests/parser/test_ssa_state_maps.c:647；tests/parser/test_ssa_state_maps.c:648；tests/parser/test_ssa_conditional_cleanup.c:303；tests/parser/test_ssa_conditional_cleanup.c:304；这些高层断言不逐一证明全部八处调用分支。
 */

/**
 * @fn TZrBool zr_state_map_owners_valid(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在外层已建立 CFG/SSA owner 与 liveness 分析后，核对单条 state-map entry 的 live 覆盖、静态 owner 状态及逻辑 root 个数约束。
 *
 * ## Parameters
 * @param[in] function 同一份未变更 ExecIR function 的有效借用，提供 valueCount、values 及诊断 block 数据；不接管这些池。
 * @param[in] ownership 针对该 function 成功建立且仍有效的 owner analysis 借用；本函数不建立或释放它。
 * @param[in] liveness 针对同一 function 的 liveness 借用；BEFORE_EFFECT 读取 before，其余已验证阶段读取 after。
 * @param[in] map 已由外层完成轻量 storage/identity 检查的 map 借用；live/root ranges、对应池及函数池须已验证、可读并在调用期间稳定。
 * @param[in] entry 正在验证的一条有效 entry；外层已验证 phase、instructionId 和 live/root range；本函数自行检查 ownerStates range、数量和非空池。
 * @param[in,out] diagnostic 可为 ZR_NULL；失败时经公共本地诊断 helper 清零并写 STATE_MAP_INVALID；成功不写它。非空输出须独立可写且不与任何输入存储重叠。
 *
 * ## Returns
 * @return 全部本地 owner/liveness/root 对应检查满足时返回 ZR_TRUE；任一检查失败时返回 ZR_FALSE，并按可选诊断输出记录本地无效位置。
 *
 * ## Constraints and lifecycle
 * 每个 liveness 要求的值须在 live slice 恰出现一次；非 live 值至多一次，且若保守附加，静态 expected 只能是 INITIALIZED 或 UNKNOWN。ValueId 采用一基编号。
 * 每个出现值的 ownerStatePool 项必须等于 StateMapOwnerAt 的静态结果；STATE_COUNT 结果无效。GC、UNIQUE、SHARED 且允许状态 mask 含 INITIALIZED 时，root slice 中该 ID 必须恰出现一次；其他出现值须零次。
 * 这里独立检查的是 owner slice 和各出现值的 owner/root 对应关系；不完整验证整份 map、所有 root ID/范围或任意原始指针 backing，也不读取运行时值 payload。不能绕过外层前置校验直接用畸形池调用。
 * function、map、entry 与两个 analysis 均只在同步调用期间借用；不保存、不分配、不修改、不释放输入，不注册真实 GC 根，不取得对象所有权或延长其生命周期。调用方负责阻止并发修改及为诊断输出隔离别名。
 *
 * ## References
 * 实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:32；私有声明：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.h:6；唯一直接调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 * 外层建立 analysis：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:614；逐 entry 调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:621；外层正常及失败释放：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660。
 * 静态 owner/mask 协作：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:252；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:278；有限测试：tests/parser/test_ssa_state_maps.c:647；tests/parser/test_ssa_state_maps.c:719；tests/parser/test_ssa_state_maps.c:751；tests/parser/test_ssa_state_maps.c:1287；tests/parser/test_ssa_state_maps.c:1288。
 */

/**
 * @fn TZrBool zr_state_map_resolve_owners(const SZrExecIrResumeRequest *request, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在已选定并验证的 checkpoint 上解析可选的具体 owner witness；有私有输出候选时写出具体 owner 状态，并筛掉不再 INITIALIZED 的逻辑 roots。
 *
 * ## Parameters
 * @param[in] request 已经由公开 MaterializeState 完成 function/map/entry 和身份校验的请求借用。ownerStates 为空当且仅当 ownerStateCount 为零；非空时数量须等于 function.valueCount，整个声称范围须可读。
 * @param[in] map 已验证 map 的借用；entry 的 live/owner slice 与对应池在调用期间须稳定有效，不被接管。
 * @param[in] entry 已验证的选中 entry 借用；live ValueId 均有效且 owner slice 与 live slice 按相同位置对应。
 * @param[in,out] prepared 可为 ZR_NULL；非空时是调用方私有、已按 entry 准备好的独立可写候选，values/ownerStates/roots 及 count/capacity 关系须有效且相互和输入隔离。本函数不分配、释放或接管这些候选池。
 * @param[in,out] diagnostic 可为 ZR_NULL；非空时须独立可写且不与请求、输入池、分析输入或 prepared 重叠。本地 witness 违例重建 STATE_MAP_INVALID；分析失败传回其诊断，liveness 失败仅覆盖 code。
 *
 * ## Returns
 * @return witness 协议、允许状态和可选候选解析全部成功时返回 ZR_TRUE；协议无效或临时分析失败时返回 ZR_FALSE。失败不保证 prepared 内容原样，由调用者丢弃未发布候选。
 *
 * ## Constraints and lifecycle
 * witness 非空时才新建临时 owner/liveness 分析，并且只读取 entry.liveValues 中 ValueId 对应的 ownerStates[valueId - 1]；虽然要求全 function 长度，但不逐项验证未被选中值的 witness 元素。
 * 每个被读取 witness 必须小于 CONDITIONAL、属于静态允许 mask；静态状态非 CONDITIONAL 时还必须与其完全相等。该枚举匹配不验证实际运行时 payload 的标签、初始化内容、真实 GC 保活或对象所有权。
 * witness 为空时，prepared 非空拒绝遇到的静态 CONDITIONAL；prepared 为空则可以保留这种未解静态状态并返回真。公开接口在 request.target 为空时走 prepared 为空分支；这不意味着整条公开调用无分配，因为此前仍验证整份 map。
 * prepared 非空时按 live 顺序逐项写 ownerStates；完成后原位稳定压缩现有 roots，仅保留其 ID 出现在 prepared.values 且对应 ownerStates 为 INITIALIZED 的项并更新 rootCount。不增加新根、不做真实 GC 注册、不清零尾部、不改变容量。
 * 后续 witness 失败前可能已经写入候选的较早 ownerStates；helper 不回滚这些写入。外层 MaterializeState 在失败后释放私有候选，在所有步骤成功后才替换旧 target；不能把外层发布事务性归给本 helper 或任意别名调用。
 * owner-analysis 建立失败直接返回，其已分配资源由该 builder 的失败路径自清理；进入 liveness 或 witness 处理后都经 finish 释放两个临时 analysis，零初始化和已自清理状态可按配套 Free 再次清理。
 * 调用同步完成，不保存参数或 witness 指针；输入及 candidate 的存活与并发互斥由调用者负责。无 witness 时本 helper 不建立 analysis；非空 witness 即便 prepared 为空也可能分配并失败。
 *
 * ## References
 * 实现：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:85；私有声明：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.h:10；两个直接调用：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:719；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:811。
 * 本地 witness 校验及根压缩：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:95；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:113；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:121；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:125；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:127；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:143。
 * analysis 构造/释放：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:267；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:313；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:314；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:209；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:208；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:168；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:234；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:16。
 * 公开候选失败释放/最终替换：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819；有限 witness/root/NULLtarget 断言：tests/parser/test_ssa_conditional_cleanup.c:277；tests/parser/test_ssa_conditional_cleanup.c:281；tests/parser/test_ssa_conditional_cleanup.c:303；tests/parser/test_ssa_conditional_cleanup.c:308；tests/parser/test_ssa_conditional_cleanup.c:311。
 */
