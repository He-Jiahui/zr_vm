---
plan_id: aot-07-codegen
record_id: 2026-08-01-callable-return-type-projection
status: completed
completed_at: 2026-08-01 12:58:49 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Callable Return TypeRef Projection

## 状态与产出记录

- 完成时间：2026-08-01 12:58:49 +08:00
- 状态：A7.2K callable return TypeRef projection 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：`SZrAotExecIrFunction` 增加 `callableReturnTypeKnown` 与 `callableReturnType`；canonical true
  浅复制为 borrowed snapshot，false 保持 unknown，不从空值或名称推导返回类型。
- 完成项目：`backend_aot_exec_ir_callable_return_type()` 是内部唯一读取入口，只有 sidecar known 精确为 true
  才返回 TypeRef；noncanonical sidecar 值 fail closed 为 unknown。
- 完成项目：MethodInfo return signature 与 bool/u64/f64 scalar-local direct-return gate 改为只消费 ExecIR
  snapshot；raw `SZrFunction.hasCallableReturnType/callableReturnType` 即使被毒化也不能改变已建 ExecIR 的结果。
- 完成项目：unknown snapshot 保留既有 static-return inference；显式 `hasCallableReturnType=false` 且 raw
  TypeRef 被毒化为 bool 的 u64 函数仍生成 u64 MethodInfo signature 和 u64 direct return。
- 完成项目：源 `hasCallableReturnType` 必须是 canonical bool，并在 complete function table 构建期间、code
  stripping 前验证；测试先证明 3→2、removed=1，再证明被裁剪 child 的 noncanonical flag 使 writer 失败且
  不留下 artifact。
- 完成项目：不新增 runtime、dictionary、public function、artifact、manifest 或 reachability schema；TypeRef
  内部字符串指针借用 source function graph 生命周期，与现有 parameter-layout snapshot 一致。
- 计划映射：完成 AOT 07 A7.2K 的 callable-return TypeRef 内部投影与两个保守 consumer；AOT 12 复用既有
  complete owner prefilter，不新增图节点，S1/S3/S6 与 AOT 12 保持部分完成。

## 代码与文档产出

- `backend_aot_exec_ir.h/.c` 定义、验证并构建函数级 callable-return borrowed snapshot。
- `backend_aot_c_method_metadata.c` 与 `backend_aot_c_scalar_locals.c` 通过内部 accessor 消费 snapshot；
  MethodInfo/scalar inference fallback 保持不变。
- `test_semir_pipeline.c` 与 `test_aot_c_return_contracts.c` 锁定 schema、producer、consumer 与禁止 raw 读取；
  focused MethodInfo case header 覆盖 raw/sidecar 双向隔离、borrowed pointer、poison-after-build、unknown
  inference、实际 trimming 和不可达 malformed owner。
- 模块文档、AOT 07/12 计划回链与 acceptance evidence 同步更新。
- 大文件决策：`backend_aot_c_scalar_locals.c` 为 5783 行，但本切片只替换既有 return predicate 的 metadata
  来源，没有增加 helper 或新责任；为此迁移强行拆分会扩大风险。最小后续边界是抽取 scalar return proof/
  inference 模块。其余生产文件为 146/809/571 行，focused case header 为 275 行。

## 验证结果

- RED 冻结树基于 `HEAD=c3c4d45127c5468ddca4de90600850b392b49b2d`，只覆盖四个测试文件；GCC
  编译明确失败于缺少 `callableReturnTypeKnown`、`callableReturnType` 与 accessor。
- 首个 GREEN 的不可达负例未触发，是两个匿名 child 的行号相同，被 function-table equivalence 合并；为
  child 提供不同稳定源码位置后，测试准确覆盖 complete table 中后续被裁剪的 owner。
- 独立设计/实现复审两轮均无 P1/P2；最终 P3 要求的 build-after-poison MethodInfo、显式 unknown u64
  inference 与 3→2 trimming 证据全部补齐，复审结论为 `No findings`。
- WSL GCC 与 Clang 均通过：MethodInfo 8/0、return contracts 1/0、SemIR 10/0、load-const scalar 1/0、
  code stripping 37/0、generic typed-call 19/0、generic sharing 9/0、debug metadata 6/0、value-SemIR 8/0、
  typed-call contracts 4/0、typed scalar 1/0、call shared-library smoke 5/0。
- Windows MSVC x64 Debug 通过：MethodInfo 7/0（1 个 Unix private-consumer case 不注册）、return 1/0、
  SemIR 10/0、code stripping 37/0、generic sharing 9/0、debug metadata 6/0、value-SemIR 8/0、typed-call
  contracts 4/0；load-const scalar/typed scalar/call smoke 为预期 Unix-only ignore，generic typed-call 为 19 项、
  0 失败、4 ignore。仅保留 `%TEMP%` 下既有 MSB8029 warning。
- Windows `git archive` 首次把 runtime source-contract 输入导出为 CRLF，造成既有跨行 needle 假失败；用同一
  冻结 WSL snapshot 的 LF 文件替换后 return contract 1/0，未修改产品或断言。
- 八个受控实现/测试文件在 main、冻结 WSL 与冻结 Windows 树的 SHA-256 逐一一致；提交前
  `git diff --check` 通过，冻结基线仍是当前 `HEAD=c3c4d45127c5468ddca4de90600850b392b49b2d`。

## 未完成边界

- typed bool/i64/u64/f64 thunk 与 TypeLayout-token consumer 仍读取 raw callable metadata，未纳入本切片。
- producer 未携带 canonical static TypeLayout identity，不能据此关闭 inline-struct aggregate return
  destination、nested callable return 或 return storage ABI。
- `in/ref/out`/readonly direction、spill/address-taken slot、GC/ref provenance、safepoint/debug map 仍开放。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
