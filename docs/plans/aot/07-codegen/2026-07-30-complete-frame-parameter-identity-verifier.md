---
plan_id: aot-07-codegen
record_id: 2026-07-30-complete-frame-parameter-identity-verifier
status: completed
completed_at: 2026-07-30 15:34:59 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Complete-Frame Parameter Identity Verifier

## 状态与产出记录

- 完成时间：2026-07-30 15:34:59 +08:00
- 状态：A7.2C complete-frame parameter identity verifier 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：ExecIR 对 `frameSlotLayoutLength == stackSize` 的完整 frame table 逐行验证 canonical parameter
  identity，并要求 `isParameter` 行数与 `parameterCount` 精确一致。
- 完成项目：无 typed-local binding 时，canonical parameter 为 `[0, parameterCount)` stack-slot prefix；存在
  binding 时，复用 producer 规则，跳过 null name 并选择前 `parameterCount` 个 named binding 的 stack slot。
- 完成项目：zero-byte/zero-row frame 继续支持完全寄存器承载的 scalar parameter；sparse hybrid frame 可只
  物化 inline local，不会被强制补回 register-only parameter row。
- 完成项目：不可达完整表的 parameter undercount 与“数量相同但从 slot 0 参数错标到 slot 1 local”均在
  code stripping 前 fail closed，且不留下 generated C。
- 计划映射：完成 AOT 07 A7.2C parameter marker verifier 与 AOT 12 retained-frame graph-input gate；沿用既有
  `frameLayoutManifest`，不新增 schema，也不将 A7.2 或 AOT 12 标为完成。

## 代码与文档产出

- `backend_aot_exec_ir.c` 新增 complete-table canonical parameter-slot 判定，并在复制 ExecIR frame row 前完成
  marker identity 与 cardinality 校验。
- `test_aot_c_code_stripping.c` 新增 zero-frame、sparse hybrid、完整 borrowed alias 正例，以及不可达 undercount
  与 equal-count swapped-marker 负例。
- 模块文档同步 `csharp-value-type-semir-aot.md` 与 `aot-function-reachability-manifest.md`；验收入口为
  `tests/acceptance/2026-07-30-aot-07-complete-frame-parameter-identity-verifier.md`。

## 验证结果

- 初始 RED：冻结 WSL GCC 基线 32/32；加入 zero/sparse 正例与 undercount 负例后，旧实现为 32/33，仅新增
  负例因 writer 错误返回成功而失败。
- 首版 exact-count gate 为 33/33。独立审查发现完整两槽表可保持 marker 数量为一，却把 canonical slot 0
  参数与 slot 1 local 对调；新增该反例后首版 gate 为 32/33。
- 最终实现逐槽复用 producer parameter-slot 规则，并保留 exact-count gate；独立复审返回 `No findings`。
- WSL GCC 11.4、WSL Clang 14.0、Windows MSVC 19.44 均完成 focused build 与 code stripping 33/0；GCC
  相邻 generic reference sharing 为 9/0。
- 两份代码/测试文件在主工作树、WSL 冻结树与 Windows 冻结树 SHA-256 完全匹配；失败输出路径在 WSL 与
  Windows 均不存在。MSVC 仅保留冻结构建位于 `%TEMP%` 的既有 MSB8029 warning。
- 未修改 `tests/CMakeLists.txt`、parser/core producer、runtime frame 模块或活跃 syntax/LSP/debug/CI 路径。

## 未完成边界

- 本切片不校验 parameter TypeId、direction、readonly/ref/out 或 default value，不生成 receiver role、return、
  aggregate destination、spill 或 address-taken slots，也不完成 A7.2。
- 稀疏 frame 的 register/slot mapping 仍需未来 typed-register schema 显式表达；本切片只证明当前完整表。
- 不声明 C/LLVM frame ABI golden parity，不生成 GC/ref provenance/debug/cleanup maps；四 backend parity、policy
  模式、binary size/behavior 总验收与 AOT 07~12 总目标仍开放。
