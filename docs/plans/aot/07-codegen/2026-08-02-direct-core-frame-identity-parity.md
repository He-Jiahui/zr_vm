---
plan_id: aot-07-codegen
record_id: 2026-08-02-direct-core-frame-identity-parity
status: completed
completed_at: 2026-08-02 02:55:46 +08:00
source_plans:
  - docs/plans/aot/07-codegen-register-model-and-environment-isolation.md
evidence_scope: sub-milestone
---

# AOT 07 Direct-Core Frame Identity Parity

## 状态与产出记录

- 完成时间：2026-08-02 02:55:46 +08:00
- 状态：A7.2O direct-core frame identity parity 子里程碑完成；A7.2、AOT 07 与 AOT 07~12 总目标继续进行。
- 完成项目：`ZrLibrary_AotRuntime_CallStaticDirect` 与 `ZrLibrary_AotRuntime_CallInlineStruct` 在 stack growth、GC、
  call-window reset、参数 staging 和 callee call-info 创建前解析 callable metadata，并要求其与 supplied thunk
  在 `calleeFunctionIndex` 处精确匹配 generated frame 的 function/thunk snapshot。
- 完成项目：null callable/metadata、metadata generation drift 与 thunk generation drift 均 fail closed，写入
  `generated AOT direct-core call identity drift`，且不调用 supplied thunk 或修改 caller 调用状态。
- 完成项目：所有 `includeFrameDescriptor` 生成帧均初始化 function/thunk table 与 count；module、moduleExecuted
  与 codeRegistration 继续只在 `includeExportContext` 时发布，修复 nested non-export generated call frame 缺少
  identity snapshot 的问题。
- 计划映射：完成 AOT 07 A7.2O 的 C direct-core frame identity parity；不新增 public ABI、artifact schema、
  manifest 字段或 AOT 12 trimming graph 节点。

## 代码与文档产出

- root-built `aot_runtime_return.c` 在两个 direct-core entry 中复用 A7.2N 的 exact identity predicate，并在可能
  relocation 后保留原有 callable/metadata reacquisition。
- `backend_aot_c_frame_setup.c` 将四个 generation snapshot 字段移出 export-only 分支；帧设置契约同时固定 table
  和 count 位于 `includeExportContext` 之前，export context 字段仍位于分支内部。
- focused runtime fixture 以真实 state/function/native closure/call-info/frame 覆盖两个 API 的 metadata/thunk drift，
  并比较 thunk counter、caller call-info/base/top、stack top、frame base、snapshot entry、callable 与 destination。
- source contract 固定 metadata resolution -> identity gate -> stack growth 顺序；模块文档、AOT 07 主计划回链、
  acceptance evidence 与会话记录同步更新，`tests/CMakeLists.txt` 未修改。

## 验证结果

- 冻结基线为 `HEAD=014be1e599d7b07f953ea6c2f05c6272319163de`。初始 test-only runtime RED 为
  12 项、4 failures，四个新增用例均证明旧实现会调用错误 thunk；初始 direct-core source contract 也因缺少
  pre-growth identity gate 失败。
- 首轮 runtime GREEN 暴露 generic typed-call 24 项中的 4 个回归；诊断定位到 nested generated frame 的
  function/thunk table 与 count 全为零。加强后的 frame-setup RED 为 1/1，随后将四个 snapshot 字段移出
  export-only 分支，同一 frame-setup 目标转为 1/0，generic typed-call 恢复 24/0。
- 独立复审无 Critical 或 Important findings。复审指出 count placement 尚未被直接锁定后，契约补充
  `functionCount` 与 `functionThunkCount` 的分支前顺序断言；GCC、Clang、MSVC 均重跑 1/0，最终复审确认空白关闭。
- WSL GCC 11.4.0 与 Clang 14.0.0 均通过：focused compatibility 12/0、source contracts 26/0、frame setup 1/0、
  generic typed-call 24/0、call shared-library smoke 5/0、LLVM symbol stripping/receiver alias 3/0、call contracts
  8/0、typed-call contracts 4/0。
- Windows MSVC 19.44.35228.0 x64 Debug 通过 focused 12/0、source contracts 26/0、frame setup 1/0、LLVM 3/0、
  call contracts 8/0 与 typed-call contracts 4/0；generic typed-call 共 24 项、0 failures、5 个预期 Unix-only
  ignore，call smoke 共 5 项、0 failures、5 个预期 Unix-only ignore。
- 既有跨行 source contracts 在 CRLF 冻结文件上产生 4 个文本匹配假失败；仅在 WSL/Windows 冻结副本中将
  AOT backend 源文件规范化为 LF 后恢复 26/0，主工作区与断言未修改。
- 七个受控实现/测试文件在 main、冻结 WSL 与冻结 Windows 树中按 LF 规范化后的 SHA-256 逐一一致。

## 未完成边界

- dynamic/meta/cross-module call generation binding 与 provider/closure generation identity 继续开放。
- `in/ref/out` 的物理 reference storage、readonly/scoped address、callee writeback 与 caller observation 未实现。
- aggregate caller destination/return storage、nested callable return、spill/address-taken slot、GC/ref provenance、
  safepoint/debug map 与 A7.3 environment generation key 仍开放。
- 不完成 A7.2、AOT 07、AOT 12 或 AOT 07~12 总目标。
