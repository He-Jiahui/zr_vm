---
plan_id: aot-12-stripping
record_id: 2026-07-30-function-reachability-manifest
status: completed
completed_at: 2026-07-30 04:43:24 +08:00
source_plans:
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1B / 12-S2F / 12-S6A Function Reachability Manifest

## 状态与产出记录

- 完成时间：2026-07-30 04:43:24 +08:00
- 状态：function reachability reason schema 与 deterministic manifest 子里程碑完成；AOT 12 的 S1、S2、S6
  均仍为部分完成，完整 code/metadata stripping 计划未关闭。
- 完成项目：compute 输入边界严格区分 root reason 与 dependency-edge reason，拒绝 `NONE`、未知枚举及理由
  类别错位，不再让 malformed graph 进入 mark 数组。
- 完成项目：保留既有 BFS first-reason 语义，为每个 retained function 记录稳定 reason 与 predecessor，并在
  emitter 过滤 function table 前发布 version 1 manifest。
- 完成项目：manifest 按 flat function index 升序输出；pending/dirty-unmarked、root/edge 链错位、越界
  predecessor 与 cycle 全部 fail closed。
- 完成项目：补齐 root/edge/unknown reason 负例、manifest 稳定性与 direct/export/manifest emitter 集成验证。
- 计划映射：12-S1 graph schema、12-S2 root policy、12-S6 reporting 的 function-node 子切片。

## 代码与文档产出

- `backend_aot_reachability.c` 在图输入验证阶段校验 root/edge reason class。
- `test_aot_reachability.c` 增加 malformed reason schema 矩阵；既有 `test_aot_c_code_stripping.c` 验证
  direct-call、export 与 manifest root 的发布结果。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 记录图契约、reason chain、稳定输出和
  fail-closed 边界。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-s1b-s2f-s6a-function-reachability-manifest.md`。

## 验证结果

- 最终有效源码为 `5af9a0c` 加本子里程碑两个 code/test overlay；变化文件 blob 与 overlay SHA-256 已核对。
- WSL GCC 11.4：focused CTest 2/2；直接运行 reachability 17/0、code stripping 10/0。
- WSL Clang 14.0：focused CTest 2/2；直接运行 reachability 17/0、code stripping 10/0。
- Windows MSVC 19.44 clean build：focused CTest 2/2；直接运行 reachability 17/0、code stripping 10/0。
- MSVC 旧增量目录在共享 struct ABI 收缩后出现陈旧对象失配；全新目录通过，验收不采用旧目录结果。

## 未完成边界

- type/layout、generic dictionary、native callback、module initializer、reflection metadata、DebugMap sidecar 与
  resource Drop 尚未全部纳入统一 graph。
- closed-world dynamic descriptor、missing preserve、generic reflection、callback、Drop negative matrix 仍需后续
  子里程碑扩展。
- before/after binary bytes、完整 root/untrimmed cause 报告、artifact loader parity 与 CLI dump/diff 尚未完成。
