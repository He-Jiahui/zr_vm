---
plan_id: aot-12-stripping
record_id: 2026-07-30-native-import-contract-reachability
status: completed
completed_at: 2026-07-30 13:16:44 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 11-A11.2A / 12-S1K / 12-S3H / 12-S6J Native Import Contract Reachability

## 状态与产出记录

- 完成时间：2026-07-30 13:16:44 +08:00
- 状态：native import contract reachability 子里程碑完成；AOT 11 A11.2 与 AOT 12 S1、S3、S6 仍为部分完成，
  AOT 07~12 总目标继续进行。
- 完成项目：reason schema 增加 `NATIVE_IMPORT` 与稳定名称 `edge.native_import`；该 reason 仅用于 function owner
  到 canonical contract node 的边，function-to-function graph 明确拒绝它。
- 完成项目：AOT C writer 在 ExecIR 与 code stripping 前递归校验完整 function tree 的 native import contract；
  不可达 owner 上的损坏 schema/hash/ABI/policy、nonempty/null table 与 count overflow 均 fail closed。
- 完成项目：裁剪后按 stable flat function index 与 contract index 发布版本 1 `nativeImportManifest`，包含
  `symbolId`、`callable.contractHash`、owner、`edge.native_import` 与 predecessor。
- 完成项目：独立报告 `nativeImportsBefore/After/Removed`；验收 fixture 证明四个原始 contract 裁剪为三个，
  owner 0 的两个 contract 与 sparse owner 2 的一个 contract 保持稳定顺序，owner 1 的 unreachable entry point
  不进入最终 contract table。
- 计划映射：AOT 11 A11.2 canonical FfiSignature contract 的 AOT C 消费/校验子切片，以及 AOT 12 S1 native
  node schema、S3 owner-linked trim、S6 reason/count reporting 子切片。

## 代码与文档产出

- `backend_aot_c_native_imports.h/.c` 增加 pre-ExecIR tree validator、加强 table/count validation，并增加稳定
  retained-contract manifest writer。
- `backend_aot_function_table.c` 拒绝 `indexSpace > capacity`，避免损坏表驱动超容量 manifest 扫描；native
  import count 路径保留同一防线。
- `backend_aot_reachability.h/.c` 增加 contract-node reason 和共用 stable reason-name API；function graph edge
  分类保持收敛，不接受 native import contract reason。
- `backend_aot_c_emitter.c` 接入裁剪前后 contract count、removed delta、manifest publication 与失败清理。
- `test_aot_reachability.c` 覆盖 reason 名称、unknown sentinel、错误 function-edge 使用与 function-table
  index-space 上界；`test_aot_c_code_stripping.c` 覆盖 4→3 trim、跨 owner/contract 稳定顺序、range/table 输出和
  不可达损坏 contract。
- 模块文档同步 `aot-function-reachability-manifest.md` 与 `ffi-extern-declarations.md`；验收入口为
  `tests/acceptance/2026-07-30-aot-11-12-native-import-contract-reachability.md`。

## 验证结果

- RED：冻结 WSL GCC 上原 26 项全部通过；新增两项分别因缺少 native import 统计/manifest、以及不可达损坏
  contract 被错误接受而失败，结果 26 pass / 2 fail。
- WSL GCC 11.4、WSL Clang 14.0、Windows MSVC 19.44 均完成 focused build；直接运行 reachability 34/0、
  code stripping 28/0。
- 八份生产代码/测试文件在主工作树、WSL 冻结树与 Windows 冻结树的 SHA-256 完全匹配。
- 生成 C 报告 native import 4→3，按顺序发布 `(ownerFunction,contractIndex,symbolId)` 为
  `(0,0,0x...0101)`、`(0,1,0x...0102)`、`(2,0,0x...0301)`；range table 为 `(0,2)`、`(2,0)`、
  `(2,1)`，所有行均为 `reason=edge.native_import` 且 predecessor 等于 owner。`trimmed_native` 不存在，损坏
  负例文件不存在。
- 相邻 `native_extern_contract` 构建成功并为 22 pass / 5 fail；五项均在 active syntax cutover 拒绝 `%import`
  与无 `fn` lambda 时失败，早于 writer/native import reachability 路径。
- 独立审查先后发现显式与 `flatIndex` 派生的损坏 `indexSpace` 可触发超容量扫描，以及跨 owner/contract
  稳定顺序缺少聚焦覆盖；三项均已修复并加入回归，最终复审无发现。MSVC 仅保留冻结 `%TEMP%` 构建目录
  触发的既有 MSB8029/MSB8064 warning。

## 未完成边界

- 本切片不新增或迁移 `.zro/.zrm` NativeImportTable section schema，不完成 source/binary/assembly provider parity、
  public contract hash、version migration 或 cross-target FfiSignature golden。
- native import thunk body、missing library/symbol runtime behavior、callback descriptor root 与 cross-module
  ModuleIdentity closure 不在本子里程碑内。
- S4 metadata pool/remap、S5 compacted artifact publication、完整 bytes/behavior parity、四 backend 和 AOT 07~12
  总验收仍开放。
