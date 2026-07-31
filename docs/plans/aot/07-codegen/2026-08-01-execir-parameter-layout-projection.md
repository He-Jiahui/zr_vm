---
plan_id: aot-07-codegen
record_id: 2026-08-01-execir-parameter-layout-projection
status: completed
completed_at: 2026-08-01 06:46:40 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 ExecIR Parameter Layout Projection

## 状态与产出记录

- 完成时间：2026-08-01 06:46:40 +08:00
- 状态：A7.2G ExecIR parameter layout projection 子里程碑完成；A7.2、AOT 07、AOT 12 与 AOT 07~12
  总目标继续进行。
- 完成项目：内部 `SZrAotExecIrFrameLayout` 新增按 runtime parameter index 排列的 parameter sidecar；每行
  保存 stack slot、SymbolId、TypeId、PlaceId、receiver role 与 shallow TypeRef，并由 frame 模块统一构建/释放。
- 完成项目：存在 typed-local table 时，投影严格复用 A7.2F 已验证的 producer-order eligible prefix；MethodInfo
  signature 从 ExecIR sidecar 取类型，避免 instance receiver 被第一条显式 AST parameter metadata 错位覆盖。
- 完成项目：无 typed-local table 时，仅当 `parameterMetadataCount == parameterCount` 才按索引复制 legacy
  TypeRef；不完整 metadata 无法证明 receiver/synthetic parameter offset，因此全部 runtime parameter 保持 unknown。
- 完成项目：全树预检拒绝 nonzero/null parameter metadata table 与 metadata count 大于 runtime parameter count；
  malformed 不可达 owner 在 code stripping 前 fail closed，两个拒绝分支均立即验证不留下部分输出。
- 完成项目：保留 metadata count 0、完整 legacy metadata、zero-parameter 与无 typed-local 兼容路径；不修改
  public function/artifact schema、frame manifest version、parser producer 或 reachability graph node schema。
- 计划映射：完成 AOT 07 A7.2G slot-aligned parameter signature consumption 与 AOT 12 pre-filter metadata shape
  gate；direction/default、TypeId/TypeRef equality、return/destination、spill/address-taken 与完整 A7.2 保持开放。

## 代码与文档产出

- `backend_aot_exec_ir.h` 定义内部 `SZrAotExecIrParameterLayout`，`backend_aot_exec_ir_frame.c/.h` 负责
  parameter sidecar 的验证、分配、投影与释放，`backend_aot_exec_ir.c` 统一调用 frame release boundary。
- `backend_aot_c_method_metadata.c` 的 signature parameter lookup 优先消费 ExecIR parameter layout；sidecar
  不存在时保留原 metadata fallback，但正常 `parameterCount > 0` ExecIR 路径均持有 slot-aligned rows。
- `test_aot_c_method_info_signature.c` 覆盖 receiver + explicit parameter 对齐、typed-local 对冲突 metadata 的
  权威性，以及 partial legacy metadata 全部 unknown；`test_aot_c_code_stripping.c` 覆盖 metadata shape 正负例。
- `test_semir_pipeline.c` 继续锁定内部 parameter sidecar 的 schema、复制、构建与释放边界；模块文档、07/12
  计划链接和 acceptance evidence 同步更新。
- 大文件决策：生产编排/frame 模块分别为 886/501 行，MethodInfo 测试为 522 行；既有 4003 行
  code-stripping 文件仅增加 61 行，并复用同一不可达函数 fixture、writer options 与无输出断言，单独拆分会
  复制测试所有权且要求触碰共享 `tests/CMakeLists.txt`，因此本切片不做无关测试框架重组。

## 验证结果

- 初始 RED：未修改 backend 时 MethodInfo 1/2，receiver 类型缺失；code stripping 36/37，nonzero/null metadata
  table 被接受。实现后两组转绿；独立审查再发现 partial legacy metadata 错位与 authority 覆盖缺口。
- 审查修正 RED：metadata 改为与 typed-local 冲突的 U64 后 authority 用例仍应输出 I64；新增
  `parameterCount=2`、metadata count 1 的 legacy 用例在修正前 2/3，收紧 exact-count fallback 后 3/0。
- WSL GCC 11.4、Clang 14.0 与 Windows MSVC 19.44.35228.0 x64 Debug 均通过 MethodInfo 3/0、code stripping
  37/0、SemIR 10/0、generic sharing 9/0、debug metadata 6/0、value-SemIR 8/0 与 typed-call 4/0。
- Windows 首次增量验证因冻结副本保留旧时间戳而混用 ExecIR 结构对象并访问冲突；独占冻结树执行 CMake
  clean 后全量重建，完整矩阵通过。MSVC 仅保留 `%TEMP%` 目录既有 MSB8029 与第三方编译 warning。
- GCC call shared-library smoke 为 4/5；失败发生在 binary-input dynamic-call writer。A7.2G metadata gate 未命中，
  且替换为 A7.2F 提交 `c09091b` 的生产实现后同例仍失败，故记录为冻结基线漂移而非本切片回归。
- GCC frame-setup 静态文本契约 0/1，缺失的是既有 `includeStackFrameSetup` 文本片段；未在本切片扩大修复范围。
- 主工作树、WSL 冻结树与 Windows 冻结树的 8 个实现/测试文件 SHA-256 逐项一致；`git diff --check`
  通过，独立最终复审返回 `No findings.`。

## 未完成边界

- parameter sidecar 只投影已经验证的 runtime slot 顺序，不证明 AST metadata 完整性、名称顺序、TypeId 与
  TypeRef/TypeLayout/CallableContract 相等，也不推导 `in/ref/out`、readonly、default 或 decorator。
- shallow TypeRef 仅在 writer lifetime 内消费，当前 MethodInfo 使用数值类型字段；本切片不建立独立字符串所有权
  或 public serialized CallableContract。
- 不生成 aggregate return destination、spill、address-taken slot、GC/ref provenance、safepoint/debug map，
  不声明 C/LLVM frame ABI golden parity。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
