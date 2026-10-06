#ifndef ZR_VM_PARSER_EXEC_IR_SOURCE_MODULE_CONTRACT_H
#define ZR_VM_PARSER_EXEC_IR_SOURCE_MODULE_CONTRACT_H

#include "zr_vm_parser/compiler.h"
#include "zr_vm_core/exec_ir.h"

/**
 * @par Summary
 * 冻结的有限 source module contract producer API；当前实现为 feature RED stub，
 * 仅返回 UNSUPPORTED，以下合同描述待实现的 GREEN 行为，不能视为已交付能力。
 * GREEN 将从真实 finalized SCRIPT metadata 和同一编译快照的 single-entry
 * BuildModule 结果验证并发布模块身份，不产生 module layout 或 AOT descriptor。
 *
 * @par Parameters
 * @param compiler 借用真实且仍存活的编译器状态、currentFunction、SCRIPT AST、
 * canonical interner 与其可达 metadata 存储。源函数必须已经完成编译和 metadata
 * 发布；仅支持 versionless、无参数、无接收者、无效果且返回非空 INT64 的普通
 * SCRIPT。不支持 submission、子函数、闭包、导入或导出等扩展源形态。
 * @param module 借用同一源快照实际 BuildModule 的可写结果；只含原始 entry
 * function，其 token 与 canonical signature hash 必须已经正确。原 function
 * 的合同版本、token、canonical hash、首次 generation=1 必须本来正确；此 API
 * 仅允许其 contract.moduleHash 缺席为零或等于真实候选值，不修复其余身份。
 * @param diagnostic 可为 NULL；非空时借用独立、完整且可写的
 * SZrExecIrDiagnostic。入口清零；只有确认真实 entry 身份后才填 functionToken。
 *
 * @par Returns
 * GREEN：ZR_TRUE 表示 module.contract 与唯一原 function.contract.moduleHash
 * 已在全部校验通过后发布，重复调用幂等；成功 diagnostic 的全部字节为零。
 * ZR_FALSE 表示拒绝，整个 compiler、源函数、module 及所有可达 owner 存储
 * 保持不变。RED stub：忽略 compiler/module，不读非空输入，不写 module，
 * 清零可选 diagnostic 并设置 ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED，返回 ZR_FALSE。
 *
 * GREEN 诊断映射：NULL compiler/module 为 INVALID_ARGUMENT；不支持源资格为
 * ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED；record/blob/container 的数值范围错误为
 * ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE；MODULE token/hash/binding/唯一性矛盾为
 * MODULE_MISMATCH；entry target 为 TARGET_MISMATCH；canonical/signature/blob/
 * hash/配对矛盾为 SIGNATURE_MISMATCH；已有版本或 reserved 矛盾为
 * VERSION_MISMATCH；generation 为 STALE_GENERATION；已有非零 module layout
 * 为 LAYOUT_MISMATCH；能力及效果分别为 CAPABILITY_MISMATCH、EFFECT_MISMATCH。
 * 上述无前缀名称均为 ZR_EXECUTION_DIAGNOSTIC_*。expectedVersion/actualVersion
 * 用于版本标量；expectedHash/actualHash 用于真实身份及其他标量事实，不能
 * 截断 generation/hash。blockId/instructionId/sourceId 无对应身份时保持零。
 * 既有 helper 重算 hash 返回零（包括 opaque allocator/hash-provider 失败）为
 * SIGNATURE_MISMATCH；expectedHash 取真实已发布的非零 witness 或非零要求，
 * actualHash=0。零结果只证明未得到有效 hash，不能推断必然 OOM，不能将零
 * 改为一。原源函数已发布的 moduleSignatureHash 为零仍按 MODULE_MISMATCH
 * 拒绝准入；诊断不猜测零结果的内部原因，失败仍保持所有输入不变。
 *
 * @par Constraints and lifecycle
 * 调用在真实 source 准备及 BuildModule 之后、compaction 之前。compiler 的
 * AST、源函数、类型池、metadata records、signature heap 和 module 的全部
 * 容器必须有效、正确对齐、可读且在整个调用期间存活；可写 module 与 compiler
 * 可达源存储不得重叠，diagnostic 与全部输入/输出存储不得重叠。调用期间不得
 * reset/free、增长、compact 或并发修改任何快照 owner。数值范围与地址跨度
 * 检查不能证明任意指针可读，也不能证明混合快照来自同一次编译；调用者提供
 * 真实来源，producer 验证当前存储的一致性。
 *
 * GREEN 先验证容器形状、乘积/跨度与 Blob 范围，再查唯一真实 MODULE/SIGNATURE
 * 和 SCRIPT_ENTRY MEMBER_DEF/SIGNATURE 配对，使用现有 ValidateSignatureBlob
 * 与 metadata_signature_hash_v1 重算两类 Blob 的实际哈希。源函数
 * moduleSignatureHash 使用现有 compiler_script_entry_metadata_hash 独立重算；
 * 不复制 hash 域算法，不用 MODULE Blob hash 替代 entry ABI hash。
 * 候选 module.contract 的 schema/abi/logical 为现有 6/17/1，targetToken 为真实
 * MODULE token，signatureHash 为 MODULE Blob hash，moduleHash 为独立 entry
 * ABI hash，generation 为原 function 的实际首次值 1（不取 moduleVersion 或
 * metadataGeneration）。layoutHash=0 明确表示缺席；requiredCapabilities、
 * declaredEffects、reserved0/reserved1 均为零。已有 module 合同每个字段只接受
 * 缺席零值或对应真实候选值，任何非零矛盾拒绝；不改 module.moduleToken、
 * module.moduleHash 或函数列表等 BuildModule 结果。
 *
 * producer 可为既有 entry ABI hash helper 暂时分配并释放内存，不保留指针，
 * 不转移 owner，不延长 compiler/module 生命周期。只在全部检查结束后写两个
 * 合同目标；失败路径没有输入补丁。frame/projection producer 独立负责其证据。
 *
 * @par References
 * exec_ir_builder.h: ZrParser_ExecIr_BuildModule；execution_contract.h:
 * SZrExecutionContract/SZrExecIrDiagnostic；compiler_script_entry_metadata.c:
 * 真实 SCRIPT_ENTRY 配对与 entry ABI hash 域；compiler_metadata_signature.h:
 * metadata_signature_hash_v1；zrp_metadata.h: ZrCore_ZrpMetadata_ValidateSignatureBlob。
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_BindSourceModuleContract(
        const SZrCompilerState *compiler, SZrExecIrModule *module,
        SZrExecIrDiagnostic *diagnostic);

#endif
