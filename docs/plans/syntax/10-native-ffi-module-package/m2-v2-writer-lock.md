# Syntax 10R M2.3 Canonical V2 Writer And Dependency Lock Projection

Design source: `docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md`, Syntax 10R M2.

Implementation plan: `docs/plans/syntax/10-native-ffi-module-package/m2-manifest-artifact-implementation-plan.md`.

## 状态与产出记录

- 状态：completed
- 开始时间：2026-07-24 21:43 +08:00
- 完成时间：2026-07-24 22:22 +08:00
- 已完成项目：
  - 已建立 RED：canonical manifest 排序、read-write-read、v1 migration writer 拒绝、绝对 path dependency
    与 loopback URI 拒绝，以及独立 dependency lock 的 resolved version/content hash/transitive identity/provider
    投影。
  - `ZrLibrary_ProjectManifestV2_Write` 仅写 v2 base/alias/package/dependency 声明，按 canonical key 排序；
    `ZrLibrary_ProjectManifestV2_WriteDependencyLock` 独立输出 resolved version、content hash、transitive identity
    与 provider kind，且没有 locator 输入字段。
  - 发布 writer 对所有 source kind fail closed：拒绝绝对/`file:`/UNC/drive locator、空 authority、drive-like
    authority、localhost、IPv4 loopback，以及 IPv6 loopback 的压缩、展开、IPv4-compatible、IPv4-mapped 和
    zone-id 形式；`registry` package ID 与外部 IPv6 git authority 保持可用。
  - GCC、Clang、MSVC 均通过 writer 8/8、v1 normalization 29/29、module-specifier 5/5 和相关 CTest 2/2；
    复审已关闭，未发现 P1/P0。

## Contract

- Writer 只发出 v2 spelling，固定 base 字段顺序并按 canonical alias、export 和 package key 排序。
- 可发布 manifest 不接受任意本地 locator：`path` 只允许相对声明；`registry` 只允许 package ID 或带非-loopback
  authority 的 HTTP(S) URI；`git` 只允许带非-loopback authority 的 HTTP(S)/SSH/git URI。它也不序列化
  migration-only 字段。
- Lock projection 与 manifest 分离，只写 resolved version、content hash、transitive identity 和 provider kind；它
  不接收或输出机器本地 provider cache locator。
