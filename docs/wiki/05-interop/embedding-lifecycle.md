---
related_code:
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/callback.h
  - zr_vm_library/include/zr_vm_library/common_state.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/project.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/project/project.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/state-lifecycle.md
  - docs/plans/aot/09-memory-management.md
tests:
  - tests/core/test_session_checkpoint.c
  - tests/module/test_module_system.c
  - tests/library/test_project_import_resolver.c
  - tests/cli/test_cli_import_basic_fixture.c
doc_type: workflow-detail
---

# 嵌入式宿主完整流程

## 推荐顺序

```text
1. 配置 allocator、log、panic、IO 和时间/取消 callback
2. ZrCore_GlobalState_New（返回时已拥有 `global->mainThreadState` 和基础 registry）
3. ZrCore_GlobalState_SetNativeModuleLoader / SetAotModuleLoader / SetCompileSource
4. ZrLibrary_NativeRegistry_Attach
5. 注册官方 provider（system、math、container、iteration、ffi、testing ...）
6. 使用 `global->mainThreadState`；AttachedDomain worker 才额外执行 `ZrCore_State_New + State_MutatorLaunch`
7. 解析/编译或加载 .zro/.zrm
8. ZrCore_State_DoRun 或 ZrCore_Execute
9. 清理 Task/FFI/file/network handles
10. State_Exit/Free -> NativeRegistry_Free -> GlobalState_Free
```

`ZrLibrary_CommonState_CommonGlobalState_New` 可把 1-5 的默认配置压缩成一步，适合 CLI
和 fixture；复杂宿主仍应显式安装 callback，以便把诊断转发到自己的日志/事件循环。

## 运行前检查

- global `isValid` 为 true，registry 已 attach，provider phase 与当前 host 匹配。
- source loader、native loader、AOT loader 至少安装一个；只有 binary 模式可以不安装 source
  compiler。
- 所有 `.zrp`/`.zrm`/`.zro` 的 module signature、contract hash 和 ABI version 已验证。
- 执行 budget/cancel token（若有）在 state 创建前或首次调用前配置。

## 运行中规则

可能触发 GC 的 API 包括 module import、对象构造、字符串创建、native callback、Task schedule
和 reflection construction。调用方必须使用 Value copy/root/pin 保护局部值；不得缓存
`SZrString`、`SZrObject`、`ZrLibCallContext` 的地址。跨线程只通过 AttachedDomain mutator
或 IsolatedDomain transfer contract；直接共享 stack/heap 是未定义行为。

## 关闭顺序

先停止新任务并等待/故障化 queue，再关闭 FFI callback/symbol/library、网络和文件句柄，
然后退出 secondary states，最后退出主 state。`NativeRegistry_Free` 可能恢复宿主 loader，
必须早于 `GlobalState_Free`；global free 后任何 descriptor、TypeValue、module cache 或
diagnostic 指针均失效。失败路径也要执行同一顺序，finalizer 只作幂等兜底。
