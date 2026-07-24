# Syntax 10R M1 Specifier Foundation Record

Design source: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`, 10R M1.

Implementation plan:
`docs/plans/syntax/10-native-ffi-module-package/m1-specifier-foundation-implementation-plan.md`.

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 19:52 +08:00
- 完成时间：2026-07-24 20:33 +08:00
- 已完成项目：
  - 新增 domain-aware `ModuleSpecifier`/`ModuleIdentity` public API，并以独立 source module 避免继续扩张
    1334 行的 legacy project import resolver。
  - 覆盖 `zr`、`native:`、Workspace、relative、`#alias`、单段 `@package` 和 POSIX/drive/UNC canonical
    `file:` locator 分类；native 与同名 Workspace identity 保持不相等。
  - relative resolution 支持任意连续 `../` 与等价连续点拼写，继承 Workspace/Package domain 与 package
    root，越过 module root 时显式拒绝；Package root 自身是有效 identity，但没有模块叶子，不能作为
    relative-resolution caller。
  - 审查后以 RED/GREEN 收口 multi-level relative parsing、root escape rejection、package-root structural
    equality 和 output/input identity aliasing；GCC、Clang 与 MSVC 最终均通过 5 tests / 0 failures 和 1/1
    focused CTest，GCC 还重跑既有 project import resolver 9 tests / 0 failures。
  - 建立 module-system 文档、acceptance evidence 和 M1 implementation plan。

## Boundary

本里程碑不读取 manifest、不展开 alias、不选择 source/binary/descriptor/artifact provider，也不将 `file:`
locator 转为 public identity。`.zrp` v2、package version/lock、provider phase、artifact roundtrip 和 legacy
resolver 的目标语法接管仍由 10R M2 负责。
