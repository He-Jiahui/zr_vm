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
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/profile/test_coverage.c
  - tests/profile/test_profile_deterministic.c
  - tests/testing/test_assertions.c
doc_type: api-reference
---

# zr.debug 与 zr.testing API 参考

debug provider 面向调试器、coverage 和 profiler；testing provider 的导出需在 Test phase 访问。
调试协议输出和文本字段采用有界快照；coverage/profile 的 C API 中，function 身份指针借用自 VM，
宿主只能在 VM 对象有效期间使用。

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
| ClassifyEvaluationEffect | 按 property getter、allocation、call、native call、mutation、owner mutation 标志及规范化事实分类求值 effect。 |
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

先 Init，再注册 function/tree；Start/Stop 可重复成对调用。line snapshot 的 function 指针借用自 VM，
name/source 文本由 coverage 行快照持有；GetLine 返回的行指针在扩容、Reset 或 Destroy 后失效。
Reset 清空行记录；如需继续报告未执行的静态可执行行，需重新注册 function/tree。

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

## Coverage 的调用契约与报告边界

Coverage 由宿主分配，Init 只清初始状态；已拥有行数组或活动 hook 的对象应先 Destroy。
CLI 在 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:961 初始化，再由 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:102 登记入口树、zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:103 开始采集。
静态登记使未命中行也进入分母；LINE hook 在 zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:249 合并同一函数身份/行号，也能新增动态行。
RegisterFunction 只登记单函数；Tree 按有限、无环 child 树递归，分配失败保留已经登记的前缀。

### 所有权与读取时序

- 会话拥有行数组和 NUL 终止的 name/source 文本快照；容量含 NUL，长文本会截断。function 只借用 VM。
- 新增记录只有在扩容时才可能改变数组地址；重复登记或 LINE 仍可原位更新状态，地址稳定不保证内容不变。
- Reset 只清 line_count，不卸 hook、不释放容量。实现不拒绝活动对象，因此建议 Stop 后重置；再次报告完整静态分母前重新登记。
- zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1151 Stop 后才在 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1153 输出报告，zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1158 随后 Destroy。
  报告失败分支在 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1154 Destroy；早退 helper 的 Stop/Destroy 位于 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:81 和 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:82。
- Stop 恢复旧 hook、mask 和 COUNT 周期基值；core 的 zr_vm_core/src/zr_vm_core/debug.c:682 重启计数，不恢复之前剩余计数。
  Stop 不冻结对象；后续 Register、Reset、Destroy 仍改变结果。借用函数必须保持到最后一次 GetLine 读取。
- GetLine 用零基索引借内部行，越界/NULL 返回 NULL；Reset 使旧记录逻辑失效并可复用，Destroy 释放数组。
  全表顺序包括多函数与动态追加，不承诺整体按源行排序。

### 已知报告 I/O 边界

BUG: CLI 在 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:191 打开文件失败时返回 false；但 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:199 的 fprintf、zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:217 的 fclose 和 zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:219 的 fflush 均未检查返回值，zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:221 仍返回 true。调用方不能据此判定报告已完整写入；后续沿 CLI 的覆盖率输出错误传播核查此已知路径。

### 待核实设计边界

- TODO: zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:12 的进程级活动表无锁；沿多 state 宿主调度与并发采集用例核共同同步模型。
- TODO: zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:391 用旧快照覆盖活动期间外部 SetHook 配置；沿 zr_vm_core/src/zr_vm_core/debug.c:663 和嵌入宿主调用链核 hook 的所有权策略。
- TODO: zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:252 的 LINE 扩容失败静默漏记，zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:201 的报告查询没有 incomplete 状态；用可控内存失败路径核报告语义。
- TODO: zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:119 的容量倍增、zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:124 的字节乘法、zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:173 的计数加一及 zr_vm_lib_debug/src/zr_vm_lib_debug/coverage.c:309 的静态行号数组乘法均需核上界；沿 zr_vm_common/include/zr_vm_common/zr_common_conf.h:35、zr_vm_core/include/zr_vm_core/function.h:646、zr_vm_core/src/zr_vm_core/debug.c:744 核宽度和可达规模，尚不足以标 BUG。

### 现有验证覆盖

tests/profile/test_coverage.c:119 的入口树登记、tests/profile/test_coverage.c:120 的 Start 和 tests/profile/test_coverage.c:122 的执行形成正常路径。
断言 tests/profile/test_coverage.c:133 检查已执行 return，tests/profile/test_coverage.c:138 检查未执行 return，tests/profile/test_coverage.c:144 检查 Stop 后空 hook mask。
这些是现有测试的实际内容；本批未执行该运行时测试，不声称覆盖 OOM、并发采集、重复 Start 或外部 hook 冲突。
