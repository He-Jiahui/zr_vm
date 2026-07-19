# 01-M3 前置 Semantic IR 与 facts 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md` 的 `M3 前置 Semantic IR 与 facts`。

## 状态与产出记录

- 完成时间：2026-07-19 19:14 +08:00
- 状态：已完成
- 完成项目：
  - 建立独立于 execution SemIR sidecar 的前置 `SZrSemanticIrFunction`，统一持有 symbol/callable identity、locals、Place graph、CFG、Values、instructions、operands、regions、cleanup scopes、source map 与 loan facts。
  - 建立 41 类规范指令，覆盖 constant/conversion、Place、load/store/init/move/copy/drop、borrow/reborrow/deref、四类 call、control flow、scope/cleanup、value/aggregate/union/GC/ownership construction、property 与 evaluate-once destructuring。
  - 所有 instruction 使用稳定 `TypeId`、`PlaceId`、`ValueId`、`LoanId`、region/cleanup/block identity；VM stack slot 仅存在于私有 compiler bridge。
  - 让普通 local initialize/read/write、compound read/write、百分号 ownership directive 与点式 ownership operation 先生成前置 SemIR，再由语义 opcode 和显式 ownership operation 选择 `GET_STACK`、`SET_STACK` 或精确 `OWN_*` ExecBC。
  - 删除 ownership 的 default-to-`OwnConstruct` fallback；unique/share/weak/upgrade 显式编码，detach 映射 Move，release 映射 Drop，borrow 映射共享 loan，loan 映射可变/排他 loan。
  - 将旧 `compiler_semir.c` 明确限定为 final assembly 后的 execution compatibility projection，并在其生成前验证前置 SemIR。
  - 实现结构校验，拒绝悬空 Place/Value/Loan/Region/Cleanup/block 引用、非法 operand span、非连续 instruction/source-map identity、无效 CFG range/edge 与 ownership/loan 不一致。
  - 实现 CFG fixed-point flow facts，分别维护 initialization、availability、borrowing、escape 与 reachability；join 过滤不可达前驱并对 shared/mutable loan 保守合并。
  - 覆盖 definite assignment、move、loan、escape block join 负例，以及 store-after-move、mutable-loan read fence、exact end-loan 和 multiple-loan join 的回归测试。
  - 建立完整 opcode-family golden、source-level local golden 与 ownership 两种表层语法 golden；最终 `zr_vm_pre_semantic_ir_test` 为 6/6。
  - 完成 MSVC 19.44 工作树兼容验证，以及 GCC 11.4 / Clang 14 r5 暂存快照 15 目标矩阵；最终 Critical/Important 审查为 GO（0 Critical、0 Important）。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-01-m3-pre-semantic-ir.md`
  - `docs/parser-and-semantics/pre-semantic-ir-flow.md`
  - GCC 暂存快照：`/home/hejiahui/zr_vm-syntax-m3-staged-gcc-20260719-r5`
  - Clang 暂存快照：`/home/hejiahui/zr_vm-syntax-m3-staged-clang-20260719-r5`
  - MSVC 工作树：`build-syntax-01-m1-msvc`
  - GCC r5 的 `GCC_R5_INDEX_MATCH` 逐字节确认所有暂存 M3 文件等于 Git index；三套环境均通过 pre-SemIR、Place/CFG、union、reachability、finally、try/catch、typed catch、loop flow、dataflow、parser、type inference、semantic facts、compiler integration 与两组 AOT contract 共 15 个目标。
  - WSL 快照仅额外叠加当前非 M3 的 `profile.h/profile.c` 构建前置基线，该边界已在 acceptance 中明确记录。
- 里程碑提交：本记录随 `feat(syntax): complete pre-execution semantic IR milestone` 一并提交。

## 边界与后继

- M3 的 local Place、block entry/exit facts、loan origin/last-use 与 flow state 仍为编译会话数据，不进入 `.zro` ABI。
- M3 保留 execution SemIR/AOT/deopt compatibility projection；VM、AOT、LSP、reflection 与 debug 的全面 canonical consumer 迁移属于 M5。
- 下一里程碑严格进入 M4 artifact schema：完成 `.zrs/.zri/.zro` roundtrip、TypeRef/TypeSpec/Signature/Layout/Contract hash mismatch 诊断、安全拒绝 malformed artifact，并证明 source compile 与 binary import 产生等价 TypeId/public contract。
