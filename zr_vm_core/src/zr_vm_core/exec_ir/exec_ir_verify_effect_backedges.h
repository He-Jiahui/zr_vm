#ifndef ZR_VM_CORE_EXEC_IR_VERIFY_EFFECT_BACKEDGES_H
#define ZR_VM_CORE_EXEC_IR_VERIFY_EFFECT_BACKEDGES_H

#include "zr_vm_core/exec_ir.h"

TZrBool zr_exec_ir_classify_backedges(
        const SZrExecIrFunction *function,
        TZrBool **outBackedges,
        SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool zr_exec_ir_classify_backedges( const SZrExecIrFunction *function, TZrBool **outBackedges, SZrExecIrDiagnostic *diagnostic)
 *
 * ### Summary
 * 按predecessor全局槽标记目标块支配可达源块的自然回边。
 *
 * ### 使用时机
 * 由上列实际调用入口按本单元职责使用；不跨其它翻译单元同名静态定义取得信用。
 *
 * ### Parameters
 * @param[in] function 调用期间保持有效且只读的CFG描述符与池；完整前提见下节。
 * @param[out] outBackedges 非空独立可写指针槽，入口先置NULL；成功分配的数组由caller free。
 * @param[out] diagnostic 可空独立诊断输出，仅错误分支清零重建；成功不承诺清空旧值。
 *
 * ### Returns
 * @return TRUE表示完成本实现的分类/约定早退，并非完整CFG验证通过；零predecessor时*outBackedges为NULL，分配后成功时为由调用者free的数组。FALSE表示容量、分配或引用的predecessor ID失败，输出为NULL，非空diagnostic带本次错误。
 *
 * ### Constraints and lifecycle
 * function与outBackedges须非空；outBackedges须是独立可写且可丢弃旧值的指针槽，先由调用者释放旧分配，不能覆盖function描述符、输入池或diagnostic。diagnostic可空，非空须独立可写。调用期间只借用function及各池，禁止并发修改/释放，不保存输入借用或提供锁。调用方先保证blocks可读、块id为下标加1、predecessor池形状和每块range合法且加法不回绕；本函数只再检查所引用predecessor ID落在1..blockCount，不核successor对称性或完整CFG。零predecessor立即TRUE且输出NULL；非零时分配全局predecessorCount项，未被range引用的槽保持FALSE。entry无效/越界并不令valid变FALSE：分配成功并完成predecessor检查后，返回TRUE及全FALSE数组；有意义的支配分类要求调用方提供有效entry。reachable从entry单调传播，最多blockCount轮；只分类可达predecessor，self-edge直接TRUE，其余反向排除目标块，找到绕行entry则FALSE。visited入栈前置位，每个块最多入栈一次，栈容量为blockCount。显式SIZE_MAX/sizeof(stack元素)检查仅在SIZE_MAX<=UINT32_MAX时编译。三个calloc和一个malloc均尝试后统一判空；失败释放全部已取得对象且输出保持NULL；成功转移唯一backedges堆所有权，调用者用free释放。成功不主动清diagnostic，不保证任意输出别名安全。
 *
 * ### References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.h:6（definition）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:151（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:847（direct_call）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:31（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:43（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:44（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:46（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:47（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:52（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:53（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:54（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:55（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:56（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:58（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:63（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:70（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:71（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:79（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:81（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:82（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:98（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:99（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:108（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:109（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:114（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:120（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:129（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:135（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:139（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:140（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:141（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:142（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:143（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:144（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:145（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c:148（classification algorithm/failure/ownership）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:841（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:844（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:850（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:851（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:854（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:855（Core effect verifier caller, frees output）。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c:858（Core effect verifier caller, frees output）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:34（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:35（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:37（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:38（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:156（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:157（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:158（Parser loop collector caller/ownership）。
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effect_loops.c:159（Parser loop collector caller/ownership）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:888（finite direct API assertion）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:891（finite output assertion）。
 * - tests/parser/test_ssa_cfg_effects_builder.c:894（free output）。
 * - tests/parser/test_ssa_effects_verifier.c:250（indirect effect loop test）。
 * - tests/parser/test_ssa_effects_verifier.c:321（indirect loop assertion）。
 */
