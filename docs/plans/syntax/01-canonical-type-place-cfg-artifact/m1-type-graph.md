# 01-M1 Type graph 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md` 的 `M1 Type graph`。

## 状态与产出记录

- 完成时间：2026-07-19 11:34 +08:00
- 状态：已完成
- 完成项目：
  - 建立不可变 canonical type graph，覆盖 primitive、nominal、type/const generic、array、tuple、union、function、nullable、ref、owner、readonly view、error 与 never。
  - 实现结构 hash、精确 equality、indexed interning、单调 `TypeId` 查找、definition/capability registry、projection 与生命周期管理。
  - 建立完整 callable parameter/receiver/effect identity，并修复函数 symbol 错用 return `TypeId` 的发布路径。
  - 将 type inference、class/struct/union compiler、semantic facts 与 LSP 接入 canonical `TypeId`；泛型 arity/kind/constraint 失败在任何可见状态发布前终止。
  - 支持 const literal 与开放 const parameter 的 owner/ordinal 身份、嵌套 union/generic 投影，以及开放 const/projected-union constructor pattern 的闭合匹配。
  - 为 source member 保存结构化 return type，闭合 generic prototype 与 LSP signature help 递归替换 type/const 参数；metadata/native 路径保留兼容 fallback。
  - 通过 capability 与 public constructor set 绑定 `init TypeRef(...)`，明确阻止 runtime `zr.reflection.Type` 进入静态 value construction 路径。
  - canonical graph 测试扩展到 18 项，并完成 parser、union、type inference、semantic facts、CFG、LSP、metadata 与 AOT guardrail 父级回归。
  - 完成 GCC 11.4、Clang 14、MSVC 19.44 三编译器 Debug 验证；最终 Critical/Important 审查为 GO（0 Critical、0 Important）。
- 验收证据：
  - `tests/acceptance/2026-07-19-syntax-01-m1-canonical-type-graph.md`
  - `docs/parser-and-semantics/canonical-type-graph.md`
  - GCC：`/home/hejiahui/zr_vm-syntax-m1-gcc-20260719-r8`
  - Clang：`/home/hejiahui/zr_vm-syntax-m1-clang-20260719-r8`
  - MSVC：`build-syntax-01-m1-msvc`
  - 暂存快照：`/home/hejiahui/zr_vm-syntax-m1-staged-gcc-20260719-r9`（canonical 18/18、type inference 118/118、LSP expression hover 全通过；仅覆盖 HEAD 已引用但未定义的 profile 基线枚举与名称，不纳入 M1 提交）
- 里程碑提交：本记录随 `feat(syntax): complete canonical type graph milestone` 一并提交。

## 边界与后继

- M1 保留 `SZrInferredType` 作为迁移期输入，不声明 artifact schema 或 consumer 迁移完成。
- 下一里程碑严格进入 M2 Place 与通用 CFG：覆盖 local/field/index/deref/tuple/union projection、通用 edge 和 place overlap 四态。
