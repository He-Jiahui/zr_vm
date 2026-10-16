#ifndef ZR_VM_CORE_EXEC_IR_MATERIALIZE_OWNERS_H
#define ZR_VM_CORE_EXEC_IR_MATERIALIZE_OWNERS_H

#include "zr_vm_core/exec_ir_state_map_liveness.h"

TZrBool zr_state_map_owners_valid(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic);
TZrBool zr_state_map_resolve_owners(
        const SZrExecIrResumeRequest *request, const SZrExecIrStateMap *map,
        const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared,
        SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool zr_state_map_owners_valid(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明 Core 内部单 entry owner 校验接口，使完整 state-map 验证器在已有 owner/liveness 分析上复用一致的 live、owner-state 和逻辑 root 规则。
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
 * @return 返回 ZR_TRUE 表示本接口负责的单 entry owner 对应关系成立；返回 ZR_FALSE 表示本地协议无效并可写诊断，不代表此接口独立完成整份 map 或运行时 payload 验证。
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
 * 本私有头的真实包含入口：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:1；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:2；不是公开安装 ABI 或持有 analysis 的容器。
 */

/**
 * @fn TZrBool zr_state_map_resolve_owners(const SZrExecIrResumeRequest *request, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明 checkpoint owner witness 解析接口，供公开逻辑物化的验证分支和私有候选分支共享；分析结果和输出池的拥有者均由调用约定明确分离。
 *
 * ## Parameters
 * @param[in] request 已经由公开 MaterializeState 完成 function/map/entry 和身份校验的请求借用。ownerStates 为空当且仅当 ownerStateCount 为零；非空时数量须等于 function.valueCount，整个声称范围须可读。
 * @param[in] map 已验证 map 的借用；entry 的 live/owner slice 与对应池在调用期间须稳定有效，不被接管。
 * @param[in] entry 已验证的选中 entry 借用；live ValueId 均有效且 owner slice 与 live slice 按相同位置对应。
 * @param[in,out] prepared 可为 ZR_NULL；非空时是调用方私有、已按 entry 准备好的独立可写候选，values/ownerStates/roots 及 count/capacity 关系须有效且相互和输入隔离。本函数不分配、释放或接管这些候选池。
 * @param[in,out] diagnostic 可为 ZR_NULL；非空时须独立可写且不与请求、输入池、分析输入或 prepared 重叠。本地 witness 违例重建 STATE_MAP_INVALID；分析失败传回其诊断，liveness 失败仅覆盖 code。
 *
 * ## Returns
 * @return 解析成功返回 ZR_TRUE，witness 协议或分析失败返回 ZR_FALSE；prepared 为空时允许无 witness 的静态 CONDITIONAL，不输出具体候选；prepared 非空失败时须丢弃可能部分写入的候选。
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
 * 包含和外层协作：zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:2；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:685；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:718；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:817。
 */
