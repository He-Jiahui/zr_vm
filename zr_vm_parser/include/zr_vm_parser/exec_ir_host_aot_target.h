#ifndef ZR_VM_PARSER_EXEC_IR_HOST_AOT_TARGET_H
#define ZR_VM_PARSER_EXEC_IR_HOST_AOT_TARGET_H

#include "zr_vm_parser/semantic.h"
#include "zr_vm_core/aot_ir.h"

/**
 * @par Summary
 * 为当前宿主上的无参数、无接收者、无效果且返回 INT64 的规范函数构造
 * 独立的 SZrAotIrTargetContract。目标只描述有限的 Win64 C callable ABI；
 * 不产生模块、descriptor、源代码、可执行文件或具有保留所有权的 artifact，
 * 不建立 Common 完整平台 ABI，也不证明函数体能够通过 native backend。
 *
 * @par Parameters
 * @param context 借用当前已初始化的语义上下文及其真实 canonical interner 快照。
 * canonicalTypes 的节点按 ID 排序；节点及其内部容器均由该上下文的真实 owner
 * 管理。调用者必须提供仍然有效且可读的完整存储，不能用伪造头部或任意地址
 * 替代类型图。此参数为 NULL 时拒绝。
 * @param callableTypeId 同一快照中的非零 FUNCTION 身份。节点 structuralHash
 * 必须非零，parameterContracts.length 为零，receiverEffect 为 NONE，
 * effectFlags 为 NONE；returnTypeId 必须引用真实的 PRIMITIVE INT64 节点。
 * @param returnLayout 借用同一快照中实际返回类型的宿主布局行。id 必须非零，
 * typeToken 必须等于 callable 的 returnTypeId。producer 将使用现有
 * MakeHostPrimitiveLayout 以该 typeToken 和 id 重算，并逐项比较 id、typeToken、
 * byteSize、byteAlign、layoutHash；调用者提交的非零哈希本身不构成验证依据。
 * 此参数为 NULL 时拒绝。
 * @param output 独立且可写的完整目标记录；无需预先初始化。仅在所有验证通过后
 * 一次性发布完全初始化的候选；失败保持该记录的每一个字节不变。NULL 时拒绝。
 * @param diagnostic 可为 NULL；非空时必须指向独立且可写的完整诊断记录。
 * 入口可清零，失败写入 AOTIR status。
 *
 * @par Returns
 * ZR_TRUE 表示输出已发布且通过 ZrCore_AotIr_ValidateTarget；ZR_FALSE 表示
 * 拒绝或尚不支持，输出与输入源对象均保持不变，可读取非空 diagnostic。
 * 诊断分配：空必需指针为 INVALID_ARGUMENT；无效或未驻留
 * callable ID 为 INVALID_ID；非 FUNCTION 或其 structuralHash 为零为
 * INVALID_SIGNATURE；参数、接收者、效果、非 INT64 返回或宿主属性不支持为
 * UNSUPPORTED；行 ID 为零、返回类型不符或重算字段不同为 INVALID_LAYOUT；
 * canonicalTypes 形状、length/capacity 或乘积/地址跨度错误为 INVALID_RANGE；
 * 任一目标哈希为零为 INVALID_TARGET。成功诊断全部清零，即 status=OK。
 * functionId/blockId/instructionId 保持零；矛盾的 expected/actual 使用真实值，
 * index 可标注输入 callableTypeId，此 API 没有图中函数或指令身份。
 *
 * @par Constraints and lifecycle
 * 每个非空 context、returnLayout、output、diagnostic 对象都必须独立有效、
 * 正确对齐并在整个调用期间存活。context 及其所有可达存储、布局行、输出和
 * 诊断的存储必须互不重叠；诊断清零不能覆盖任何源对象或输出。调用者维持
 * 同一快照：不得混用其他上下文或 Reset 前的 ID/行，也不得在调用期间增长、
 * Reset、Free 或并发改写类型池。shape、capacity 乘积和地址跨度的检查只验证
 * 元数据数值关系，不能证明任意指针可读，更不能替代上述调用者前置条件。
 * producer 不分配内存、不接管 owner、不保留输入指针，不修改 context 或行。
 * 成功目标仅含标量值，不借用类型图；它不延长任何 AOT projection 的生命周期。
 * 零参数只要求 parameterContracts.length 为零；真实 interner 仍可持有非空
 * head 或小容量存储，不能因此拒绝无参数函数。
 *
 * 宿主白名单要求 _WIN32、_WIN64、_MSC_VER，且 _M_X64 或 _M_AMD64；
 * ARM64、ARM64EC 与其他宿主均不支持。实际 CHAR_BIT 必须为 8，指针大小为
 * 8 字节，TZrInt64 大小与对齐均为 8 字节，实际字节序必须为 little endian。
 * 目标写入现有 AOT target ABI version、pointerSize=8、endianness=0、
 * requiredCapabilities=0；此零能力字段不授予任何运行时能力。
 *
 * 冻结 schema 1 使用 Stable64，输入为连续显式 ASCII 和 little-endian
 * 固定宽度字节，域与 triple 均不含 NUL：
 * - targetTripleHash: 域 "zr.aotir.target-triple"，u32 schema=1，u32 triple
 *   ASCII 字节数，固定 "x86_64-pc-windows-msvc" 的实际 ASCII 字节。
 * - abiHash: 域 "zr.aotir.host.noargs-i64.abi"，u32 schema=1，u32 现有
 *   ZR_AOT_IR_TARGET_ABI_VERSION，u64 targetTripleHash，u32 pointerSize=8，
 *   u32 CHAR_BIT=8，u32 endianness=0，u32 ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64，
 *   u32 parameterCount=0，u32 implicitParameterCount=0，u32 signedReturnTag=1，
 *   u32 returnBitWidth=64，u32 returnByteSize=8，u32 returnByteAlign=8，
 *   u32 Win64 C direct-I64-in-RAX ruleTag=1。
 * 任一结果为零即拒绝，不能改写为 1。哈希不含原始 struct/padding、地址、时间、
 * salt、上下文局部 ID、布局行 ID、structuralHash、literal bits 或 frame hash；
 * 不调用 Common DetectHostAbi/ComputeAbiHash，不接受调用者提供的 triple。
 *
 * @par References
 * ZrParser_CanonicalType_Find 的借用节点只能在当前快照中消费；调用者从真实
 * parser/semantic 管线取得 callable 身份并维持 owner。调用者使用
 * ZrParser_ExecIr_MakeHostPrimitiveLayout 构造实际返回行，另行负责追加布局表，
 * 构造 frame 并调用 ZrParser_ExecIr_LowerAotWithCanonicalCallable，保持其真实
 * NOP、ID、源映射及状态元数据。本入口不替这些步骤提供证明。目标的结构验收
 * 使用 ZrCore_AotIr_ValidateTarget，模块 contract/binding 仍由独立消费方负责。
 * 有限 ABI 依据为 lua/Rust/compiler/rustc_target/src/spec/targets/
 * x86_64_pc_windows_msvc.rs、callconv/x86_win64.rs 与 callconv/mod.rs，及
 * lua/Mono/mono/mini/mini-amd64.c 的 I8/U8 返回寄存器规则；它们不是本项目
 * native 执行验收。计划见 .codex/plans/20261005-ssa-host-aot-target.md。
 */
ZR_PARSER_API TZrBool ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
        const SZrSemanticContext *context, TZrTypeId callableTypeId,
        const SZrExecIrLayout *returnLayout, SZrAotIrTargetContract *output,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_EXEC_IR_HOST_AOT_TARGET_H */
