---
plan_id: aot-12-stripping
record_id: 2026-07-30-canonical-generic-dictionary-reachability
status: completed
completed_at: 2026-07-30 12:25:39 +08:00
source_plans:
  - docs/plans/aot/08-generic-sharing.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 08-G6A / 12-S1J / 12-S3G / 12-S6I Canonical Generic Dictionary Reachability

## 状态与产出记录

- 完成时间：2026-07-30 12:25:39 +08:00
- 状态：canonical generic dictionary reachability 子里程碑完成；AOT 08 G6 与 AOT 12 S1、S3、S6 仍为
  部分完成，AOT 07~12 总目标继续进行。
- 完成项目：reference-generic dictionary instance 以 compiler-owned typed-local `typeId` 去重；同 TypeId 的
  不同显示名共享一个字典，不同 TypeId 不再因相同显示名被错误合并。
- 完成项目：多个 retained function owner 复用同一 canonical dictionary ID；不可达 owner 独占的 dictionary
  随 function table 过滤删除。
- 完成项目：版本 1 `genericDictionaryManifest` 为每个保留字典发布 canonical TypeId、stable owner function、
  `edge.generic_instance` 与 predecessor，并报告 before/after/removed count。
- 完成项目：reference-generic candidate 缺少 TypeId、nonempty/null typed-local 或 frame-layout table，或同一
  TypeId 对应冲突 layout ID 时，公共 writer fail closed；结构预检先于 ExecIR，且不留下半成品输出。
- 计划映射：08-G1 canonical identity、08-G2 dictionary sharing、08-G6 stripping closure，以及 12-S1 graph
  schema、12-S3 generic instance trim、12-S6 reason/count reporting 的本模块子切片。

## 代码与文档产出

- `backend_aot_c_generic_sharing.h/.c` 增加 canonical dictionary validation/count/manifest API，以 TypeId 替代
  pretty type text 作为 dictionary instance dedup key，并修正重复 owner 的 dictionary ID 复用。
- `backend_aot_c_emitter.c` 在 function stripping 前后计算 dictionary count，发布 2→1 delta 与 retained-node
  manifest，并在 malformed metadata 时删除输出。
- `test_aot_c_generic_reference_sharing.c` 从 4 个扩展为 9 个测试，覆盖 identity、owner reuse、trim、manifest、
  missing TypeId、typed-local/frame-layout null table 与 conflicting schema。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 dictionary node、identity、reason 与失败
  契约。
- 验收入口：`tests/acceptance/2026-07-30-aot-08-12-canonical-generic-dictionary-reachability.md`。

## 验证结果

- RED 1：unchanged production 可编译，但新增 6-test suite 中 canonical stats/manifest 缺失且 missing TypeId
  writer 仍成功，结果 4 pass / 2 fail。
- RED 2：初版 canonical dedup 后，第二个 retained owner 仍得到 null dictionary，复用断言以 1 而非 2 失败。
- RED 3：独立审查指出 nonempty/null frame-layout table 在 generic 校验前进入 ExecIR；新增负例在 preflight
  缺失时异常退出，随后以前置 tree validation 修复。
- WSL GCC 11.4、WSL Clang 14.0 与 Windows MSVC 19.44：focused CTest 均为 2/2；直接运行 generic sharing
  9/0、code stripping 26/0。
- 相邻 `generic_call_typed` target 可构建，直接套件为 2 pass / 5 fail；两个 runtime dictionary 用例通过，
  其余失败均由 active syntax cutover 拒绝 legacy keywordless function 与 `$` construct，早于 writer。
- 冻结树的相邻 frame/source contract 套件分别为 0/1 与 21/24，缺失文本均位于未触碰的 frame setup、
  scalar-local 与 typed arithmetic helper，属于既有 source/test contract skew。
- 四份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 报告 dictionary 2→1，发布 `typeId=41 ownerFunction=0 reason=edge.generic_instance predecessor=0`，
  仅生成一个 dictionary，并让两个 retained owner 指向它；四个 malformed 负例文件均不存在。
- 独立审查最初发现 null frame-layout metadata 可早于 generic validation 进入 ExecIR；修复后同一审查者
  复核无发现。
- MSVC 仅保留冻结 `%TEMP%` 构建目录触发的既有 MSB8029/MSB8064 warning。

## 未完成边界

- 本切片只将 closed reference-generic dictionary instance identity 收敛到 TypeId；candidate discovery/debug text
  与 shared-body symbol grouping 仍沿用既有 type text，canonical definition token/sharing key 尚未闭合。
- dictionary schema 仍是现有 TYPE_LAYOUT/METHOD baseline；constraint witness、GC/drop slot、完整 slot hash/version
  与 incompatible-layout negative matrix 仍按 AOT 08 后续阶段开放。
- `.zro/.zrm` dictionary schema roundtrip、cross-module provider drift/dedup、MethodSpec/TypeSpec remap closure 与
  reflection invoke 尚未完成。
- 完整 size-bytes/behavior parity、四 backend、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
