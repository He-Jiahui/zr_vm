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

/**
 * @fn TZrBool ZrParser_ExecIr_BuildStateMaps(SZrExecIrFunction *function, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 在 ExecIR 已形成可验证的边界后，按当前指令、liveness 和 owner analysis 构建 GC root、异常、挂起及 deopt 可用的 state map。该 Parser 阶段把 Core 分析结果整理成候选 map，并在完整成功后一次替换 function 当前关联的 map。
 *
 * ## Parameters
 * @param[in,out] function 非空、未 sealed 且可读写的 ExecIR 函数；函数及其指令/value/block/deopt 池由调用方拥有。若已有 map，它须是结构可安全分别释放、由此 function 唯一拥有的对象；map 对象及 entries/value/root/owner-state pools 彼此不重叠，也不与 function 本体或其任何池存储重叠。map 内容可以因 IR 已变更而语义过期。
 * @param[out] diagnostic 可为空。非空时本调用会先清零并在失败时填写诊断；调用期间其存储必须与 function、stateMap 对象及其各池、function 拥有的所有池互不重叠。
 *
 * ## Returns
 * @return 返回 ZR_TRUE 表示所有边界及检查点均构造成功，新的 map 已关联到 function；旧 map 对象及其池随即释放。返回 ZR_FALSE 表示未发布候选，function 原有 map 和其他字段保持不变；diagnostic（若提供）可被清零或写入失败信息。
 *
 * ## Constraints and lifecycle
 * 调用方必须保证 function 的 Core 结构和池可供验证与分析读取，并在整个同步调用期间独占访问；本接口没有锁或并发替换协议。校验新 IR 时使用浅拷贝并清空该验证副本的 stateMap，所以语义过期但仍由 function 安全、唯一拥有的旧 map 不会阻挡重建；这不表示可传入已释放、重叠、共享或结构不可安全释放的旧 map。成功只替换 stateMap：function 的其他池、指令和值及 `contract.generation` 不由本过程修改；新 map 记录构建时的 generation。成功后所有指向旧 map、entries、value/root/owner-state pools 的借用指针失效，调用方须重新查询。候选构建、Core 验证、分析或最终 map 分配失败均保留旧 map；调用方仍拥有 function 并可按错误原因修正后重试。诊断不得与 function、旧 map 或其池别名。
 *
 * ## References
 * 公开调用路径包括 `ZrParser_ExecIr_RunPassPipeline`、ExecIR build 的 `build_state_maps_unpublished`、`ZrParser_ExecIr_ElaborateCleanupDrops` 及状态 map、deopt、AOTIR 和 runtime 测试。配套 Core 契约由 `ZrCore_ExecIr_StateMapLivenessBuild`、`ZrCore_ExecIr_OwnerAnalysisBuild`、`ZrCore_ExecIr_StateMapFree` 和 `ZrCore_ExecIr_VerifyFunction` 提供；现有定向测试见 `tests/parser/test_ssa_state_maps.c` 与 `tests/parser/ssa_state_map_verifier_cases.inc`。
 */
