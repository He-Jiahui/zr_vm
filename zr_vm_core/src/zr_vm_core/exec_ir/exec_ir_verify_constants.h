#ifndef ZR_VM_CORE_EXEC_IR_VERIFY_CONSTANTS_H
#define ZR_VM_CORE_EXEC_IR_VERIFY_CONSTANTS_H

#include "zr_vm_core/exec_ir.h"

/* The module and function must have passed structural/full verification. */
TZrBool zr_exec_ir_verify_owned_constants(
        const SZrExecIrModule *module, const SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool zr_exec_ir_verify_owned_constants(const SZrExecIrModule *module, const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明供 VerifyModule 使用的内部补充验证接口：对非空模块常量池检查 CONSTANT.layoutId 及池项、结果和非零显式指令类型的一致性。它在当前函数完整验证之后调用，不是独立的输入形状验证入口。
 *
 * ## Parameters
 * @param[in] module 非空、已通过模块结构校验且在调用期间保持稳定的模块；其非空 constants 池必须可读。
 * @param[in] function 非空、已通过完整验证且在调用期间保持稳定的函数；CONSTANT 的唯一结果、结果区间与 ValueId 必须仍有效。
 * @param[out] diagnostic 可为空；非空时是与输入记录和被读池分离的可写诊断槽。
 *
 * ## Returns
 * @return 零大小模块池或全部被检查引用有效时返回 ZR_TRUE；首个越界或类型不匹配时返回 ZR_FALSE。失败时非空诊断被清零后写入错误及位置身份；本接口成功时不清除或改写诊断。
 *
 * ## Constraints and lifecycle
 * constantCount 为 0 时跳过模块池索引及类型限制，允许消费者另行配置外部常量池；这不表示本接口验证了该外部池。非空池先拒绝 layoutId >= constantCount，再读取池项；池项 typeToken 必须等于结果 typeToken，显式 instruction.typeToken 仅在非零时参与相等比较，常量 flags/bits 不由此接口解释。
 * 实现直接依赖前置验证，不提供任意空参数、损坏区间、无效 ValueId 或悬空指针的安全拒绝保证。diagnostic 不得与 module、function 或任何仍被读取的池重叠；调用期间输入不得被并发修改或释放；并发调用若要写诊断，须使用独立槽或由调用方同步。
 * 接口本体不分配或释放资源，不转移所有权、不保存输入指针、也不返回池元素借用。这里只声明同一物理实现，不新增另一实现或独立运行时覆盖。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.h:7：实际接口声明；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:30：唯一实现定义。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:8：声明头曝光；zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:711：唯一生产直接调用。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:34、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:43、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:52、zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_constants.c:57：零池与具体拒绝顺序。
 * - tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:22、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:53、tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:102：结构化错误断言；tests/parser/test_ssa_module_constant_pool_verifier_cases.inc:120：零池的 UINT32_MAX layoutId 接受场景。
 * - tests/cmake/ssa-module-constant-pool-verifier.cmake:2、tests/cmake/ssa-module-constant-pool-verifier.cmake:10：专用可执行目标和 CTest 注册；本提案没有执行它们。
 */
