# Syntax 10R M2.4 Artifact Entry And Provider Phase Bridge

Design source: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`, Syntax 10R M2.

Implementation plan: `docs/plans/syntax/10-native-ffi-module-package/m2-manifest-artifact-implementation-plan.md`.

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 22:37 +08:00
- 完成时间：2026-07-24 22:52 +08:00
- 已完成项目：
  - 已建立 `.zrm` provider phase 与 public contract hash 的归档读写 RED/GREEN；旧归档缺少
    `providerPhase` 时保持 Runtime 默认值，未知 phase 拒绝打开。
  - 已让 provider-location 与 AOT load-request 返回 artifact kind、provider phase、archive entry 和
    public contract hash，而不把这些物理提供方事实并入 `SZrLibrary_ModuleIdentity`。
  - 已建立 Runtime 对 CompileTool archive 的拒绝边界：AOT request 和 source loader 都在加载前失败；
    Runtime archive 的 package-root 默认 entry 可读取原始 `.zro` bytes。
  - 已收紧 archive admission：显式空值、`null` 或非字符串 phase 不会降级为 Runtime；writer 与 reader
    都要求默认 entry 确实指向 `modules` 中的一个 module entry。
  - GCC、Clang、MSVC Debug 均通过 ZRM 6/6、import resolver 9/9、manifest v2 8/8、manifest
    normalization 29/29、module specifier 5/5，以及已注册 CTest 2/2。复审已关闭，未发现 P1/P0。

## Contract

- `.zrm` manifest 的 `assembly.providerPhase` 只允许 `runtime`、`test` 或 `compileTool`；缺省值为
  `runtime` 以兼容既有 `zr.zrm/v1` 容器。
- `SZrLibrary_ProjectImportProviderLocation` 与
  `SZrLibrary_ProjectImportProviderAotLoadRequest` 分别携带 provider kind、phase、artifact entry 和
  contract hash。它们是 provider 选择事实，不能修改或替代 module identity。
- Runtime 入口只能消费 Runtime provider。非 Runtime `.zrm` 在创建 AOT request 或读取其 `.zro`
  entry 之前必须失败；不会按文件名、入口文本或 locator 推断 phase。
