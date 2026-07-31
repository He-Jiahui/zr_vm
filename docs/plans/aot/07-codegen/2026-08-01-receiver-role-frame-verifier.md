---
plan_id: aot-07-codegen
record_id: 2026-08-01-receiver-role-frame-verifier
status: completed
completed_at: 2026-08-01 04:21:18 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Receiver Role Frame Verifier

## 状态与产出记录

- 完成时间：2026-08-01 04:21:18 +08:00
- 状态：A7.2E receiver role frame verifier 子里程碑完成；A7.2、AOT 07、AOT 12 与 AOT 07~12 总目标继续进行。
- 完成项目：ExecIR 在 zero-frame 早退与 code stripping 前消费已有 patch-38
  `ZR_FUNCTION_TYPED_LOCAL_ROLE_RECEIVER`，非空 typed-local 长度必须有表，未知 role 位 fail closed。
- 完成项目：receiver role 至多一个，必须具有非零 `SymbolId`、`TypeId`、`PlaceId`，固定在 stack slot 0，
  且 function 至少声明一个 parameter；已物化 receiver frame row 必须标记 `isParameter`。
- 完成项目：complete-frame parameter classifier 将 canonical receiver role 作为结构化 parameter row，即使
  display name 已裁剪也不会反转 slot 身份；complete、sparse 与 zero-frame receiver 正例均保持合法。
- 完成项目：role 为零的旧 artifact 保持 metadata unavailable，不从 local name、slot、AST 或文本制造
  receiver；既有有名 parameter compatibility 继续有效。
- 完成项目：不可达 malformed owner 在 reachability filtering 前拒绝且不留下 generated C；沿用既有
  `frameLayoutManifest` version 1，不新增 graph/manifest schema。
- 计划映射：完成 AOT 07 A7.2E receiver-role frame verifier 与 AOT 12 pre-filter owner gate；parameter
  direction/type、return/destination、spill/address-taken、缺失 role producer 补全与完整 A7.2 保持开放。

## 代码与文档产出

- `backend_aot_exec_ir.c` 增加 receiver-role verifier，并让 complete-frame parameter classifier 识别 nameless
  canonical receiver；校验位于 frame layout 的所有 early return 之前。
- `test_aot_c_code_stripping.c` 在不可达 owner 上覆盖 complete/sparse/zero frame 正例、role-free compatibility，
  以及 unknown/duplicate role、null table、identity 缺失、wrong slot、zero parameter 与 non-parameter frame row。
- 模块文档同步 `csharp-value-type-semir-aot.md` 与 `aot-function-reachability-manifest.md`；验收入口为
  `tests/acceptance/2026-08-01-aot-07-receiver-role-frame-verifier.md`。

## 验证结果

- 初始 RED：当前 AOT 接受未知 receiver role 位，WSL GCC code stripping 为 34/35，其余 34 个回归通过。
- 独立审查发现 complete-frame nameless receiver 仍被 A7.2C classifier 错分的 P2；fixture 升级为完整两 row
  frame 后稳定复现 34/35。classifier 改为计数已验证 receiver role 后转绿，最终复审返回 `No findings.`。
- WSL GCC 11.4、Clang 14.0 与 Windows MSVC 19.44.35228.0 x64 Debug 均通过 AOT code stripping 35/0、
  generic reference sharing 9/0 与 upstream canonical receiver binary/runtime roundtrip 6/0。
- 主工作树、WSL 冻结树与 Windows 冻结树的实现/测试 SHA-256 完全一致，分别为
  `aefee791...9ebdfa9` 与 `26f38210...f2117e9`；三套 build tree 的 malformed generated C 均不存在。
- `git diff --check` 通过；MSVC 仅保留冻结目录位于 `%TEMP%` 的既有 MSB8029 warning。AOT source-contract
  静态文本套件仍有 4 个与本切片无关的既有文本漂移，因此不作为本子里程碑通过证据。
- 已向 AOT/Syntax traceability 会话发送顶层计划链接协调消息；未修改 `tests/CMakeLists.txt`、活跃
  syntax/LSP/debug/CI 路径或脏 `function.h`/parser producer 工作树内容。

## 未完成边界

- 本切片只验证已发布 receiver role，不证明所有 instance callable 都已发布 role，也不从名称、slot 或
  owner metadata 回推缺失 receiver。
- 不校验 parameter direction/type、readonly/ref/out，不生成 aggregate return destination、spill 或
  address-taken slot，不声明 C/LLVM frame ABI golden parity。
- 不生成 GC/ref provenance/debug safepoint map，不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
