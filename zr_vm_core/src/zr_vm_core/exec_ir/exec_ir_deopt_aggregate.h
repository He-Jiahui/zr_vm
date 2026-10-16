#ifndef ZR_VM_CORE_EXEC_IR_DEOPT_AGGREGATE_H
#define ZR_VM_CORE_EXEC_IR_DEOPT_AGGREGATE_H

#include "zr_vm_core/exec_ir_state_map.h"

/* Validate every recipe before any consumer traverses the side tables. */
TZrBool zr_exec_ir_deopt_aggregates_validate(
        const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic);

/* The shared preflight and the checkpoint's live range must be valid first. */
TZrBool zr_exec_ir_deopt_aggregates_live(
        const SZrExecIrFunction *function, const SZrExecIrDeoptState *state,
        const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry,
        SZrExecIrDiagnostic *diagnostic);

/* Append owned, compact recipe copies to an unpublished prepared target. */
TZrBool zr_exec_ir_deopt_aggregates_prepare(
        const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry,
        SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool zr_exec_ir_deopt_aggregates_validate( const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明跨Core编译单元使用的私有接口：依次预检recipe侧表形状、选区、身份/字段索引、payload及各state图闭合。
 *
 * ## Parameters
 * @param[in] function 可空且空值拒绝；非空记录/池及诊断定位所需存储须可读。
 * @param[out] diagnostic 可空独立诊断，失败重写、成功保留旧内容。
 *
 * ## Returns
 * @return 所有本模块检查通过为TRUE，首个错误为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。所有全局aggregates/fields均检查，包括未选中的项。identityId/typeToken/layoutId非零；每aggregate的fieldIndex唯一、每state的identityId唯一。先range后索引，允许合法别名/循环。不替代完整CFG/SSA、deopt state唯一ID/source/resume、owner/liveness或运行时类型/layout验证。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。
 * 失败/资源边界：NULL/形状/身份等为STATE_MAP_INVALID，range为INVALID_RANGE，重复为DUPLICATE_DEFINITION，VALUE引用为INVALID_VALUE；无分配，输入和已发布map不变。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h:7。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:175。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:178。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:196。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:207。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:214。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:222。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:233。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:236。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:403。
 */

/**
 * @fn TZrBool zr_exec_ir_deopt_aggregates_live( const SZrExecIrFunction *function, const SZrExecIrDeoptState *state, const SZrExecIrStateMap *map, const SZrExecIrStateMapEntry *entry, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明跨Core编译单元使用的私有接口：确认所选state的每个VALUE field出现在checkpoint已验证的liveValues中。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数，recipe预检已成功。
 * @param[in] state 非空state，其aggregate/field范围可安全遍历。
 * @param[in] map 非空图，valuePool及entry.liveValues范围已有效。
 * @param[in] entry 非空checkpoint，caller已核其identity/phase与state匹配。
 * @param[out] diagnostic 可空独立诊断。
 *
 * ## Returns
 * @return 所有VALUE依赖存在为TRUE，首次遗漏为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。只核存在性，不核唯一出现/root/owner/availability；AGGREGATE和UNINITIALIZED不当SSA value，不递归展开图。diagnostic 可空；非空须独立可写，不覆盖借用输入/池。失败先清零再写；成功不主动清除旧内容。
 * 失败/资源边界：遗漏写STATE_MAP_INVALID、expected=field.valueId/actual=0并优先entry位置；不改输入/target。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h:11。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:336。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:347。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:348。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:351。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:352。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h:11。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:369。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:400。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:503。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:504。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:62。
 */

/**
 * @fn TZrBool zr_exec_ir_deopt_aggregates_prepare( const SZrExecIrFunction *function, const SZrExecIrStateMapEntry *entry, SZrExecIrMaterializedState *prepared, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明跨Core编译单元使用的私有接口：把checkpoint选中的recipe复制为紧凑、独立拥有的两份逻辑数组，附加到未发布prepared。
 *
 * ## Parameters
 * @param[in] function 非空稳定函数，recipe和checkpoint语义验证已完成。
 * @param[in] entry 非空已验证checkpoint；非零deoptId应有唯一匹配state。
 * @param[in,out] prepared 非空Init后的未发布候选；其它三池可已准备，两recipe指针/count/capacity须为空。
 * @param[out] diagnostic 可空，须独立于prepared/function/entry和所有池。
 *
 * ## Returns
 * @return 无需recipe(deoptId0)或复制成功为TRUE；无state、累计/字节尺寸溢出或recipe malloc失败为FALSE。
 *
 * ## Constraints and lifecycle
 * 实际读取的记录/池须稳定可读，期间不得并发修改、重分配或释放；形状检查不认证地址可读性、分配来源或独立 free 权。非零ID取首个state，唯一性由上游保证。先核UInt32累计与size_t尺寸，再分别malloc，按选区顺序复制并将fields.start重基为紧凑offset；identity/payload原样保留，不构造运行时对象。不能传已发布target或已有recipe池。成功新池由prepared拥有，唯一上层负责提交。
 * 失败/资源边界：失败可能已写第一份malloc指针但count/capacity尚未赋值；helper不free/回滚，上层必须MaterializedStateFree候选，不能消费/重试半成品。deoptId0不改prepared。错误为STATE_MAP_INVALID/CAPACITY_OVERFLOW/OUT_OF_MEMORY；旧target不在此helper处理。
 *
 * ## References
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.h:17。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:361。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:367。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:368。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:374。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:378。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:380。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:383。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:387。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:391。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:398。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_deopt_aggregate.c:406。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:728。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:812。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:813。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:818。
 * zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:819。
 * tests/parser/test_ssa_deopt_aggregates.c:385。
 * tests/parser/test_ssa_deopt_aggregates.c:392。
 * tests/parser/test_ssa_deopt_aggregates.c:401。
 */
