# Syntax 01 M5 canonical consumers acceptance

对应计划：`docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md`
的 `M5 consumers`。

## Scope

- VM module 与 AOT backend 共享同一个 canonical artifact projection 和错误状态。
- reflection、debug、layout 只按 token/TypeId 查询，不使用类型名。
- expression/call semantic facts 携带 canonical TypeId；LSP hover/signature/diagnostics
  消费 compiler semantic context。
- 保持 M1-M4、compiler、AOT、reflection/debug 和完整 LSP interface 回归。

## RED evidence

1. 新增 consumer 测试后，首次 MSVC build 因缺少
   `backend_aot_canonical_artifact.h` 失败，证明 VM/AOT 尚无共享入口。
2. 泛型 `identity<T>(value: T)` 端到端用例首次在 `CallAt` 失败；事实范围只覆盖 callee，
   未覆盖 `identity(42)` 参数位置。
3. 扩展参数范围后，`zero()` 空括号内查询仍失败；parser 的 function-call node 错误复用
   primary 起始 range。
4. 修正 call range 后，last-good incomplete-edit signature test 得到 `pick: int`；同一范围
   先存在 LSP 返回类型 fact，后存在 compiler canonical function fact，查询错误依赖写入顺序。

## GREEN implementation

- 新增 `SZrCanonicalConsumerProjection` 与 token/TypeId/layout resolver；root TypeRef、
  TypeSpec、Signature、TypeDef、Layout、Contract 必须结构一致。
- VM module 与 AOT adapter 委托同一个 core open；失败 status 和 expected/actual hash 相同。
- reflection、debug 与 layout API 只接收稳定 identity。
- expression fact 自动取得 TypeId；resolved call 使用实例化后的参数/passing/return 重新驻留
  function TypeId，并生成 compiler-owned declaration label。
- `CallAt` 只选择 canonical function node并优先 compiler signature；hover 移除
  `TypeNameString` 回退。
- parser 为普通/显式泛型 call 保存精确括号 range；call fact 覆盖参数和空调用位置。
- 新增调用事实模块，避免 `type_inference_semantic_facts.c` 超过千行边界。

## MSVC verification

环境：MSVC 19.44.35228.0，Debug，`/W4`。

已通过 18 个目标。可计数套件共 273 项：artifact 13、canonical consumers 4、
canonical graph 18、Place/CFG 4、pre-SemIR 6、semantic query 16、compiler query
diagnostics 16、reference facts 6、expression hover 6、call/member 4、LSP query
diagnostics 14、reflection 30、debug metadata 4、AOT value SemIR 4、AOT layout 1、
compiler integration 127；此外完整 local semantic query 与 LSP interface 套件通过。

## GCC and Clang verification

- GCC 11.4.0：18/18 target，`GCC_M5_MATRIX_PASS`。
- Clang 14.0.0：18/18 target，`CLANG_M5_MATRIX_PASS`。
- 两套快照的 27 个 M5 实现、测试与 CMake 文件与 Git index blob 逐文件一致；
  3 份收口 Markdown 在验证后生成，不属于该构建输入：
  `M5_INDEX_MATCH files=27`。
- GCC 快照：`/home/hejiahui/zr_vm-syntax-m5-staged-gcc-20260719-r1`。
- Clang 快照：`/home/hejiahui/zr_vm-syntax-m5-staged-clang-20260719-r1`。

两个纯 index 快照首次都在既有 `value.h` 的
`ZR_PROFILE_HELPER_VALUE_CONSTRUCT` 引用处失败。当前 HEAD 尚未包含另一个会话正在完成的
`profile.h/profile.c` 配套枚举/计数实现，因此复用 M4 已记录的前置处理，只叠加工作树中的
这两个非 M5 文件后重建。该叠加不改变上述 27 个构建输入 blob；最终两套 648-step build 和
18-target test matrix 均通过。

## Promotion gate

- VM/AOT projection 和失败行为一致：PASS。
- LSP hover/signature/diagnostics 与 compiler semantic context 同源：PASS。
- reflection/layout 不按名字猜测：PASS。
- staged production diff 未新增 concrete built-in type-name dispatch：PASS。
- `git diff --cached --check`：PASS。

结论：M5 promotion gate 为 GO，Critical 0，Important 0。
