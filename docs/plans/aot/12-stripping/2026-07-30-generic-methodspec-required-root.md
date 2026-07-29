---
plan_id: aot-12-stripping
record_id: 2026-07-30-generic-methodspec-required-root
status: completed
completed_at: 2026-07-30 07:10:30 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1F / 12-S2J / 12-S3C / 12-S6E Generic MethodSpec Required Root

## 状态与产出记录

- 完成时间：2026-07-30 07:10:30 +08:00
- 状态：generic MethodSpec required-function root 子里程碑完成；AOT 12 的 S1、S2、S3、S6 仍为部分完成，
  AOT 07~12 总目标保持进行中。
- 完成项目：writer generic preserve root 携带 `hasMethodSpecBinding` 时，将 current-module
  `methodSpecMethodToken` 的唯一 `MemberDef` callable 作为 `root.generic_methodspec` 保留。
- 完成项目：null generic root 数组、non-`MemberDef` token、缺失或歧义 typed-symbol binding 均使 graph
  construction 与公共 AOT C writer fail closed；公共 writer 删除半成品输出。
- 完成项目：TypeSpec-only generic root 不在 function collector 中误保留 callable，继续由 type/generic
  graph 子阶段负责。
- 完成项目：公共 writer 正例保留原本不可达的 stable flat index 2，发布无 predecessor 的 MethodSpec
  root reason；负例返回 false 且不留下生成 C。
- 计划映射：12-S1 graph schema、12-S2 preserve root policy、12-S3 generic MethodSpec code gate、12-S6
  root-reason reporting 的 current-module 子切片。

## 代码与文档产出

- `backend_aot_reachability.h/.c` 增加 root-class reason `GENERIC_METHODSPEC` 与稳定名称
  `root.generic_methodspec`。
- `backend_aot_reachability_function_graph.h/.c` 新增兼容扩展入口，解析 MethodSpec underlying
  `MemberDef` token；原函数图 API 保留为无 generic roots 的包装。
- `backend_aot_c_emitter.c` 将 writer generic roots 接入 code-stripping graph。
- `test_aot_reachability.c` 从 24 个扩展为 26 个测试，覆盖保留、非法/缺失/歧义拒绝与 TypeSpec-only
  过滤。
- `test_aot_c_code_stripping.c` 从 18 个扩展为 20 个测试，覆盖实际生成清单、函数保留和半成品删除。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 MethodSpec root 契约与开放边界。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-generic-methodspec-required-root.md`。

## 验证结果

- RED：新增测试在 unchanged production 上因缺少 `GENERIC_METHODSPEC` reason 与 generic-roots graph API
  编译失败，证明测试未误用既有 manifest-function root。
- GREEN：WSL GCC 11.4、WSL Clang 14.0 与 Windows MSVC 19.44 focused CTest 均为 2/2；直接运行
  reachability 26/0、code stripping 20/0。
- 七份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 包含函数 0/1/2、`methodSpec.methodToken = 0x03000007` 与
  `node[2] = reason=root.generic_methodspec predecessor=none`；unresolved 负例文件不存在。
- 扩展 WSL GCC 回归中 metadata token、generic instantiation、code stripping 3/4 通过；
  `aot_c_generic_call_typed` 在进入 writer 前因并行语法切换已删除旧 keywordless function 与 `$`
  construct 而失败，不属于本切片调用路径，未在本提交修改。

## 未完成边界

- 当前只解析 current-module writer-visible `MemberDef`；cross-module `MemberRef` 与 package graph 绑定仍开放。
- generic dictionary、constraint witness、共享实例之间的边，以及 MethodSpec metadata/remap 的统一图节点仍开放。
- constructor、reflection createInstance/invoke、native callback、module initializer、DebugMap sidecar 与独立
  metadata node 尚未全部纳入统一 graph。
- 完整 mode policy、binary-size/behavior parity、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
