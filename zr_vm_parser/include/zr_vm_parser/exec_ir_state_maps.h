#ifndef ZR_VM_PARSER_EXEC_IR_STATE_MAPS_H
#define ZR_VM_PARSER_EXEC_IR_STATE_MAPS_H

#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/conf.h"

ZR_PARSER_API TZrBool ZrParser_ExecIr_BuildStateMaps(
        SZrExecIrFunction *function,
        SZrExecIrDiagnostic *diagnostic);

/* Explicitly elaborate cleanup-block obligations and rebuild their maps.
 * Ordinary DROP instructions elsewhere retain strict semantics. */
ZR_PARSER_API TZrBool ZrParser_ExecIr_ElaborateCleanupDrops(
        SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic);

#endif

/**
 * @fn TZrBool ZrParser_ExecIr_ElaborateCleanupDrops(SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在 ExecIR 清理块的显式 lowering 阶段，把其中的普通 DROP 改为条件清理指令，验证候选图的 owner 状态并重建 state map，使单一清理块可安全覆盖资源已初始化与尚未初始化的路径。
 *
 * ## Parameters
 * @param[in,out] function 待处理的可变 ExecIR 函数。调用方保留该对象及其池和 map 的所有权；成功时对象内容由已验证的候选整体替换。
 * @param[out] diagnostic 可选的调用方诊断存储。非空时由本次调用清零或填写，存储须与 function 及其拥有的池、map 独立；失败类别及指令位置可能由本层或 Core verifier 提供。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示候选已验证且 function 已替换，所有清理块 DROP 均按条件清理语义处理，state map 已重建；返回 ZR_FALSE 表示未发布候选，原 function 内容保持不变，诊断（若提供）可说明失败。
 *
 * ## Constraints and lifecycle
 * function 必须非空、未 sealed，且调用期间由调用方独占修改。若输入已有 stateMap，该 map 必须与当前 IR 一致：初始 STRUCTURE|EFFECT 校验仍会验证非空 map，过期 map 可在 clone 前拒绝输入；本接口不会先清空旧 map 再尝试重建。所有成功路径均以候选替换整个 function，因此此前借用其 pools、instructions 或 stateMap 的指针随后失效，调用方须重新取得。仅当指令被改写时 generation 才递增；即使没有改写，成功路径仍 clone 并替换对象、重建 map。改写时 generation 已达上限会失败且不发布候选。
 *
 * ## References
 * 输入 map 的前置校验来自 ZrCore_ExecIr_VerifyFunction（zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:909）；该路径会进入 ZrCore_ExecIr_ValidateStateMap（zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:556）。可对照 ZrParser_ExecIr_BuildStateMaps 在验证副本中清空旧 map 后构建候选（zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:372）。定向调用见 tests/parser/test_ssa_conditional_cleanup.c:101、tests/parser/test_ssa_conditional_cleanup.c:633、tests/parser/test_ssa_conditional_cleanup.c:651。
 */
