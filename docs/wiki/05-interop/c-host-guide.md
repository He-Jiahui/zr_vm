---
related_code:
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/execution.h
  - zr_vm_library/include/zr_vm_library/common_state.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/state-lifecycle.md
  - docs/library-and-builtins/index.md
tests:
  - tests/core/test_session_checkpoint.c
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_module_specifier.c
  - tests/module/test_module_system.c
  - tests/ffi/test_native_extern_contract.c
doc_type: integration-guide
---

# C 宿主集成指南

本页给出从空进程到执行 ZR 模块的可复用顺序。示例使用 C API 的 ownership 约定：
返回 const pointer 通常借用，带 Free/Close 的对象由调用方释放；任何 managed value 都不能
用 memcpy 代替 value copy/root。

## 0. 选择宿主模式

| 模式 | 适用场景 | 入口 |
| --- | --- | --- |
| 快速库宿主 | 读取 .zrp、默认文件/source loader、注册常见 provider | ZrLibrary_CommonState_CommonGlobalState_New |
| 完全自定义 | 自己提供 allocator、source loader、AOT/native loader、日志 | ZrCore_GlobalState_New |
| 仅 binary | 没有 parser/compiler，只加载 .zro | global 不注入 compileSource |
| compiler/IDE | 需要 AST、semantic query、diagnostic snapshot | 显式 parser/compiler state |

快速库宿主仍要按需注册 provider；CommonState 不会自动加载所有可选模块。

## 1. 创建 global 和主 state

~~~c
SZrGlobalState *global =
    ZrCore_GlobalState_New(allocator, userArguments, 1u, &callbacks);
if (global == ZR_NULL) {
    return 1;
}
SZrState *state = global->mainThreadState;
~~~

GlobalState_New 会建立 allocator、GC、字符串表、基础原型、主线程 state 和模块 registry，
返回后不要再次调用 State_MainThreadLaunch。需要 secondary AttachedDomain state 时：

~~~c
SZrState *worker = ZrCore_State_New(global);
if (!ZrCore_State_MutatorLaunch(worker)) {
    ZrCore_State_Free(global, worker);
    return 1;
}
/* use worker only from its owning mutator */
ZrCore_State_MutatorExit(worker);
ZrCore_State_Free(global, worker);
~~~

同一个 state 只能由所属 mutator 使用；跨线程不要共享 stack、callInfoList 或 borrowed
string/object pointer。

## 2. 挂接 library 和 provider

~~~c
ZrLibrary_NativeRegistry_Attach(global);
ZrVmLibSystem_Register(global);
ZrVmLibMath_Register(global);
ZrVmLibContainer_Register(global);
ZrVmLibIteration_Register(global);
ZrVmTask_Register(global);
ZrVmLibNetwork_Register(global);
ZrVmLibFfi_Register(global);
~~~

实际函数名按所链接 provider 的 module.h 为准。注册失败时读取：

~~~c
EZrLibNativeRegistryErrorCode code =
    ZrLibrary_NativeRegistry_GetLastErrorCode(global);
const TZrChar *message =
    ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
~~~

provider phase 通过 State_SetProviderPhase 设置；Runtime state 不能消费 CompileTool/Test
provider。若使用 descriptor plugin，加载失败或 source path 失效时调用
InvalidateDescriptorPluginSource。

## 3. 解析、诊断和 AST 所有权

~~~c
SZrParserState parser;
ZrParser_State_Init(&parser, state, sourceBytes, sourceLength, sourceName);
parser.structuredErrorCallback = onStructuredDiagnostic;
SZrAstNode *ast = ZrParser_ParseWithState(&parser);
if (parser.hasFatalError || parser.hasError || ast == ZR_NULL) {
    ZrParser_Ast_Free(state, ast);
    ZrParser_State_Free(&parser);
    return 1;
}
~~~

parser state 持有 lexer、当前位置和 diagnostic storage；callback 收到的 location/message
只在 state/revision 有效。Ast 由调用方拥有，必须用 ZrParser_Ast_Free；不要在 state Free 后
读取 AST 或 diagnostic 指针。增量解析可使用 State_SeekToTokenStart、ParseTopLevelStatementWithState、
ParseExpressionWithState，但 offset 必须正好落在 token 起点。

## 4. 编译或加载 artifact

~~~c
SZrFunction *entry = ZrParser_Compiler_Compile(state, ast);
if (entry == ZR_NULL) {
    /* read compiler structured error */
}
ZrParser_Ast_Free(state, ast);
ZrParser_State_Free(&parser);
~~~

project host 应先用 ZrLibrary_Project_New(state, manifestText, manifestPath) 建立 manifest，
再通过 resolver 得到 canonical module key。CompileWithCurrentModuleKey 用于把 identity
传播到 call binding；不能把相对路径直接拼成 module name。只 binary 的 global 使用
AotModuleLoader，.zro/.zri/.zrs writer 由 Parser/Compiler C API 生成。

## 5. 调用和异常检查

~~~c
SZrTypeValue args[1];
SZrTypeValue result;
ZrLib_Value_SetInt(state, &args[0], 42);
if (!ZrLib_CallModuleExport(state, "app.main", "run",
                            args, 1u, &result)) {
    if (state->hasCurrentException) {
        /* copy/format the exception before reset */
    }
    ZrCore_State_ResetThread(state, state->currentExceptionStatus);
    return 1;
}
~~~

调用可能触发 GC、任务挂起或 provider callback。result/args 是当前 state value；需要跨
调用保存的 object 使用 TempValueRoot、NativeCallPin 或 ownership API。捕获异常时同时处理
currentException 和 currentExceptionStatus；只清状态会留下 stale exception。

## 6. 运行、暂停和终止

ZrCore_State_DoRun(state, entryName) 驱动入口并返回 EZrThreadStatus。正常退出后调用
State_Exit；secondary state 还要 State_Free。若安装 debug agent，先停止 agent 和 callbacks；
若启用 scheduler，先 pump/stop worker，再关闭 global。process.exit 是不可恢复转移，不能
依靠普通 exception handler 继续执行。

## 7. 关闭顺序

~~~text
stop debug/test/profile/coverage callbacks
  -> stop task/thread schedulers and join/ShutdownIsolatedSchedulers
  -> close FFI/network/file/assembly handles
  -> release prepared jobs, roots, AST/compiler/artifacts
  -> NativeRegistry_Free
  -> State_Exit
  -> State_Free (secondary only)
  -> GlobalState_Free
~~~

Global_Free 会回收主 state、GC 和全局 storage，但不会替宿主释放外部线程、动态库或自定义
allocator 的用户资源。关闭顺序错误的典型症状是 finalizer 访问已卸载 provider、callback
落到已释放 stack 或 module load diagnostic 被覆盖。

## 常见宿主错误

| 错误 | 后果 | 修复 |
| --- | --- | --- |
| 共享一个 state 给多个线程 | stack/GC race | 每个 mutator 独立 state + provider transfer。 |
| 保存 const pointer | reload/GC 后悬挂 | 复制字符串/值或注册 root。 |
| 未检查 bool/status | 继续使用未初始化 out | 失败立即读取 diagnostic 并停止。 |
| AST/Project 顺序释放错误 | use-after-free | AST -> parser state -> project/state。 |
| 关闭 global 前仍有 callback | native 调用崩溃 | stop callback/agent，再释放 global。 |
| 把 ZR_NULL 当脚本 null | 错误传播 | 脚本值用 ZrLib_Value_SetNull，C 指针按 ownership 处理。 |

完整 C 函数表见 [Core C API](c-api-core.md)、[Parser/Compiler C API](c-api-parser.md) 和
[Library C API](c-api-library.md)。
