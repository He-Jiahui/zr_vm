---
plan_id: aot-07-codegen
record_id: 2026-08-02-static-direct-call-frame-identity-guard
status: completed
completed_at: 2026-08-02 01:09:20 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
evidence_scope: sub-milestone
---

# AOT 07 Static Direct-Call Frame Identity Guard

## 状态与产出记录

- 完成时间：2026-08-02 01:09:20 +08:00
- 状态：A7.2N static direct-call frame identity guard 子里程碑完成；A7.2、AOT 07 与 AOT 07~12 总目标继续进行。
- 完成项目：same-module `ZrLibrary_AotRuntime_PrepareStaticDirectCall` 在 frame preparation 前解析 runtime record 的
  metadata function 与 entry thunk，并要求二者在 `calleeFunctionIndex` 处分别精确等于 generated frame 的
  `functionTable` 与 `functionThunks` 快照。
- 完成项目：null frame/table/entry、function/thunk count 越界、metadata generation drift 与 thunk generation
  drift 均 fail closed；输出 `directCall` 先清零且保持 unprepared，并写入显式 runtime error diagnostic。
- 完成项目：身份门禁位于 stack growth、GC、参数 staging 与 callee call-info 创建之前；真实 runtime-record
  fixture 保存并比较 frame、caller call-info、stack top、destination value 与 source payload，证明两种漂移均不
  改变 caller 调用状态，恢复精确身份后原有 stack relocation/readonly receiver alias 成功路径继续通过。
- 计划映射：完成 AOT 07 A7.2N 的 same-frame static-call generation identity 前置门禁；不新增 public ABI、
  artifact schema、manifest 字段或 AOT 12 trimming graph 节点。

## 代码与文档产出

- `aot_runtime_internal.h` 提供 null/bounds/exact-pointer identity predicate；root-built `aot_runtime.c` 在静态直调
  frame preparation 前调用该 predicate，并复用已验证的 callee thunk。
- focused compatibility suite 覆盖 exact match、metadata drift、thunk drift、null/incomplete 与独立 function/thunk
  count 越界；source contract 固定 output reset -> identity gate -> frame preparation 顺序。
- LLVM symbol-stripping/receiver-alias fixture 补齐 generated frame table snapshots，并以真实 AOT runtime record
  覆盖两种 drift 的 caller-state invariants 与恢复后的成功路径；`tests/CMakeLists.txt` 未改动。
- value-type SemIR/AOT 模块文档、AOT 07 主计划回链、acceptance evidence 与会话记录同步更新。

## 验证结果

- 冻结基线为 `HEAD=8c3ff8a80590af0c543b70ce9f9bf9e8412ea3ec`。初始 test-only RED 因缺少
  `aot_runtime_static_direct_call_identity_matches` 符号而链接失败；源码契约新增项同样在 helper 缺失时失败。
- 增强后的真实 API fixture 单独替换为冻结 HEAD 的 `aot_runtime.c` 后按预期为 LLVM 3 项、1 failure；恢复
  A7.2N 运行时且确认 SHA-256 与 main 一致后，同一目标转为 3/0 GREEN。
- 独立复审发现既有 LLVM static-call fixture 未提供 frame table snapshots，且 helper-only 测试不足以证明真实 API
  失败副作用；修正后 fixture 通过真实 record 注入两种 drift，并保持生产门禁严格，最终复审无 Critical 或
  Important findings。
- WSL GCC 11.4.0 与 Clang 14.0.0 均通过：focused compatibility 8/0、LLVM symbol stripping/receiver alias 3/0、
  source contracts 25/0、frame setup 1/0、call contracts 8/0、typed-call contracts 4/0、generic typed-call 24/0、
  call shared-library smoke 5/0。
- Windows MSVC 19.44.35228.0 x64 Debug 通过 focused 8/0、LLVM 3/0、source contracts 25/0、frame setup 1/0、
  call contracts 8/0、typed-call contracts 4/0；generic typed-call 共 24 项、0 failures、5 个预期 Unix-only
  ignore，call smoke 共 5 项、0 failures、5 个预期 Unix-only ignore。
- 跨行 source/frame-setup contracts 在 CRLF 冻结文件上产生文本匹配假失败；仅在 WSL/Windows 冻结副本中将
  non-owned AOT backend 源文件规范化为 LF 后恢复 25/0 与 1/0，主工作区和测试断言未修改。
- 六个受控实现/测试文件在 main、冻结 WSL 与冻结 Windows 树中的 SHA-256 逐一一致。
- 既有 `zr_vm_execbc_aot_pipeline_test` 在本切片前的冻结基线为 97 项、8 个历史失败；本子里程碑不以该陈旧
  fixture 集合作为 GREEN gate，改用 focused helper、真实 LLVM runtime record 和相邻生成/共享库 suites 验收。

## 未完成边界

- dynamic/meta/cross-module call generation binding 未纳入本门禁；跨模块 provider/closure generation identity
  继续开放。
- `in/ref/out` 的物理 reference storage、readonly/scoped address、callee writeback 与 caller observation 未实现。
- aggregate caller destination/return storage、nested callable return、spill/address-taken slot、GC/ref provenance、
  safepoint/debug map 与 A7.3 environment generation key 仍开放。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
