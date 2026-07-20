# 03-M3 ref struct restrictions 里程碑记录

对应计划：`docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md`
的 `M3 ref struct restrictions`。

## 状态与产出记录

- 完成时间：2026-07-21 05:05 +08:00
- 状态：已完成
- 完成项目：
  - 完成 `ref struct` / `readonly ref struct` 语法、AST modifier 与 canonical
    `REF_LIKE` capability；ref-like 类型不再进入 boxed construction。
  - 完成独立 ref-struct rule pass，静态拒绝 heap/resource/static/plain-struct field、
    array、module/global、unconstrained generic、boxing 与 native opaque ABI 存储。
  - 将 ref-like provenance 接入 reference escape，覆盖参数/局部/构造参数来源、返回、
    closure capture 与 await/yield suspension，允许安全 parameter forwarding 和无 ref
    来源的 ref-struct 返回。
  - 冻结 frame layout 的 GC/ownership/ref 三类精确 map：语言 ref 使用
    `VALUE_SLOT | REF_VALUE`，owner field 保持 `GC_VALUE | OWNERSHIP_VALUE` 与 fieldwise
    drop；parser/core 共享 `REF_FIELD` artifact bit。
  - 新增 10 项 ref-struct 专项合同，并扩展 runtime GC survival、layout、metadata、
    escape、constructor、artifact、canonical consumer 与 compiler integration 回归。
  - 在固定 `ef7c6e1 + M3 exact paths` 隔离快照上完成 GCC、Clang、MSVC 三工具链
    9/9 进程验收；每套 Unity 合计 253 Tests、0 Failures，所有进程真实退出码为 0。

## 当前验收边界

- ref-like 类型只能位于 local、temporary、value/scoped parameter、安全返回和其他
  ref struct 的 inline field；普通 class/resource class/struct heap storage、GC array、
  module/global、unconstrained generic、boxing、escaping closure、await/yield 与 native opaque
  ABI 必须在静态阶段拒绝。
- ref struct 可包含语言 ref、nested ref struct、GC handle 和 owner value；GC handle 必须继续
  进入精确 frame GC map，owner field 必须继续进入 ownership/drop map。
- 第一阶段不引入 `allows ref struct` anti-constraint，不按 `Span`/`ReadOnlySpan` 名称特判。
