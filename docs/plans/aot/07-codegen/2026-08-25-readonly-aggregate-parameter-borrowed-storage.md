---
plan_id: aot-07-codegen
record_id: 2026-08-25-readonly-aggregate-parameter-borrowed-storage
status: completed
completed_at: 2026-08-26 00:47:37 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Readonly Aggregate Parameter Borrowed Storage

## 状态与产出记录

- 完成时间：2026-08-26 00:47:37 +08:00
- 状态：A7.2P readonly aggregate parameter borrowed storage 子里程碑完成；A7.2、AOT 07、AOT 12 与 AOT
  07~12 总目标继续进行。
- 完成项目：源码声明的 inline aggregate `in`、`ref readonly` 与 `scoped ref readonly` 参数投影为 callee
  borrowed frame storage，物理槽固定为 `INLINE_STRUCT` 和
  `ALIAS | INDIRECT_ALIAS | BORROWED_ALIAS`，并复用既有 borrowed-alias runtime/artifact contract。
- 完成项目：普通已知 free call 在包含 readonly inline aggregate 参数时使用独立连续 callable/argument window；
  callable 通过 `SET_STACK` 搬入，窗口按参数 TypeLayout/readonly 物理签名复用，嵌套 active window 不参与复用，
  消除相邻调用 slot metadata 污染，同时避免 frame slot 随相同签名调用次数线性增长。
- 完成项目：active readonly call window 中的显式 struct-init 参数使用独立 high-water constructor/result pair，
  再把结果复制到参数槽，避免构造器隐式 `targetSlot - 1` callable 槽覆盖外层调用 callable；嵌套标量调用继续使用
  VALUE 参数槽，并与外层 readonly aggregate 参数槽保持不同物理身份。
- 完成项目：ExecIR frame verifier 在裁剪前将 canonical parameter role/type identity 与 physical frame storage
  交叉校验；readonly aggregate 缺少 borrowed storage、被降级为 VALUE 或 known VALUE role 携带 borrowed storage
  均 fail closed，不可达 malformed owner 不能被 C/LLVM code stripping 掩盖。
- 计划映射：完成 AOT 07 A7.2P 与 AOT 12 的裁剪前 frame-owner 校验子切片；不新增 VM runtime ABI、opcode、
  serialized artifact schema、manifest 字段或 ExecIR schema。既有 `SZrFunctionTypedTypeRef` 字段现在对该参数
  填入 canonical struct/layout identity；仅 compiler in-memory stack-slot hint 增加 readonly aggregate、window
  signature 与 active lifecycle markers。

## 代码与文档产出

- `compile_expression_types.c` 从源码 parameter AST 读取精确 passing form，仅对 source-declared、non-receiver、
  non-spread、non-tail 的普通 free call 启用隔离窗口；相同物理签名复用空闲窗口，active 嵌套窗口保持独立，
  imported/member/spread 等不属于该路径的调用不写 readonly marker。
- `compiler_typed_metadata.c` 生成 caller inline argument hint 和 callee borrowed parameter frame row，同时把已解析
  TypeLayout identity 回填到现有 typed binding TypeRef，供后端 verifier 使用。
- `backend_aot_exec_ir_frame.c` 以 parameter prefix 中的 canonical typed binding 为权威输入，拒绝 role/type/layout/
  storage 组合漂移；writer 继续在 stripping reachability 生效前构建并验证完整 ExecIR function tree。
- focused fixture 覆盖 slot 0 与非零参数槽、`in/ref readonly/scoped ref readonly`、readonly -> scalar -> readonly
  混合调用、`in int + in Snapshot` 重复混合签名、嵌套标量调用、显式 struct-init 参数、相同签名窗口复用、
  非窗口 marker 拒绝、interpreter 结果 47、
  `.zro`/AOT C/AOT LLVM 写出、三个物理/角色负例、zero/partial table omission/downgrade 与不可达 owner writer
  负例；结构断言同时锁定 struct-init constructor receiver 与外层 readonly argument 不同槽。
- `compile_expression_types.c` 与 `compiler_typed_metadata.c` 已超过 1000 行，但本切片只扩展既有 call compilation、
  stack-slot metadata 和 frame projection 责任；未引入新的模块责任，因而没有为本次窄改动拆文件。

## 验证结果

- TDD 冻结 RED 基线为 `HEAD=6dea202`；最终组合验证基于已提交语义改动同步到 `6a0e8a9` 后叠加 A7.2P，且
  `6a0e8a9..2b135b2` 未触碰本切片受控文件。首轮 focused RED 为 27 项、2 failures：旧实现复用 ordinary staging slot，且将
  readonly aggregate physical row 降级为 VALUE 后仍可绕过 verifier。复审加强 RED 同为 27/2：相同 readonly
  物理签名仍分配槽 18/27，且 partial frame table 省略 canonical readonly 参数行被错误接受。
- 最终复审前的 direct struct-init RED 为 27 项、3 failures：`inspectIn(init Snapshot(2))` 直接把 struct-init
  编译到外层 argument slot 时，其 `targetSlot - 1` 构造 callable 覆盖了外层 callable，后续源码绑定报
  `Identifier 'temporaryValue' not found in current scope`。high-water constructor/result pair 修复后聚焦套件恢复
  27/0，新增结构断言证明 constructor receiver 与外层 readonly argument 不同槽；`inspectOffset(identity(1),
  snapshot)` 同时证明嵌套 scalar 参数保持 VALUE 且不占用外层 readonly argument 槽。
- 最终独立复审继续发现 `in int` scalar prefix 会被误写 readonly aggregate marker，导致下一次相同混合签名
  无法复用窗口；新增连续 `inspectOffset(prefix: in int, value: in Snapshot)` RED 后聚焦套件为 27/1，槽断言
  `Expected 33 Was 39`。marker 写入增加 resolved inline TypeLayout 门禁后恢复 27/0，两个 scalar 参数槽均为
  VALUE、两个 Snapshot 参数槽均为 INLINE_STRUCT，且分别复用相同物理槽。
- WSL GCC 11.4.0 通过 generic typed-call 27/0、compiler integration 127/0、SemIR 13/0、AOT C stripping
  37/0 与 AOT LLVM stripping 3/0。receiver boundary 为 28 项、3 个既有 baseline failures、无新增失败。
- Windows MSVC 19.44.35228 x64 Debug 通过 generic typed-call 27/0/6 ignored、SemIR 12/0、AOT C stripping
  37/0 与 AOT LLVM stripping 3/0。Unix private ExecIR symbol 负例在 Windows 按平台 guard 跳过，公共 writer、
  interpreter、physical frame 和 artifact 路径仍执行。
- Clang 14 最终重新编译通过 `compile_expression_types.c` 与 focused test object，仅报告该生产文件既有的两个
  unused-static-function 告警；此前完整变更对象也已编译通过。最终链接仍因仓库既有 C11 inline/static archive
  `ZrCore_*` 未解析符号失败，因此不声明 Clang executable test pass。
- 首轮独立复审发现 reused call-window metadata、physical VALUE downgrade bypass、Windows private symbol visibility
  和缺少 pre-strip unreachable owner 证明；第二轮发现 partial table omission、out-of-scope marker 与窗口线性扩张；
  实现与测试逐项收紧。最终独立复审结论：无 Critical、Important 或 Moderate findings；direct struct-init
  callable 隔离、混合 scalar/aggregate marker、窗口复用与裁剪前反向校验均闭合。

## 未完成边界

- tail call、receiver/direct member call、imported/spread declaration-less callsite 尚未进入 readonly aggregate
  parameter borrowed staging。
- caller 原始 Place/provenance、可写 `ref/out` storage 与 writeback、aggregate return/destination、spill/
  address-taken slot、GC/debug map 继续开放。
- 本切片验证 interpreter 行为以及 `.zro`、AOT C、AOT LLVM 产物写出，但未执行新 readonly parameter fixture
  生成的 C/LLVM 产物；generated execution parity 保留为后续 A7.2 扩展证据。
- 本切片不完成完整 CallableContract frame derivation、A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
