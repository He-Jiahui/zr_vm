---
plan_id: aot-12-stripping
record_id: 2026-07-30-resource-drop-required-root
status: completed
completed_at: 2026-07-30 06:45:11 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1E / 12-S2I / 12-S3B / 12-S6D Resource Drop Required Root

## 状态与产出记录

- 完成时间：2026-07-30 06:45:11 +08:00
- 状态：Resource Drop required-function root 子里程碑完成；AOT 12 的 S1、S2、S3、S6 仍为部分完成，
  AOT 07~12 总目标保持进行中。
- 完成项目：serialized prototype 带 `RESOURCE` modifier，且成员为非 abstract `ZR_META_DESTRUCTOR` meta
  method 时，其可解析函数作为 `root.resource_drop` 保留。
- 完成项目：required resource destructor 的 `functionConstantIndex` 缺失、越界或无法映射到稳定 function
  table entry 时，graph construction 和公共 AOT C writer fail closed，不再让动态 Drop 调度指向被裁函数。
- 完成项目：non-resource destructor、非 meta member、constructor meta method 与 abstract destructor contract
  均不形成 Drop root，即使携带 unresolved constant 也保持忽略。
- 完成项目：公共 writer 正例将 3 个函数裁为 2 个，保留 destructor target、删除无关函数，并发布 stable
  flat index 1；负例返回 false 且删除半成品。
- 计划映射：12-S1 graph schema、12-S2 root policy、12-S3 required Drop gate、12-S6 reporting 的 Resource
  Drop 子切片。

## 代码与文档产出

- `backend_aot_reachability.h/.c` 增加 root-class reason `RESOURCE_DROP` 与稳定名称 `root.resource_drop`。
- `backend_aot_reachability_function_graph.c` 将 prototype 扫描泛化为 required member root collector，并对
  executable resource destructor 执行 fail-closed callable resolution。
- `test_aot_reachability.c` 从 21 个扩展为 24 个测试，覆盖 Drop 保留、unresolved 拒绝和过滤矩阵。
- `test_aot_c_code_stripping.c` 从 16 个扩展为 18 个测试，覆盖实际生成清单、裁剪和半成品删除。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 Drop runtime-dispatch 契约与开放边界。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-resource-drop-required-root.md`。

## 验证结果

- RED：生产代码未变时，MSVC reachability 24 项中 Drop 保留与 unresolved Drop 两项失败，分别为
  `Expected 2 Was 1` 与 `Expected FALSE Was TRUE`；code stripping 18 项中对应两项失败。
- GREEN：新增 reason 后，既有 unknown-reason 哨兵从旧枚举尾部后移；更新哨兵后所有行为测试转绿。
- WSL GCC 11.4：focused CTest 2/2；直接运行 reachability 24/0、code stripping 18/0。
- WSL Clang 14.0：focused CTest 2/2；直接运行 reachability 24/0、code stripping 18/0。
- Windows MSVC 19.44：focused CTest 2/2；直接运行 reachability 24/0、code stripping 18/0。
- 最终五份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 已检查 `functionsBefore=3`、`functionsAfter=2`、`functionsRemoved=1`，包含
  `node[1] = reason=root.resource_drop predecessor=none`，且 unresolved 负例文件不存在。

## 未完成边界

- 当前安全策略保留 serialized metadata 中全部 executable resource destructor；按 reachable type/layout
  收窄 Drop root 尚未关闭。
- constructor、generic dictionary/MethodSpec、reflection createInstance/invoke、native callback、module
  initializer、DebugMap sidecar 与独立 metadata node 仍未全部纳入统一 graph。
- 完整 mode policy、binary-size/behavior parity、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
