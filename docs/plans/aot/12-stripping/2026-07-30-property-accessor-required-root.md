---
plan_id: aot-12-stripping
record_id: 2026-07-30-property-accessor-required-root
status: completed
completed_at: 2026-07-30 06:14:34 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1D / 12-S2H / 12-S3A / 12-S6C Property Accessor Required Root

## 状态与产出记录

- 完成时间：2026-07-30 06:14:34 +08:00
- 状态：property accessor required-function root 子里程碑完成；AOT 12 的 S1、S2、S3、S6 仍为部分完成，
  AOT 07~12 总目标保持进行中。
- 完成项目：serialized compiled prototype 中非 abstract、property identity 有效且 accessor role 为 getter、
  setter 或 initializer 的可执行成员，均将可解析函数发布为 `root.property_accessor`。
- 完成项目：required executable accessor 的 `functionConstantIndex` 缺失、越界或无法映射到稳定 function
  table entry 时，graph construction fail closed，不再静默跳过并误裁函数。
- 完成项目：abstract contract-only accessor、无 property identity 成员和越界 accessor role 不要求可执行函数，
  即使携带 unresolved function constant 也保持忽略。
- 完成项目：公共 AOT C writer 正例将 3 个函数裁为 2 个，保留 accessor target、删除无关函数，并在
  deterministic function manifest 中发布 stable flat index 1；负例返回 false 且删除半成品。
- 完成项目：getter、setter、initializer 三种 role 均有 unit 正例、unresolved 负例与 abstract 忽略覆盖；
  非 accessor 过滤守卫另有回归覆盖，公共 writer 有 getter 保留正例与 initializer fail-closed 负例。
- 计划映射：12-S1 graph schema、12-S2 root policy、12-S3 missing-required-root gate、12-S6 reporting 的
  property-accessor 子切片。

## 代码与文档产出

- `backend_aot_reachability_function_graph.c` 将可执行 accessor 的解析失败从 silent continue 改为 false，并
  在解析前排除 abstract contract-only accessor。
- `test_aot_reachability.c` 从 17 个扩展为 21 个测试，覆盖 3 种 accessor role、abstract role 和非 accessor
  过滤守卫。
- `test_aot_c_code_stripping.c` 从 14 个扩展为 16 个测试，覆盖实际生成清单和半成品删除。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 required-root 与 fail-closed 契约。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-property-accessor-required-root.md`。

## 验证结果

- RED：修正测试夹具身份后，MSVC reachability 19 项仅 unresolved-role 用例失败，code stripping 16 项仅
  public unresolved-accessor 用例失败，均为 `Expected FALSE Was TRUE`。
- RED：严格 fail-closed 后加入 abstract contract-only 用例，MSVC reachability 20 项仅该用例失败，表现为
  `Expected TRUE Was FALSE`；增加 abstract 排除后转绿。
- 审查闭环：补充无 property identity、role 0、role 4 三种 non-accessor 过滤回归，均使用 unresolved constant。
- WSL GCC 11.4：focused CTest 2/2；直接运行 reachability 21/0、code stripping 16/0。
- WSL Clang 14.0：focused CTest 2/2；直接运行 reachability 21/0、code stripping 16/0。
- Windows MSVC 19.44：focused CTest 2/2；直接运行 reachability 21/0、code stripping 16/0。
- 最终三份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 已检查 `functionsBefore=3`、`functionsAfter=2`、`functionsRemoved=1`，并包含
  `node[1] = reason=root.property_accessor predecessor=none`。

## 未完成边界

- field/property metadata token 的独立节点、RID/remap/query visibility 与 constructor 节点尚未关闭。
- generic dictionary、native import/callback、module initializer、reflection metadata、DebugMap sidecar 和
  resource Drop 节点仍未全部纳入统一 graph。
- 完整 mode policy、binary-size/behavior parity、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
