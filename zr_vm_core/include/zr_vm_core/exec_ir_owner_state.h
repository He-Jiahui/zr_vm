#ifndef ZR_VM_CORE_EXEC_IR_OWNER_STATE_H
#define ZR_VM_CORE_EXEC_IR_OWNER_STATE_H

#include "zr_vm_core/exec_ir_state_map.h"

/* Transient analysis storage, never serialized into an artifact. Build into
 * an empty object, free after use, and do not mutate the function meanwhile. */
typedef struct SZrExecIrOwnerAnalysis {
    TZrUInt8 *before;
    TZrUInt8 *after;
    TZrUInt8 *reachable;
    TZrUInt32 valueCount;
    TZrUInt32 instructionCount;
} SZrExecIrOwnerAnalysis;

ZR_CORE_API TZrBool ZrCore_ExecIr_OwnerAnalysisBuild(
        const SZrExecIrFunction *function, SZrExecIrOwnerAnalysis *analysis,
        SZrExecIrDiagnostic *diagnostic);
ZR_CORE_API void ZrCore_ExecIr_OwnerAnalysisFree(SZrExecIrOwnerAnalysis *analysis);
/* COUNT means unreachable or invalid. Mixed concrete states are CONDITIONAL. */
ZR_CORE_API EZrExecIrStateMapOwnerState ZrCore_ExecIr_OwnerStateAt(
        const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);

#define ZR_EXEC_IR_OWNER_STATE_BIT(state) ((TZrUInt8)(1u << (state)))
ZR_CORE_API TZrUInt8 ZrCore_ExecIr_OwnerStateMaskAt(
        const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);
/* Shape check shared by structural verification, analysis and the oracle. */
ZR_CORE_API TZrBool ZrCore_ExecIr_ConditionalCleanupValid(
        const SZrExecIrFunction *function, TZrExecIrInstructionId instruction);

#endif

/**
 * @struct SZrExecIrOwnerAnalysis
 *
 * ## Summary
 * 持有一次 ExecIR owner 数据流分析的临时状态池；属于分析工作数据，不序列化到 artifact，也不拥有运行时 payload。
 *
 * ## Fields and invariants
 * before 和 after 按指令、ValueId 保存状态位集；reachable 每指令一个可达字节。valueCount 和 instructionCount 是本次源 IR 的计数快照。
 * 成功且 instructionCount 非零的 Build 分配三份独立池；before/after 每行分配 max(valueCount, 1) 字节。valueCount 为零时不存在可查询的 ValueId。零指令成功可保留非零 valueCount 而三个池均为 NULL。
 * 空对象的所有字段为零。输入检查失败或分配失败后为空；尺寸乘法溢出发生在 counts 写入后，允许两个 counts 保留而三指针为 NULL。不可仅由非零 count 推断池存在。
 *
 * ## Constraints and lifecycle
 * 记录和其三份池的所有权分开：记录存储由调用方管理，非空池必须分别独占且兼容 C free。Build 仅接收空对象或已 Free 的对象，不能直接覆盖仍拥有旧池的记录。
 * Free 释放三份旧池并清零记录，旧池/元素的借用指针随之失效，记录本身继续有效。不得在 Build/Free 同时读取或使用旧借用。
 * 本记录不保存 function 指针、token、generation 或版本见证；数组只是静态 owner 状态。将其用于函数或 map 时，调用方须保证仍对应产生它的 IR 版本；输入变化不会自动使查询失败。
 * 借用的 function/池在 Build 调用内须稳定可读。分析结果本身不延长 function、map、运行时对象或 GC 对象的生命周期；它也不建立运行时保活关系。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h:6
 * - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h:9
 * - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h:12
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:285
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:289
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:297
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:310
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:348
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:381
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:437
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656
 */

/**
 * @fn TZrBool ZrCore_ExecIr_OwnerAnalysisBuild(const SZrExecIrFunction *function, SZrExecIrOwnerAnalysis *analysis, SZrExecIrDiagnostic *diagnostic)
 *
 * ## Summary
 * 建立函数的保守 owner 位集与指令可达信息，供 map 构建、验证、cleanup 与物化路径使用。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 并被拒绝；非空输入及实际访问池在整个调用期间真实可读且不得修改/释放。
 * @param[out] analysis 独立可写的空对象，或先经 OwnerAnalysisFree 释放旧池的对象；与输入/诊断分离。
 * @param[out] diagnostic 可为 NULL；否则须为独立可写记录，不可与输入、analysis 或它们的池重叠。
 *
 * ## Returns
 * @return 成功返回 ZR_TRUE；analysis 为空指针、输入检查失败、尺寸溢出或分配失败返回 ZR_FALSE，并按条件填写 diagnostic。合法零指令输入成功且不分配池。
 *
 * ## Constraints and lifecycle
 * 入口先清零 diagnostic，再直接清零非空 analysis，故已有池必须在调用前 Free；否则会丢失旧指针。本入口不是旧分析结果的事务替换。
 * 输入检查失败后非空 analysis 全零。计数在尺寸乘法检查前写入，CAPACITY_OVERFLOW 保留 counts 而三个输出池为 NULL；分配失败则释放已取得的输出池与临时池并将记录全零，报告 OUT_OF_MEMORY。
 * 成功后调用方独占三个分析池并负责 Free。结果不保存 function 指针或版本身份；按原 IR 版本使用由调用方保证。没有同步或 runtime payload/GC 所有权。
 * 内部检查和分析完成不等于完整 VerifyFunction 或语义 liveness 合法性，不能仅凭返回 TRUE 推断所有消费/CFG/effect 协议成立。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:285
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:289
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:297
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:302
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:310
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:419
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:54
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:67
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660
 * - tests/parser/test_ssa_state_map_ownership.c:311
 */

/**
 * @fn void ZrCore_ExecIr_OwnerAnalysisFree(SZrExecIrOwnerAnalysis *analysis)
 *
 * ## Summary
 * 释放分析独占的 before、after、reachable 池，并将记录清零。
 *
 * ## Parameters
 * @param[in,out] analysis 可为 NULL；非空须是零对象或独占拥有三个可分别由 C free 释放的池的合法分析记录。
 *
 * ## Returns
 * 无 C 返回值；NULL 时无操作，正常完成后非空记录为空且可再次 Free 或 Build。
 *
 * ## Constraints and lifecycle
 * 调用方须保证三份非空池可独立 free，互不重叠且不覆盖记录存储；函数不校验真实分配、别名或并发访问。
 * 完成后旧池和元素借用失效；analysis 记录本身仍有效且为空。不得与读取/Build/其他 Free 并发，也不释放 function 或运行时值。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:209
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:211
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:214
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:385
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:656
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:660
 */

/**
 * @fn EZrExecIrStateMapOwnerState ZrCore_ExecIr_OwnerStateAt(const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 读取单个指令/ValueId 的 owner 归约状态。
 *
 * ## Parameters
 * @param[in] analysis 可为 NULL；非空须为稳定的合法空对象或未 Free 的 Build 结果。
 * @param[in] instruction 从 1 开始的指令标识。
 * @param[in] value 从 1 开始的 ValueId，语义解释须对应分析的 IR 版本。
 * @param[in] phase 有效 phase；BEFORE_EFFECT 读 before，其余有效 phase 读 after。
 *
 * ## Returns
 * @return 单一已知具体状态位返回该状态；原始掩码为 0 返回 OWNER_STATE_COUNT，其余非零掩码返回 OWNER_CONDITIONAL。
 *
 * ## Constraints and lifecycle
 * 遵循 OwnerStateMaskAt 的边界/phase 规则和借用前提，不填写诊断，不验证池实际尺寸、token/generation 或内容来源。COUNT 不能区分不可达与查询无效。
 * 本接口不进行语义 liveness 检查；CONDITIONAL 不自动证明该值能作为 state-map owner 序列化。记录和池调用内稳定，不能与 Build/Free 并发。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:443
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:445
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:450
 * - tests/parser/test_ssa_gvn_range.c:289
 * - tests/parser/test_ssa_gvn_range.c:291
 * - tests/parser/test_ssa_gvn_range.c:293
 * - tests/parser/test_ssa_gvn_range.c:296
 * - tests/parser/test_ssa_sccp_copy_availability.c:141
 * - tests/parser/test_ssa_sccp_copy_availability.c:143
 * - tests/parser/test_ssa_sccp_copy_availability.c:145
 * - tests/parser/test_ssa_sccp_copy_availability.c:147
 */

/**
 * @def ZR_EXEC_IR_OWNER_STATE_BIT(state)
 *
 * ## Summary
 * 将具体 owner 状态位序号转成 TZrUInt8 掩码，供原始位集与 liveness 层比较。
 *
 * ## Parameters
 * @param[in] state 用于表示具体状态的有效非负位序号，须处于 unsigned int 可移位宽度内；通常取 UNKNOWN、INITIALIZED、MOVED、DROPPED、UNINITIALIZED，不取 CONDITIONAL/COUNT。
 *
 * ## Returns
 * @return 宏表达式产生先计算 1u << state、再转换为 TZrUInt8 的结果。
 *
 * ## Constraints and lifecycle
 * state 表达式只求值一次。宏没有输入校验；负数或超出 unsigned int 位宽的移位不合法，转换为 UInt8 也不提供防护。使用的具体状态位须能由八位结果表示。
 * 这是状态位表达式，不是枚举值转换、运行时 owner 证明或可失败的函数。无分配、诊断、指针借用或同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_owner_state.h:25
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:272
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:286
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:288
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:294
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:298
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:71
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:115
 */

/**
 * @fn TZrUInt8 ZrCore_ExecIr_OwnerStateMaskAt(const SZrExecIrOwnerAnalysis *analysis, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 读取分析中一个指令、ValueId、phase 的原始 owner 状态位集。
 *
 * ## Parameters
 * @param[in] analysis 可为 NULL；非空须为稳定可读的合法空记录或尚未 Free 的 Build 结果及其池。
 * @param[in] instruction 从 1 开始的指令标识。
 * @param[in] value 从 1 开始的 ValueId。
 * @param[in] phase BEFORE_EFFECT 读 before；AFTER_EFFECT/CLEANUP_COMPLETE 等其余有效值读 after。
 *
 * ## Returns
 * @return analysis、ID 或 phase 无效、指令不可达、reachable 或所选池为空时返回 0；否则返回存储的掩码，不填写诊断。
 *
 * ## Constraints and lifecycle
 * 有效的非 BEFORE phase 共用 after 行，没有另一个 cleanup 专用状态池。返回值不区分所有 0 的原因。
 * 记录内 counts 和指针只是有限检查，不能证明实际分配大小、可读性或源 IR 版本一致。调用方须保证记录/池真实有效，且调用内不与 Build/Free 并发。
 * 返回整数值，不分配、不修改分析、不保有 function 或 runtime payload。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:437
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:60
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:256
 */

/**
 * @fn TZrBool ZrCore_ExecIr_ConditionalCleanupValid(const SZrExecIrFunction *function, TZrExecIrInstructionId instruction)
 *
 * ## Summary
 * 为结构验证、owner 分析、parser cleanup 和 oracle 共用 guarded cleanup 的局部形状检查。
 *
 * ## Parameters
 * @param[in] function 可为 NULL 并被拒绝；非空时实际读取的指令、operand、block、value 及定义相关池须稳定可读。
 * @param[in] instruction 从 1 开始的目标指令标识；越界被拒绝。
 *
 * ## Returns
 * @return 函数及所需池/范围、目标 opcode/arity、managed owner 的定义证据、所属 cleanup block 检查均通过返回 ZR_TRUE；其他情况返回 ZR_FALSE，不填写诊断。
 *
 * ## Constraints and lifecycle
 * 目标必须为一 operand、零 result 的 DROP_IF_INITIALIZED；operand 指向 UNIQUE 或 SHARED value，且是 external-entry 或在 result/PHI 中存在定义。
 * 按 block 数组顺序选首个包含该指令的 block，并检查 CLEANUP 标志；不重新证明区间互斥、可达性、支配或完整 CFG/effect 语义。该检查不承诺运行时 payload 有效。
 * 不分配或修改函数。所有读取的池须真实有效且稳定；局部 count/capacity/NULL 检查不构成分配来源或内存可读性证明。
 *
 * ## References
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:35
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:43
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:59
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:61
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cleanup_drops.c:47
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c:633
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c:256
 */
