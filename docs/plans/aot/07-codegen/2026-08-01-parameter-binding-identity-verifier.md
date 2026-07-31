---
plan_id: aot-07-codegen
record_id: 2026-08-01-parameter-binding-identity-verifier
status: completed
completed_at: 2026-08-01 05:11:35 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Parameter Binding Identity Verifier

## 状态与产出记录

- 完成时间：2026-08-01 05:11:35 +08:00
- 状态：A7.2F parameter binding identity verifier 子里程碑完成；A7.2、AOT 07、AOT 12 与 AOT 07~12
  总目标继续进行。
- 完成项目：ExecIR 在 zero-frame 早退与 code stripping 前按 producer 顺序选取前 `parameterCount` 个
  eligible typed-local row；有名 row 或 canonical receiver 参与，nameless non-receiver row 不消耗 prefix。
- 完成项目：parameter slot 必须在 stack 范围内且唯一；所选 row 的 identity availability 必须整组一致，
  即全部为 `SymbolId`/`TypeId`/`PlaceId` 全零 legacy tuple，或全部为三项非零 canonical tuple。
- 完成项目：canonical parameter set 的 SymbolId 与 PlaceId 分别唯一；相同 TypeId 保持合法，因为多个
  parameter 可以共享同一 canonical type。prefix 后普通 local 不由本切片校验，receiver 不得越过 prefix。
- 完成项目：无 typed-local table 的旧 artifact 与全零 identity tuple 继续走兼容路径；不从名称、slot、
  AST、TypeRef、TypeLayout 或 CallableContract 推导缺失 identity。
- 完成项目：不可达 malformed owner 在 reachability filtering 前拒绝且不留下 generated C；沿用既有
  `frameLayoutManifest` version 1，不新增 graph/manifest schema。
- 计划映射：完成 AOT 07 A7.2F canonical parameter binding identity gate 与 AOT 12 pre-filter owner gate；
  direction/type equality、readonly/ref/out/default、return/destination、spill/address-taken 与完整 A7.2 保持开放。

## 代码与文档产出

- 新增 `backend_aot_exec_ir_frame.c/.h`，集中 frame 校验与 sidecar 构建；`backend_aot_exec_ir.c` 仅保留
  ExecIR 编排，文件从 1277 行降至 893 行，frame 模块为 397 行，CMake source glob 自动发现新实现。
- `backend_aot_exec_ir_frame.c` 增加 parameter prefix 与 canonical identity verifier，并在 receiver/frame
  layout 校验及所有 frame early return 之前执行。
- `test_aot_c_code_stripping.c` 在不可达 owner 上覆盖 canonical/legacy 正例、equal TypeId、nameless skip、
  post-prefix local 边界，以及 partial/mixed identity、slot/SymbolId/PlaceId 重复、越界、缺 row 与 late receiver。
- `test_semir_pipeline.c` 与 `test_aot_c_value_semir_contracts.c` 的结构契约跟随 frame 模块拆分，仍分别验证
  frame row 复制与 alias addressing；模块文档、07/12 计划链接和本验收入口同步更新。

## 验证结果

- 初始 RED：冻结 WSL GCC 在未改动 A7.2E backend 上报告 code stripping 35/36；partial parameter identity
  被接受，其余 35 个回归通过。
- 独立审查指出 nameless non-receiver skip 与全零 legacy slot failure 两项 P3 覆盖缺口；补齐正负用例后
  最终复审返回 `No findings.`。
- WSL GCC 11.4、Clang 14.0 与 Windows MSVC 19.44.35228.0 x64 Debug 均通过 AOT code stripping 36/0、
  generic reference sharing 9/0、receiver metadata roundtrip 6/0、SemIR pipeline 10/0 与 value-SemIR 8/0。
- 主工作树、WSL 冻结树与 Windows 冻结树的 6 个实现/测试文件 SHA-256 逐项完全一致；三套 build tree
  的 `malformed_unreachable_parameter_binding_identity.c` 均不存在，`git diff --check` 通过。
- MSVC 仅保留冻结目录位于 `%TEMP%` 的既有 MSB8029 warning；未修改 `tests/CMakeLists.txt`、脏
  `function.h`、parser producer 或活跃 Syntax/LSP/Debug/CI 路径。

## 未完成边界

- 本切片验证 identity availability、prefix、slot 与 SymbolId/PlaceId 唯一性，不证明 TypeId 与 TypeRef、
  TypeLayout 或 CallableContract 相等，也不验证 passing direction、readonly/ref/out/default。
- 不生成 aggregate return destination、spill、address-taken slot、GC/ref provenance 或 debug safepoint map，
  不声明 C/LLVM frame ABI golden parity。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
