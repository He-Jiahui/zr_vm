---
plan_id: aot-12-stripping
record_id: 2026-07-30-native-callback-materialization-edge
status: completed
completed_at: 2026-07-30 11:44:31 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
  - docs/plans/aot/11-metadata.md
  - docs/plans/aot/12-code-stripping.md
evidence_scope: sub-milestone
---

# 12-S1I / 12-S3F / 12-S6H Native Callback Materialization Edge

## 状态与产出记录

- 完成时间：2026-07-30 11:44:31 +08:00
- 状态：native callback materialization edge 子里程碑完成；AOT 12 的 S1、S3、S6 仍为部分完成，
  S2 callback descriptor root 保持开放，AOT 07~12 总目标继续进行。
- 完成项目：可达函数内的 `GET_SUB_FUNCTION`、callable `GET_CONSTANT` 与 `CREATE_CLOSURE` 在目标栈槽具备
  structured `NATIVE_BINDING`、`NATIVE_HANDLE` escape flag 时形成 `edge.native_callback`。
- 完成项目：缺少上述精确 slot contract 的相同 callable materialization 保持 `edge.direct_call`，不依据源码、
  符号名或生成文本猜测 native callback。
- 完成项目：`escapeBindingLength > 0` 但 escape-binding row pointer 为空时，graph construction 与公共 AOT C
  writer fail closed，writer 不保留半成品输出。
- 完成项目：版本 1 function manifest 发布稳定的 `edge.native_callback` reason 与 predecessor chain，未被该边
  触达的函数继续裁剪。
- 计划映射：07 环境隔离后的确定性 backend 输入、11 structured escape metadata，以及 12-S1 graph schema、
  12-S3 callback code closure、12-S6 reason reporting 的 reachable creation-site 子切片。

## 代码与文档产出

- `backend_aot_reachability.h/.c` 增加 edge-class reason `NATIVE_CALLBACK` 与稳定名称
  `edge.native_callback`。
- `backend_aot_reachability_function_graph.c` 按 destination slot 的 structured escape metadata 区分 native
  callback 和普通 direct edge，并拒绝 malformed table。
- `test_aot_reachability.c` 从 30 个扩展为 33 个测试，覆盖三类 opcode、普通 direct edge 和 malformed
  metadata。
- `test_aot_c_code_stripping.c` 从 24 个扩展为 26 个测试，覆盖实际生成清单、未触达函数裁剪和半成品删除。
- `docs/parser-and-semantics/aot-function-reachability-manifest.md` 同步 reason schema、精确绑定契约与开放边界。
- 验收入口：`tests/acceptance/2026-07-30-aot-12-native-callback-materialization-edge.md`。

## 验证结果

- RED：unchanged production 的 GCC focused build 因缺少 `NATIVE_CALLBACK` reason 编译失败；code-stripping
  测试夹具在同轮成功编译链接，失败集中于缺失 graph contract。
- WSL GCC 11.4、WSL Clang 14.0 与 Windows MSVC 19.44：focused CTest 均为 2/2，直接运行 reachability
  33/0、code stripping 26/0。
- 五份代码/测试文件在 Windows 与 WSL 冻结源码树的 SHA-256 均匹配主工作树。
- 生成 C 保留函数 0/1、裁剪函数 2，并包含
  `node[1] = reason=edge.native_callback predecessor=0`；malformed 负例文件不存在。
- 相邻 `escape_pipeline` target 可构建，但 CTest 为 0/1，直接套件为 2 pass / 10 fail；失败均发生在当前
  syntax cutover 对 legacy keywordless function 与 `%import` 的解析拒绝，早于本次 graph collector。
- 独立审查无发现。
- MSVC 仅保留冻结 `%TEMP%` 构建目录触发的既有 MSB8029/MSB8064 warning。

## 未完成边界

- 本切片只证明 reachable creation-site edge；externally registered、cross-module 或无可达创建点的 callback
  descriptor root 仍未实现，因此不计入 12-S2 完成项。
- callback ABI、lifetime、thread/exception boundary 与 canonical cross-module identity 仍按 AOT 07/11/12
  后续阶段开放。
- generic dictionary/constraint witness、module initializer、reflection metadata node、DebugMap sidecar 与完整
  token/RID/pool remap closure 尚未全部纳入统一 graph。
- 完整 mode policy、binary-size/behavior parity、source/binary/full-AOT loader parity 与 AOT 07~12 总验收仍开放。
