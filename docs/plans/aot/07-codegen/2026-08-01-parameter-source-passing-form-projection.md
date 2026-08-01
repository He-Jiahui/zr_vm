---
plan_id: aot-07-codegen
record_id: 2026-08-01-parameter-source-passing-form-projection
status: completed
completed_at: 2026-08-01 22:26:51 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# AOT 07 Parameter Source Passing-Form Projection

## 状态与产出记录

- 完成时间：2026-08-01 22:26:51 +08:00
- 状态：A7.2M parameter source passing-form projection 子里程碑完成；A7.2、AOT 07、AOT 12 与
  AOT 07~12 总目标继续进行。
- 完成项目：在 patch 38 已序列化的 `SZrFunctionTypedLocalBinding.roleFlags` u32 中定义 VALUE、IN、REF、
  REF_READONLY、SCOPED_REF、SCOPED_REF_READONLY 与 OUT 七个 one-hot 标志，不增加字段或变更二进制 schema。
- 完成项目：typed-metadata producer 只向当前 AST 已编译参数前缀写入 source passing form；匹配同时要求 receiver
  offset、local index、stack slot 与参数名一致，后续同名 shadow local 保持无角色，嵌套函数不继承外层 member receiver。
- 完成项目：真实源程序覆盖七种 passing form，并通过 `.zro` roundtrip 保留精确 role flags；实例方法额外证明
  slot 0 仅为 receiver，显式参数 slot 1/2 分别为 VALUE/IN。
- 完成项目：ExecIR 将 passing form 投影为 canonical enum sidecar；显式参数必须全 known 或全 legacy-unknown，
  receiver 不携带 passing form，并在 code stripping 前拒绝多 bit、未知 bit、prefix 外 passing bit 与 partial availability。
- 完成项目：aggregate `CALL_TYPED` 只在 exact arity、合法可选 receiver 与全部显式参数为 known VALUE 时发射
  当前 by-value 路径；unknown 及其余六种 non-VALUE form 保守 fallback。
- 计划映射：完成 AOT 07 A7.2M 的参数源 passing-form 生产、roundtrip、ExecIR 投影和 VALUE-only consumer gate；
  不新增 AOT 12 graph node、manifest、artifact 或 public ABI，S1/S3/S6 与 AOT 12 保持部分完成。

## 代码与文档产出

- `function.h` 定义 role flag carrier contract；`compiler_typed_metadata.c` 承担当前参数前缀的精确 producer 映射。
- `backend_aot_exec_ir.h` 与 `backend_aot_exec_ir_frame.c` 承担 canonical sidecar、完整性检查和 pre-strip fail-closed
  校验；`backend_aot_c_value_semir_calls.c` 仅消费投影后的 VALUE contract。
- focused headers 覆盖七种 source form、实例 receiver、shadow local、roundtrip、sidecar enum、consumer matrix 和
  malformed pre-strip owner；SemIR、generic typed-call 与 code-stripping suites 完成注册和邻接回归。
- canonical local-binding、value-type SemIR/AOT 模块文档、AOT 07/12 主计划回链与 acceptance evidence 同步更新；
  `tests/CMakeLists.txt` 未改动。

## 验证结果

- 初始 RED 在冻结当前 HEAD 上因缺少 passing-form flags、ExecIR sidecar 与 consumer helper 编译失败；复审加强的
  shadow RED 证明 name-only producer 会把后续同名 local 错标为 VALUE，随后以参数前缀精确匹配修复。
- 独立复审发现既有 code-stripping fixture 的 raw `2u` 已成为 canonical VALUE，改用真实未知 bit `1u << 31`；
  补齐实例 receiver 与 malformed pre-strip matrix 后，最终只读复审无 Critical/Important findings。
- WSL GCC 11.4.0 与 Clang 14.0.0 均通过：SemIR 13/0、generic typed-call 24/0、code stripping 37/0、
  MethodInfo 11/0、generic sharing 9/0、debug metadata 6/0、value-SemIR 8/0、typed-call contracts 4/0、
  source contracts 24/0。
- Windows MSVC 19.44.35228.0 x64 Debug 通过：SemIR 12/0、code stripping 37/0、MethodInfo 8/0、
  generic sharing 9/0、debug metadata 6/0、value-SemIR 8/0、typed-call contracts 4/0、source contracts 24/0；
  generic typed-call 共 24 项、0 failures、5 个预期 Unix-only ignore。
- Windows 冻结树中的三个既有跨行 source contract 因 CRLF 精确匹配产生 3 个假失败；只在冻结副本中将对应
  non-owned source 规范化为 LF 后恢复 24/0，未修改产品实现或测试断言。
- 十二个受控实现/测试文件在 main、冻结 WSL 与冻结 Windows 树中的 SHA-256 逐一一致；冻结基线为
  `HEAD=b968f2d3038bfdd1dade3349a3a243131bdbde8a`。

## 未完成边界

- `in/ref/out`、readonly 与 scoped 的物理引用 identity、address/storage ABI、callee writeback 与 caller observation
  尚未实现；这些 form 当前只进入 canonical sidecar 并阻止不安全的 by-value 快路径。
- 完整 aggregate callable contract、caller destination/return storage ABI、nested callable return 仍开放。
- spill/address-taken slot、GC/ref provenance、safepoint/debug map 与完整 callable metadata/thunk identity 仍开放。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
