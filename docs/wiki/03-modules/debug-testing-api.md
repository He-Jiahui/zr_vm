---
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/debug.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/coverage.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/profile.h
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_testing/include/zr_vm_lib_testing/module.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime
implementation_files:
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/profile.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c
  - zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/debug/test_debug_agent.c
  - tests/debug/test_debug_evaluation_policy.c
  - tests/debug/test_coverage.c
  - tests/debug/test_profile.c
  - tests/testing/test_testing_module.c
doc_type: api-reference
---

# zr.debug 与 zr.testing API 参考

debug provider 面向调试器、coverage 和 profiler；testing provider 只在 Test phase 注册。
两者都采用 bounded snapshot，避免把 VM 内部 frame、string 或 metadata 指针泄漏给宿主。

## Debug agent

### 配置和停止原因

~~~c
ZrDebugAgentConfig config = {
    .address = "127.0.0.1:0",
    .suspend_on_start = ZR_FALSE,
    .wait_for_client = ZR_FALSE,
    .auth_token = "local-token",
    .stop_on_uncaught_exception = ZR_TRUE
};
ZrDebugAgent *agent = ZR_NULL;
TZrChar errorBuffer[256];
if (!ZrDebug_AgentStart(state, entryFunction, "app.main", &config,
                        &agent, errorBuffer, sizeof(errorBuffer))) {
    fprintf(stderr, "debug agent: %s\n", errorBuffer);
}
~~~

stop reason 枚举包括 NONE、ENTRY、BREAKPOINT、PAUSE、STEP、EXCEPTION、TERMINATED、
DATA_BREAKPOINT。breakpoint 有 LINE/FUNCTION 两类；exception filter 为 NONE/CAUGHT/UNCAUGHT。
ZrDebugBreakpointSpec 可包含 module/source/line/function、condition、hitCondition 和
logMessage。agent 只监听配置地址；生产环境应设置 token 或完全不注册 debug provider。

### 控制和查询函数

| 函数族 | 作用 |
| --- | --- |
| ZrDebug_AgentStart/Stop/GetEndpoint | 启停 agent、读取绑定 endpoint。 |
| SetBreakpoints/SetFunctionBreakpoints | 替换指定 source/module 的断点集合。 |
| SetExceptionBreakpoints | 设置 caught/uncaught 停止策略。 |
| Continue/Pause/StepInto/StepOver/StepOut | 控制当前暂停线程。 |
| ReadStack | 取得 frame snapshot；字符串和 locals 是 bounded preview。 |
| ReadTestManifest | 查询 Test phase 生成的 test entries。 |
| ReadScopes/ReadVariables | 读取 arguments、locals、closures、globals、prototype、statics、exception scope。 |
| Evaluate | 在当前 frame 受限求值。 |
| EvaluateWithCapabilities/Detailed | 显式提供 effect capability 并返回分类/诊断。 |
| ClassifyEvaluationEffect | 识别 PureValue、ReadOnly、Mutating、Blocking 等 effect。 |
| EvaluationEffectPolicy_Allows | 检查 policy 是否允许该 effect。 |
| Free | 释放 debug API 返回的 snapshot/diagnostic buffer。 |
| NotifyException/NotifyTerminated | runtime 主动通知外部调试器。 |

evaluate 默认拒绝写入、阻塞、文件/网络和线程切换；需要能力时必须显式传入 capability，
并检查 Detailed 结果中的 effect policy。Free 只释放 debug API 标记为 owned 的缓冲区，
frame/VM 指针仍由 state 管理。

## Coverage

~~~c
ZrDebugCoverage coverage;
ZrDebug_Coverage_Init(&coverage);
ZrDebug_Coverage_RegisterFunctionTree(&coverage, entryFunction);
ZrDebug_Coverage_Start(&coverage, state);
/* execute program */
ZrDebug_Coverage_Stop(&coverage);
for (TZrSize i = 0; i < ZrDebug_Coverage_GetLineCount(&coverage); ++i) {
    const ZrDebugCoverageLine *line = ZrDebug_Coverage_GetLine(&coverage, i);
    if (line != ZR_NULL && line->executable) {
        fprintf(stdout, "%s:%u %s\n", line->source, line->line,
                line->executed ? "covered" : "missed");
    }
}
ZrDebug_Coverage_Destroy(&coverage);
~~~

先 Init，再注册 function/tree；Start/Stop 可重复成对调用。line snapshot 的 function/source
引用是 borrowed，导出报告后再销毁 coverage。Reset 清计数但不改变已注册 function set。

## Profile

profile 支持 instrumentation 和 sampling：

~~~c
ZrDebugProfile profile;
ZrDebug_Profile_Init(&profile);
ZrDebug_Profile_StartWithSampling(&profile, state, 1000u);
/* execute */
ZrDebug_Profile_Stop(&profile);
TZrSize count = ZrDebug_Profile_GetEntryCount(&profile);
ZrDebug_Profile_Destroy(&profile);
~~~

GetEntry/FindByName 返回调用次数、总时长和 function identity；GetSampleCount、
GetTotalSampleCount、GetSample 返回采样帧。采样频率由 backend 解释，不能当精确 CPU 时间。

## Testing module

### 源码声明

~~~zr
#zr.testing.test#
#zr.testing.case(2, 3, 5)#
fn add_case(a: int, b: int, expected: int): void {
    zr.testing.equal(add(a, b), expected);
}

zr.testing.assert(value != null, "value must exist");
let error = zr.testing.throws<zr.system.exception.TypeError>(
    () => parse("bad")
);
~~~

稳定导出：assert(condition: bool, message?: string): void、
equal<T>(actual: in T, expected: in T): void、
throws<E extends Error>(action: fn() -> void): E。throws 只接受同步 action，不隐式 await。
zr.testing.test/case/skip attribute 生成 versioned TestManifest；production build 会裁剪
test roots，不生成隐藏 main。

### 结构化 failure

AssertionFailure 继承 Error，包含 assertionKind、sourceSpan、message、expected、actual 和
exception snapshot。C 结构 SZrTestingAssertionFailure 使用固定容量 buffer，并通过
truncated 标志报告截断；不要假设 snapshot 文本完整。测试 runner 应在独立 state/进程中执行
case，并把 timeout 映射为明确的 terminated/faulted 状态。

## Testing C API

~~~c
ZrVmLibTesting_Register(global);
ZrVmLibTesting_ClearLastFailure();
/* callbacks are also callable directly from a harness */
ZrVmLibTesting_Assert(context, result);
ZrVmLibTesting_Equal(context, result);
ZrVmLibTesting_Throws(context, result);

SZrTestingAssertionFailure failure;
if (ZrVmLibTesting_GetLastFailure(&failure)) {
    fprintf(stderr, "%s%s\n", failure.message,
            failure.truncated ? " (truncated)" : "");
}
~~~

failure storage 是 thread-local；每个线程在下一次 assertion 前读取/复制。ClearLastFailure
不会清除 VM exception，只清理 testing provider 的最后失败快照。共享库入口仍为
ZrVm_GetNativeModule_v1()。

## Debug 与 Test phase 边界

debug agent 可以读取 TestManifest，但 testing provider 不能在 Runtime phase 被普通 loader
覆盖。ZrVmLibDebug_RegisterSandboxed 用受限 evaluate policy；宿主若只需要 coverage/profile
可不注册脚本 debug module。关闭顺序为停止 agent/profile/coverage -> 释放 snapshot -> 关闭
state/global，避免 callback 访问已销毁 frame。
