---
plan_id: aot-07-codegen
record_id: 2026-07-30-frame-type-layout-closure-verifier
status: completed
completed_at: 2026-07-30 15:11:35 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Frame TypeLayout Closure Verifier

## 状态与产出记录

- 完成时间：2026-07-30 15:11:35 +08:00
- 状态：A7.2B canonical frame TypeLayout closure verifier 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：ExecIR 在 code stripping 前遍历完整 source function tree；每个 `INLINE_STRUCT` frame row 必须
  解析到 canonical prototype-frame TypeLayout，包括之后会被裁剪的 owner。
- 完成项目：READY layout 必须通过 schema/hash 校验，`cTypeId` 必须等于 frame `typeLayoutId`，kind 必须为
  `STRUCT` 或 `UNION`，payload size/alignment 必须与 frame row 精确一致。
- 完成项目：payload TypeLayout 闭包与既有 physical storage verifier 分层执行；direct overlap、indirect alias
  与 borrowed alias 的合法 binding size/alignment 规则保持不变。
- 完成项目：不可达 owner 的 unresolved ID、非零 hash 漂移、`cTypeId` 漂移、合法 VALUE kind、size 漂移与
  alignment 漂移均 fail closed，且失败后不留下 generated C。
- 计划映射：完成 AOT 07 A7.2B verifier 前置切片与 AOT 12 裁剪前 graph-input closure gate；沿用既有
  frame/type-layout manifest，不新增重复 schema，也不将 A7.2 或 AOT 12 标为完成。

## 代码与文档产出

- `backend_aot_exec_ir.c` 将 `SZrState` 接入 frame verifier，并复用
  `ZrCore_Function_ResolvePrototypeFrameTypeLayout()` 与 `ZrCore_TypeLayout_Validate()` 完成 canonical closure。
- `test_aot_c_code_stripping.c` 将 trim fixture 改为 canonical union cache，保留 direct/indirect/borrowed 正例，
  并新增六类 malformed unreachable TypeLayout 负例及失败文件清理断言。
- 模块文档同步 `csharp-value-type-semir-aot.md` 与 `aot-function-reachability-manifest.md`；验收入口为
  `tests/acceptance/2026-07-30-aot-07-frame-type-layout-closure-verifier.md`。

## 验证结果

- 初始 RED：冻结 WSL GCC 基线 31/31；新增 closure matrix 后旧实现为 31/32，首次 unresolved case 被错误
  接受。首版 aggregate/schema/shape gate 变为 32/32。
- 独立审查发现 `cTypeId` 不在 layout hash 中，READY cache 的身份漂移仍会通过；新增单字段身份漂移后旧
  gate 再次为 31/32。hash 负例同步改为非零单 bit 漂移，VALUE kind 负例匹配 size/align/identity 并重算
  hash，避免其他 gate 掩盖目标故障。
- 修复后 WSL GCC 11.4、WSL Clang 14.0、Windows MSVC 19.44 均完成 focused build 与 code stripping
  32/0；GCC 相邻 generic reference sharing 为 9/0。
- 两份代码/测试文件在主工作树、WSL 冻结树与 Windows 冻结树 SHA-256 完全匹配；malformed 输出路径在
  WSL 与 Windows 均不存在。
- 独立复审确认身份、hash 与 kind 隔离覆盖闭合，最终返回 `No findings`。MSVC 仅保留冻结构建位于
  `%TEMP%` 的既有 MSB8029 warning。
- 未修改 `tests/CMakeLists.txt`、parser/core producer、runtime frame 模块或活跃 syntax/LSP/debug/CI 路径。

## 未完成边界

- 本切片不从 `CallableContract` 派生 receiver、`in/ref/out`、return、aggregate destination、spill 或
  address-taken slots，不完成 A7.2。
- 不生成 register allocation、GC/ref provenance、cleanup 或 safepoint debug map，也不完成 A7.4。
- 不声明 C/LLVM/Windows/MSVC frame ABI golden parity，不改变 constructor bitmap 或 generic fallback cache
  producer；四 backend parity、policy 模式、binary size/behavior 总验收与 AOT 07~12 总目标仍开放。
