---
plan_id: aot-12-stripping
record_id: 2026-07-30-reflection-constructor-required-root
status: completed
completed_at: 2026-07-30 07:31:10 +08:00
source_plans:
  - docs/plans/aot/10-reflection.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1G / 12-S2K / 12-S3D / 12-S6F Reflection Constructor Required Root

## 状态与产出记录

- 完成时间：2026-07-30 07:31:10 +08:00
- 状态：reflection constructor required-function root 子里程碑完成；AOT 12 的 S1、S2、S3、S6 仍为
  部分完成，AOT 07~12 总目标保持进行中。
- 完成项目：serialized concrete class/struct prototype 的 public、非 abstract `ZR_META_CONSTRUCTOR`
  callable 作为 `root.reflection_constructor` 保留，保护 reflection `createInstance` 动态绑定。
- 完成项目：eligible constructor 的 `functionConstantIndex` 缺失、越界或无法映射到稳定 function table
  entry 时，graph construction 与公共 AOT C writer fail closed，并删除半成品输出。
- 完成项目：abstract/resource prototype、interface、non-public/abstract member、non-meta member 和其他 meta
  method 均不形成 reflection constructor root。
- 完成项目：公共 writer 正例将 3 个函数裁为 2 个，保留 constructor target、删除无关函数，并发布 stable
  flat index 1；负例返回 false 且不留下生成 C。
- 计划映射：10-R3 reflection construction，以及 12-S1 graph schema、12-S2 safe root policy、12-S3
  constructor code gate、12-S6 root-reason reporting 的保守子切片。

## 代码与文档产出

- `backend_aot_reachability.h/.c` 增加 root-class reason `REFLECTION_CONSTRUCTOR` 与稳定名称
  `root.reflection_constructor`。
- `backend_aot_reachability_function_graph.c` 在既有 required-member collector 中加入与 runtime
  createInstance binder 一致的 prototype/member 资格过滤和 fail-closed callable resolution。
- `test_aot_reachability.c` 从 26 个扩展为 28 个测试，覆盖 class/struct 保留、unresolved 拒绝和七类过滤。
- `test_aot_c_code_stripping.c` 从 20 个扩展为 22 个测试，覆盖实际生成清单、裁剪和半成品删除。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 createInstance constructor 契约与
  开放边界。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-reflection-constructor-required-root.md`。

## 验证结果

- RED：unchanged production 的 GCC focused build 因缺少 `REFLECTION_CONSTRUCTOR` reason 编译失败；
  code-stripping 测试夹具在同轮已编译，失败集中于缺失 graph contract。
- WSL GCC 11.4、WSL Clang 14.0 与 Windows MSVC 19.44：focused CTest 均为 2/2，直接运行
  reachability 28/0、code stripping 22/0。
- 五份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 包含函数 0/1、不包含函数 2，并发布
  `node[1] = reason=root.reflection_constructor predecessor=none`；unresolved 负例文件不存在。
- 独立审查无发现；MSVC 仅保留冻结 `%TEMP%` 构建目录触发的既有 MSB8029 warning。

## 未完成边界

- 当前安全策略保留 serialized metadata 中全部 eligible public constructor；按 reflection-reachable
  type/layout 收窄 constructor root 尚未关闭。
- resource/ref struct/interface/abstract/open generic 的完整 descriptor parity、constructor throw cleanup 和
  source/binary/AOT/trim 行为矩阵仍属 AOT 10 R3 后续验收。
- generic dictionary/constraint witness、native callback、ModuleIdentity/package export、module initializer、
  reflection metadata node 与 DebugMap sidecar 尚未全部纳入统一 graph。
- 完整 mode policy、binary-size/behavior parity、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
