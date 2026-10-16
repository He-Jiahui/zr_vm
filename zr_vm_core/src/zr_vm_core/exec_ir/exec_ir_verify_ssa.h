#ifndef ZR_VM_CORE_EXEC_IR_VERIFY_SSA_H
#define ZR_VM_CORE_EXEC_IR_VERIFY_SSA_H

#include "zr_vm_core/exec_ir.h"

TZrBool zr_exec_ir_verify_ssa(const SZrExecIrFunction *function,
                              SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool zr_exec_ir_verify_ssa(const SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 声明 Core 验证器内部的 SSA 阶段边界：唯一实际上游在 VerifyFunction 通用验证之后调用，实施临时支配/定义/边可用性检查，不接管函数所有权。
 *
 * ## Parameters
 * @param[in] function 非空、在整个调用期间稳定可读的函数借用；由 VerifyFunction 的通用验证先保证池、范围、值/块/边ID可安全访问。
 * @param[in,out] diagnostic 可空独立诊断；错误时按SSA分支写字段，成功不在本入口主动清零。
 *
 * ## Returns
 * @return TRUE 表示支配构建和SSA检查通过，FALSE 表示任一阶段拒绝或OOM；所有本入口取得的临时分析池均在自身/下层返回前释放，无资源交给调用方。
 *
 * ## Constraints and lifecycle
 * 不是可直接接受任意NULL/畸形存储的独立公开验证器：function在构造分析时直接解引用。VerifyFunction先清诊断并做通用检查，是否附加CFG互反/终结器检查由STRUCTURE level决定；SSA通过不能外推其它level或随后stateMap通过。不改变函数池、generation或cached immediateDominator，不延长其生命周期。diagnostic不得与function或读取池重叠；无锁，禁止调用期间修改/释放输入。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:924：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:929：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:930：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:933：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c:935：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.h:6：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:724：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:736：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:899：本单元职责、调用或相关断言证据。
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909：本单元职责、调用或相关断言证据。
 */
