---
plan_id: aot-12-stripping
record_id: 2026-07-30-package-method-export-required-root
status: completed
completed_at: 2026-07-30 11:12:37 +08:00
source_plans:
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1H / 12-S2L / 12-S3E / 12-S6G Package Method Export Required Root

## 状态与产出记录

- 完成时间：2026-07-30 11:12:37 +08:00
- 状态：package method export required-function root 子里程碑完成；AOT 12 的 S1、S2、S3、S6 仍为
  部分完成，AOT 07~12 总目标保持进行中。
- 完成项目：package manifest 中带 current-module `MemberDef` binding 的 `METHOD` declaration 通过唯一 typed
  callable symbol 解析为 `root.package_export`，即使该函数不是 source-language export 也不会被裁剪。
- 完成项目：method export 缺少 binding、token 非 `MemberDef`、symbol 缺失/歧义或 callable 无法映射时，graph
  construction 与公共 AOT C writer fail closed，并删除半成品输出。
- 完成项目：`TYPE` 与 `FIELD` declaration 不形成 function root，unknown declaration kind fail closed；collector
  不扫描 `target` 字符串猜测 callable。
- 完成项目：公共 writer 正例从原本仅可达函数 0/1 的图额外保留函数 2，发布绑定 token 与 stable
  `root.package_export` reason；负例返回 false 且不留下生成 C。
- 计划映射：11-A11.3 provider/package export binding，以及 12-S1 graph schema、12-S2 export root policy、
  12-S3 required code closure、12-S6 root-reason reporting 的 current-module 子切片。

## 代码与文档产出

- `backend_aot_reachability.h/.c` 增加 root-class reason `PACKAGE_EXPORT` 与稳定名称
  `root.package_export`。
- `backend_aot_reachability_function_graph.h/.c` 增加 preserve-roots 入口、package method export collector、唯一
  token resolution 与 fail-closed policy；既有 generic-only 和基础入口保持兼容包装。
- `backend_aot_c_emitter.c` 将 `manifestExportDeclarations` 接入 code-stripping graph。
- `test_aot_reachability.c` 从 28 个扩展为 30 个测试，覆盖正例、null/missing/wrong-table/unresolved/ambiguous
  binding、metadata-only kind 和 unknown kind。
- `test_aot_c_code_stripping.c` 从 22 个扩展为 24 个测试，覆盖实际生成清单、额外函数保留和半成品删除。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 package export 契约与开放边界。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-package-method-export-required-root.md`。

## 验证结果

- RED：unchanged production 的 GCC focused build 因缺少 `PACKAGE_EXPORT` reason 与 preserve-roots API 编译
  失败；code-stripping 测试夹具在同轮成功编译链接，失败集中于缺失 graph contract。
- WSL GCC 11.4、WSL Clang 14.0 与 Windows MSVC 19.44：focused CTest 均为 2/2，直接运行
  reachability 30/0、code stripping 24/0。
- WSL GCC 相邻 metadata 回归 `aot_c_zrp_metadata_pruning` 与
  `aot_c_zrp_metadata_export_token_remap` 为 2/2。
- 七份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 包含函数 0/1/2、`manifest.export[0].memberToken = 0x0300000b` 与
  `node[2] = reason=root.package_export predecessor=none`；unresolved 负例文件不存在。
- 独立审查无发现。
- MSVC 仅保留冻结 `%TEMP%` 构建目录触发的既有 MSB8029/MSB8064 warning。

## 未完成边界

- 当前仅关闭 current-module `MemberDef` binding；canonical cross-module `ModuleIdentity`、provider contract hash、
  `MemberRef` resolution、generation drift 和 package graph dedup 仍按 AOT 11/12 后续阶段开放。
- package `TYPE`/`FIELD` 的独立 metadata/type-layout graph 节点与完整 token/RID/pool remap closure 尚未由本
  function collector 证明。
- generic dictionary/constraint witness、native callback、module initializer、reflection metadata node 与
  DebugMap sidecar 尚未全部纳入统一 graph。
- 完整 mode policy、binary-size/behavior parity、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
