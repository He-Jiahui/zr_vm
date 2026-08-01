---
plan_id: aot-07-codegen
record_id: 2026-08-01-direct-inline-return-layout-projection
status: completed
completed_at: 2026-08-01 15:26:32 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Direct Inline Return Layout Projection

## 状态与产出记录

- 完成时间：2026-08-01 15:26:32 +08:00
- 状态：A7.2L direct inline return TypeLayout projection 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：`SZrAotExecIrFunction` 增加 `directInlineReturnLayoutKnown` 与
  `directInlineReturnTypeLayoutId`，内部 accessor 只有在 canonical known 与非 NONE id 同时成立时返回布局。
- 完成项目：独立 projector 逐条验证 `RETURN_TYPED` 的 type-table index、semantic TypeRef、直接 inline frame
  source 与 canonical TypeLayout；合法 direct alias 共用 storage，`INDIRECT_ALIAS` 保持 unknown。
- 完成项目：只有全部 typed return 路径给出同一 direct `STRUCT` TypeLayout id 才形成 known sidecar；legacy
  dynamic、union、unknown callable、indirect alias 与 copy-compatible nonuniform ids 保守保持 unknown。
- 完成项目：不同布局 id 使用与 runtime return-copy 一致的 byte/alignment/copy/drop/GC/ownership/field shape
  比较；shape 不兼容以及 malformed/missing/non-inline source 在 ExecIR 构建时 fail closed。
- 完成项目：inline-struct `CALL_TYPED` destination、`RETURN_TYPED` source 与 return-source skip-drop 只消费
  projected id；raw callable id 即使被毒化也不能授权或覆盖 sidecar。
- 完成项目：complete function table 在 code stripping 前构建并验证 ExecIR；测试先证明合法 3→2、removed=1，
  再证明即将被裁剪 owner 的 malformed non-inline return source 使 writer 失败。
- 计划映射：完成 AOT 07 A7.2L 的 direct inline return layout 投影和三个保守 consumer；不新增 AOT 12
  graph node、manifest、artifact 或 public ABI，S1/S3/S6 与 AOT 12 保持部分完成。

## 代码与文档产出

- `backend_aot_exec_ir_return_layout.h/.c` 承担 return-layout projector 与 exact return-copy compatibility；
  `backend_aot_exec_ir.c` 保持 orchestration，当前 898 行，新模块 203 行。
- `backend_aot_c_value_semir_calls.h/.c` 与 `backend_aot_c_value_semir.c` 统一从 ExecIR sidecar 校验 typed
  call/return source、destination 和 skip-drop。
- focused projection case header 覆盖 known/unknown/invalid、long type name、alias、union、layout compatibility、
  pre-strip malformed owner 与 raw/sidecar authority；MethodInfo、SemIR、generic typed-call 和 value-SemIR
  suites 承担注册、roundtrip、邻接回归及 source contract。
- 模块文档、AOT 07/12 计划回链与 acceptance evidence 同步更新；`tests/CMakeLists.txt` 未改动。

## 验证结果

- 初始 RED 在冻结当前 HEAD 上因缺少 return-layout sidecar/accessor 失败；复审增加的 incompatible-layout
  RED 证明仅比较 id 不足，随后以 runtime-equivalent return-copy shape gate 修复。
- 独立复审还要求 skip-drop 行为矩阵以及 null SemIR、missing source 和真实 pre-strip removed-owner 证据；全部
  补齐后最终只读复审结论为 `No findings`。
- WSL GCC 11.4.0 与 Clang 14.0.0 均通过：MethodInfo 11/0、return 1/0、SemIR 11/0、load-const
  scalar 1/0、code stripping 37/0、generic typed-call 19/0、generic sharing 9/0、debug 6/0、value-SemIR
  8/0、typed-call 4/0、typed scalar 1/0、call smoke 5/0。
- Windows MSVC 19.44.35228.0 x64 Debug 通过：MethodInfo 8/0、return 1/0、SemIR 10/0、code stripping
  37/0、generic sharing 9/0、debug 6/0、value-SemIR 8/0、typed-call 4/0；generic typed-call 19 项、0
  failures、4 个 Unix-only ignore，load-const/typed-scalar/call-smoke 仅有预期 Unix-only ignore。
- Windows `git archive` 的 CRLF 使既有跨行 return source contract 假失败；仅用同一冻结 WSL snapshot 的
  LF 输入替换后恢复 1/0，未修改产品或断言。MSVC 仅保留既有 `%TEMP%` MSB8029 warning。
- 十二个受控实现/测试文件在 main、冻结 WSL 与冻结 Windows 树中的 SHA-256 逐一一致；冻结基线为
  `HEAD=92feb0ce2e306ef6c3b8738487ae0f0d849ae340`。

## 未完成边界

- 尚未形成完整 aggregate callable contract、caller destination/return storage ABI 或 nested callable return。
- `in/ref/out`/readonly direction、spill/address-taken slot、GC/ref provenance、safepoint/debug map 仍开放。
- union 与 indirect return storage 仍走 unknown/fallback；typed scalar thunk 的 raw metadata consumer 不在本切片。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
