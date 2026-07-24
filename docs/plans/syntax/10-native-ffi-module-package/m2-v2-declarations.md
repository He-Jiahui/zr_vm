# Syntax 10R M2.2 V2 Package, Alias, And Dependency Declarations

Design source: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`, Syntax 10R M2.

Implementation plan: `docs/plans/syntax/10-native-ffi-module-package/m2-manifest-artifact-implementation-plan.md`.

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 21:28 +08:00
- 完成时间：2026-07-24 21:43 +08:00
- 已完成项目：
  - 已新增 M2.2 RED/Green：v2 aliases、package exports、path/registry/git dependencies、未导出包入口、legacy
    fields、递归 alias、未声明 package target 与多源 dependency 均有 focused 覆盖。
  - 已完成 structured declaration reader 与 v1/v2 reader split；`project.c` 只负责 v2 生命周期安装和释放。
  - Alias resolution 保留 Workspace、Native、Package 和 File domains；File 后缀仅允许目录或 `.zrp` target，`.zr`
    与 `.zrm` terminal target 均拒绝。
  - GCC、Clang 与 MSVC 均通过 v2 5/5、v1 normalization 29/29、M1 module-specifier 5/5，以及
    `project_manifest_v2|project_module_specifier` CTest 2/2。
  - 最终只读复审已关闭 File alias terminal target 缺口，未发现新的实际问题。

## Contract

- Alias 保存完整 `#root` 与 M1 `ModuleSpecifier` target；解析可保持 Workspace、Native、Package 或 File
  domain，不能将目标降级为裸路径。
- Package 只接受根 `@identifier`，exports 只公开 `.` 与 `./logical.module`。不存在的 export 不会回退到
  文件名或文本匹配。
- Dependency 保存 `ModuleIdentity`、version requirement、唯一 source kind/source；本里程碑不加载 source
  或选择 provider。
- v2 禁止迁移字段 `pathAliases`、`references`、`dependency` 与 `local`，v1 的 `@`、`$`、`&` resolver 路径
  保持独立。

## Acceptance

- GCC: focused build and test completed with v2 5/5, v1 29/29, M1 5/5, CTest 2/2.
- Clang: focused build and test completed with v2 5/5, v1 29/29, M1 5/5, CTest 2/2.
- MSVC Debug: focused rebuild and test completed with v2 5/5, v1 29/29, M1 5/5, CTest 2/2.
- Exact-path commit is recorded after staging this milestone's source, tests, module documentation, plan, and
  acceptance record.
