#ifndef ZR_VM_CORE_EXEC_IR_STATE_MAP_LIVENESS_H
#define ZR_VM_CORE_EXEC_IR_STATE_MAP_LIVENESS_H

#include "zr_vm_core/exec_ir_owner_state.h"

/* Private analysis of an already verified function. Rows use zero-based
 * instruction indices and bits use zero-based value indices. */
typedef struct SZrStateMapLiveness {
    TZrUInt8 *before;
    TZrUInt8 *after;
    TZrUInt8 *semanticBefore;
    TZrUInt8 *semanticAfter;
    size_t rowBytes;
} SZrStateMapLiveness;

ZR_CORE_API EZrExecutionDiagnosticCode ZrCore_ExecIr_StateMapLivenessBuild(
        const SZrExecIrFunction *function, SZrStateMapLiveness *liveness);
ZR_CORE_API void ZrCore_ExecIr_StateMapLivenessFree(SZrStateMapLiveness *liveness);
ZR_CORE_API TZrBool ZrCore_ExecIr_StateMapLivenessContains(const SZrStateMapLiveness *liveness,
                                        TZrExecIrInstructionId instruction,
                                        TZrExecIrValueId value,
                                        TZrBool after);

ZR_CORE_API TZrUInt8 ZrCore_ExecIr_StateMapOwnerMaskAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);
/* COUNT rejects unavailable semantic reads. Conditional/inactive states
 * are permitted only for UNIQUE/SHARED cleanup-only liveness. */
ZR_CORE_API EZrExecIrStateMapOwnerState ZrCore_ExecIr_StateMapOwnerAt(
        const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership,
        const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction,
        TZrExecIrValueId value, EZrExecIrStateMapPhase phase);

#endif

/**
 * @struct SZrStateMapLiveness
 *
 * ## Summary
 * 持有同一已验证 ExecIR 函数的完整存活与 semantic 存活两套位图；这些是逻辑 ValueId 集合，供状态图生成和 owner 判定使用，不是运行时值或实际 GC 保活对象。
 *
 * ## Fields
 * before、after：完整存活的两个指令行池，包括 guarded cleanup 的 operand 用途和恢复用途。
 * semanticBefore、semanticAfter：另一套指令行池，仅在构建时省略 DROP_IF_INITIALIZED 的 operand 用途；普通 DROP 与 deopt 用途仍在。
 * rowBytes：每行字节数，非空成功分析等于 valueCount/8 加余数指示。池中第 instruction-1 行、第 value-1 位对应公开一基 ID。
 *
 * ## Constraints and lifecycle
 * 成功的非空公开 Build 使四池分别拥有独立 calloc 分配；每池长度为匹配 function.instructionCount * rowBytes。记录不保存 counts、function 指针、token 或 generation，查询者须另持匹配的未变更 IR 上下文。
 * 全零是空状态。值或指令为零的成功结果全零；首轮容量溢出可保留非零 rowBytes 且四池都为空，不能从空池反推 rowBytes 必为零。单套内部构建过程只临时持有 before/after。
 * owning 记录只能有一个释放责任。复制结构会复制池地址，不会复制池；内部 semantic 查询的栈上浅视图只借用，不能单独 Free。
 * Build 入口清零而不释放旧池，故先使用空记录或先 Free。Free 后记录自身继续有效且清零，旧池借用失效；再次 Build 建立新内容，旧池借用不可继续使用。
 * 分析不延长 function、ownership 或任何运行时对象生命周期，也不自动识别 IR 变更。调用方在修改 IR 后重建，并将 Build/Free 与查询的并发读写隔离；实现不提供同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:8
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:9
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:13
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:16
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:18
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:21
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:154
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:156
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:160
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:208
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:213
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:237
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:245
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:355
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:379
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:479
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:562
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:614
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:90
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:101
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:144
 * - tests/parser/test_ssa_licm_scalar_context.inc:103
 * - tests/parser/test_ssa_licm_scalar_context.inc:123
 * - tests/parser/test_ssa_licm_scalar_context.inc:134
 */

/**
 * @fn EZrExecutionDiagnosticCode ZrCore_ExecIr_StateMapLivenessBuild(const SZrExecIrFunction *function, SZrStateMapLiveness *liveness)
 *
 * ## Summary
 * 公开声明的调用契约：在同一已验证函数上建立完整存活和语义存活两套位图，供 checkpoint 活值/根集合及 cleanup-only owner 判定共用。
 *
 * ## Parameters
 * @param[in] function 非空、调用期间稳定的已验证函数；CFG、SSA 与 aggregate recipe 的池、范围、ID 和 opcode 已由上层验证。
 * @param[out] liveness 非空且为空或已先 Free 的独立输出记录；入口清零，不保留旧分析。
 *
 * ## Returns
 * @return 成功返回 NONE：非空分析持有四个位图池，空值或空指令函数输出全零。普通 deopt 池、区间或 ID 无效时返回 STATE_MAP_INVALID 且输出全零。首套尺寸溢出返回 CAPACITY_OVERFLOW，可能仅 rowBytes 非零而四池仍 NULL；首套 OOM 或第二套构建失败会释放已分配输出并归零。
 *
 * ## Constraints and lifecycle
 * function 与 liveness 都必须非空，输出记录必须为空或事先 Free。入口先 memset，不能用来事务替换旧分析；输出记录不得与 function 或其被读池重叠。
 * 上游须先验证 CFG、SSA、opcode 和 aggregate recipe。此函数只额外检查 deoptStates/deoptValues 的 count/capacity、非零 count 的 NULL 池、各普通 valueRange 和 ValueId，不独立验证 aggregate 池或全部函数输入。Parser 当前先 Verify 再本函数，随后才建立 owner 分析；不能把现有所有路径描述为先 OwnerAnalysisBuild。
 * 两套非空分析共执行八个 calloc 表达式；每套两个 scratch 在该套结束前释放，成功仅四个结果池归输出持有。第二套的 before/after 所有权移交至 semanticBefore/semanticAfter，不随局部记录退出释放。
 * semantic 分析只省略 DROP_IF_INITIALIZED 的 operand 用途；DROP 和 deopt 恢复用途仍保留。不读取运行时 payload，也不会让逻辑 ID 变成实际 GC root。
 * 输出不保留 function 指针或 epoch。调用方保留对应函数布局/ID 上下文，IR 改变后重建，并在独占 Build/Free 时结束旧池借用；函数不提供同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:208
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:213
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:216
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:223
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:227
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:230
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:232
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:234
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:237
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:238
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:375
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:376
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:379
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:384
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:479
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:573
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:611
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:614
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:661
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:100
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:101
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:144
 * - tests/parser/test_ssa_licm_scalar_context.inc:123
 * - tests/parser/test_ssa_licm_scalar_context.inc:134
 * - tests/parser/test_ssa_state_map_liveness.c:19
 * - tests/parser/test_ssa_state_map_liveness.c:176
 */

/**
 * @fn void ZrCore_ExecIr_StateMapLivenessFree(SZrStateMapLiveness *liveness)
 *
 * ## Summary
 * 公开声明的调用契约：释放一份存活分析所持有的普通 before/after 与 semantic before/after 四个池，并将记录清零。
 *
 * ## Parameters
 * @param[in,out] liveness 非空的唯一 owning 记录；四个池为空或各自兼容 C free，调用后记录清零。
 *
 * ## Returns
 * 无 C 返回值；完成后四指针和 rowBytes 均为零。记录对象自身及其地址继续有效，旧池与池元素的借用失效。
 *
 * ## Constraints and lifecycle
 * liveness 必须非空。各非空池须可分别 free，不得指向函数输入池、栈存储或同一分配的重叠部分；不得对同一 owning 记录的浅复制分别 Free。
 * 全零记录与先前已 Free 的记录可再次传入；尺寸溢出后只有 rowBytes 非零、四池为空的记录也可清零释放。此函数不释放 function 或 ownership。
 * 调用方须独占修改/释放该记录和池，结束所有旧池借用后再调用；没有同步或借用跟踪。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:18
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:16
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:17
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:21
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:168
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:234
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:479
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:657
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c:661
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:144
 * - tests/parser/test_ssa_licm_scalar_context.inc:134
 */

/**
 * @fn TZrBool ZrCore_ExecIr_StateMapLivenessContains(const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, TZrBool after)
 *
 * ## Summary
 * 公开声明的调用契约：查询普通存活位图中某一指令 phase 是否保留指定 ValueId；这是原始位查询，不负责校验函数身份或 ID。
 *
 * ## Parameters
 * @param[in] liveness 非空且稳定的位图记录；所选池若非空，行宽和有效 ID 范围须与原函数匹配。
 * @param[in] instruction 位图所对应函数的一基指令 ID；非空池分支不检查范围。
 * @param[in] value 位图所对应函数的一基 ValueId；非空池分支不检查范围。
 * @param[in] after 零选择 before，非零选择 after；不选择 semantic 池。
 *
 * ## Returns
 * @return 所选 before/after 池为空时返回 ZR_FALSE；非空时返回对应位是否为 1。ZR_FALSE 不区分空池与合法查询得到的未存活。
 *
 * ## Constraints and lifecycle
 * liveness 自身不能为 NULL。所选池非空时，调用方保证 instruction 与 value 均为匹配函数的有效一基 ID，rowBytes 正确且整个偏移可读；记录没有 instructionCount、valueCount 或 generation 可供校验。
 * 查询只借用记录和所选行，不转移所有权，也不延长池或函数的生命周期。借用到 Free、重建或外部修改之前有效；共享只读须排除并发写与释放。
 * NULL 行的早退不构成任意 ID、任意指针或 NULL 记录的通用安全承诺。semantic 查询由内部临时视图另行选择池。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:19
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:24
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:28
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:30
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:33
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:248
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:252
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:52
 * - tests/parser/test_ssa_licm_scalar_context.inc:126
 */

/**
 * @fn TZrUInt8 ZrCore_ExecIr_StateMapOwnerMaskAt(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 公开声明的调用契约：查询 owner may-state mask，并为 effect 之后、尚未选边且只有 cleanup 用途的可抛出终结指令结果补入 UNINITIALIZED 的可能性。
 *
 * ## Parameters
 * @param[in] function 与 ownership、liveness 同一次 IR 状态匹配的已验证函数及稳定输入池。
 * @param[in] ownership 与 function 相匹配、仍有效的 owner may-state 分析。
 * @param[in] liveness 与同一 function 的 ID、行宽和指令布局匹配的存活位图记录。
 * @param[in] instruction 该函数的一基指令 ID。
 * @param[in] value 该函数的一基 ValueId。
 * @param[in] phase 同一 checkpoint 的合法 phase；BEFORE 读 before，其余合法 phase 使用 after 语义行。
 *
 * ## Returns
 * @return 返回 owner 状态位集合，非具体 owner 枚举。底层 owner 查询拒绝参数、phase、不可达指令或无行时返回 0；BEFORE 或语义存活时保留原 mask；满足后效应结果规则时在原 mask 中 OR UNINITIALIZED。
 *
 * ## Constraints and lifecycle
 * 正常查询要求 function、ownership、liveness 来自同一未变更 IR，相关池可读且 opcode 有效。底层 mask=0 或 BEFORE 的短路不会读后面的 function/liveness，不应由此推断一般 NULL 参数支持。
 * 补位依据 OpcodeInfo 的 schema 同时含 TERMINATOR 和 MAY_THROW，且 value 位于该指令 resultRange；不是仅凭 instruction.flags 或 MAY_THROW 名称。合法 AFTER 与 CLEANUP 均可走此规则。
 * 只在非 BEFORE 且 semantic 位为零时补位，保留普通结果用途的正常边可用性；它不选择具体 successor，不执行 DROP，不读取或创建运行时 owner payload。
 * 所有参数只借用，不分配、释放或同步；返回 mask 可复制，但解释它仍须使用对应函数与 phase 上下文。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:24
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:252
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:256
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:260
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:261
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:263
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:267
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:269
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:272
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:423
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:427
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:429
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_owner_state.c:433
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:276
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:67
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:111
 * - tests/parser/test_ssa_conditional_cleanup.c:415
 * - tests/parser/test_ssa_conditional_cleanup.c:426
 * - tests/parser/test_ssa_conditional_cleanup.c:435
 * - tests/parser/test_ssa_conditional_cleanup.c:437
 * - tests/parser/test_ssa_conditional_cleanup.c:449
 * - tests/parser/test_ssa_conditional_cleanup.c:451
 */

/**
 * @fn EZrExecIrStateMapOwnerState ZrCore_ExecIr_StateMapOwnerAt(const SZrExecIrFunction *function, const SZrExecIrOwnerAnalysis *ownership, const SZrStateMapLiveness *liveness, TZrExecIrInstructionId instruction, TZrExecIrValueId value, EZrExecIrStateMapPhase phase)
 *
 * ## Summary
 * 公开声明的调用契约：把 checkpoint 的 owner mask 与语义用途合并为可记录的 owner state，允许 UNIQUE/SHARED 的 cleanup-only 非活动或条件状态，并拒绝不可用的语义读取。
 *
 * ## Parameters
 * @param[in] function 与 ownership、liveness 同一次 IR 状态匹配的已验证函数及稳定输入池。
 * @param[in] ownership 与 function 相匹配、仍有效的 owner may-state 分析。
 * @param[in] liveness 与同一 function 的 ID、行宽和指令布局匹配的存活位图记录。
 * @param[in] instruction 该函数的一基指令 ID。
 * @param[in] value 该函数的一基 ValueId。
 * @param[in] phase 同一 checkpoint 的合法 phase；BEFORE 读 before，其余合法 phase 使用 after 语义行。
 *
 * ## Returns
 * @return 精确 INITIALIZED、UNKNOWN 单位 mask 分别直接返回同名状态。其余 mask 为零或 value 仍语义存活时返回 OWNER_STATE_COUNT。语义不存活且为 UNIQUE/SHARED、mask 无 UNKNOWN 时，精确 MOVED、DROPPED、UNINITIALIZED 单位 mask 均返回同名状态；其余符合前提的非零多状态 mask 返回 CONDITIONAL。
 *
 * ## Constraints and lifecycle
 * COUNT 是不可接受状态的哨兵，不是有效 witness。INITIALIZED 和 UNKNOWN 的单值分支先于后面的 ownership/semantic 限制；不能声称所有返回状态都仅限 UNIQUE/SHARED。
 * 公开枚举中 MOVED=2、DROPPED=3、UNINITIALIZED=4、CONDITIONAL=5，故 MOVED 到 CONDITIONAL 之前的循环包含 UNINITIALIZED；单 UNINITIALIZED 在语义已死且 ownership 为 UNIQUE/SHARED 等前提满足时返回 UNINITIALIZED。语义仍活时，前面的检查先返回 COUNT。
 * 调用方保证函数、两份分析和有效 ID/phase 匹配且稳定。底层返回零的短路不证明一般损坏记录或越界 ID 可安全查询。
 * 不执行移动、释放或 DROP；不验证 runtime witness payload，不分配或延长函数/分析生命周期，无内部同步。
 *
 * ## References
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map_liveness.h:30
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:278
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:282
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:286
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:288
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:290
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:292
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:293
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:296
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:298
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_state_map_liveness.c:300
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:256
 * - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c:258
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:65
 * - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize_owners.c:72
 * - tests/parser/test_ssa_licm_scalar_context.inc:129
 * - tests/parser/test_ssa_state_map_ownership.c:96
 * - tests/parser/test_ssa_state_map_ownership.c:98
 * - tests/parser/test_ssa_state_map_ownership.c:100
 * - tests/parser/test_ssa_state_map_ownership.c:104
 * - tests/parser/test_ssa_conditional_cleanup.c:339
 * - tests/parser/test_ssa_conditional_cleanup.c:344
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:31
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:34
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:36
 * - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h:38
 */
