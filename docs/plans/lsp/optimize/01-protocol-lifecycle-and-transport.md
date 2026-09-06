# LSP 协议生命周期与传输层实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `test-driven-development`, `systematic-debugging`, `using-vsdevcmd`, `evidence-driven-wsl-validation`, and `verification-before-completion` while executing this plan.

**Goal:** 让 native stdio server 严格遵循 JSON-RPC/LSP 生命周期、frame、取消和错误语义，并能确定性停止 reader、释放 LSP/context/global 状态。

**Architecture:** 将现在混在 `stdio_transport.c`、`stdio_requests.c` 与 `main` 中的 frame reader、JSON-RPC envelope、request registry、server lifecycle 和进程编排拆成五个窄模块。reader 只读取 frame 并入队；主线程验证 envelope 和 lifecycle；request context 持有自己的取消与依赖代际。

**Tech Stack:** C11、cJSON、Win32 threads/pthreads、Node.js protocol tests、ASan/LSan、Valgrind。

---

## Task 1：建立严格生命周期状态机

**Files:**
- Create: `zr_vm_language_server/stdio/stdio_lifecycle.h`
- Create: `zr_vm_language_server/stdio/stdio_lifecycle.c`
- Modify: `zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h`
- Modify: `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c`
- Modify: `zr_vm_language_server/stdio/stdio_requests.c`
- Test: `tests/language_server/stdio_protocol_conformance.js`

- [x] RED：普通 request 在 initialize 前返回 `-32002 ServerNotInitialized`；exit 之外的 notification 在初始化前被忽略；[当前协议回放](2026-09-07-plan01-task01-task02-protocol-negative-replay.md)。
- [x] RED：第二次 initialize 返回 `-32600 InvalidRequest`；shutdown 前 exit 返回进程码 1；shutdown 后 exit 返回 0；[当前协议回放](2026-09-07-plan01-task01-task02-protocol-negative-replay.md)。
- [x] RED：shutdown 后的普通 request 返回 `-32600`，notification 仅允许 exit；[当前协议回放](2026-09-07-plan01-task01-task02-protocol-negative-replay.md)。
- [ ] 实现明确状态：

```c
typedef enum EZrStdioLifecycleState {
    ZR_STDIO_LIFECYCLE_NEW = 0,
    ZR_STDIO_LIFECYCLE_INITIALIZING,
    ZR_STDIO_LIFECYCLE_RUNNING,
    ZR_STDIO_LIFECYCLE_SHUTDOWN,
    ZR_STDIO_LIFECYCLE_EXITED,
} EZrStdioLifecycleState;
```

- [x] Sub01：除 `exit` 外的 control notification 只在 `INITIALIZING` 或 `RUNNING` 状态生效；`$/setTrace` 在初始化前和 shutdown 后被忽略，且 shutdown/exit 顺序返回精确退出码；[记录](2026-09-07-plan01-task01-sub01-lifecycle-notifications.md)。
- [x] `initialized` notification 只允许把 INITIALIZING 转为 RUNNING；server 可以在 initialize response 后接受规范允许的请求，但必须记录 initialized 是否到达以便诊断客户端错误；[记录](2026-09-07-plan01-task01-sub02-state-transitions.md)。
- [x] 删除仅有 `shutdownRequested` 的隐式状态判断；生命周期状态和 `initializedNotificationReceived` 由 `SZrStdioLifecycle` 统一保存；[记录](2026-09-07-plan01-task01-sub02-state-transitions.md)。

## Task 2：验证 JSON-RPC envelope 和 params

**Files:**
- Create: `zr_vm_language_server/stdio/stdio_json_rpc.h`
- Create: `zr_vm_language_server/stdio/stdio_json_rpc.c`
- Modify: `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c`
- Modify: `zr_vm_language_server/stdio/stdio_request_dispatch.c`
- Test: `tests/language_server/stdio_protocol_conformance.js`

- [ ] RED：拒绝数组/标量顶层消息、缺失或错误 jsonrpc、bool/object/array id、非 object/array params、request 缺失 id。
- [x] notification 的 malformed params 只能记录日志，不发送 response；request 必须返回精确 `-32602`；[当前协议回放](2026-09-07-plan01-task01-task02-protocol-negative-replay.md)。
- [ ] 定义统一 envelope，而不是让每个 handler 用 `NULL`/空数组猜测错误：

```c
typedef struct SZrJsonRpcEnvelope {
    const cJSON *id;
    const TZrChar *method;
    const cJSON *params;
    TZrBool isRequest;
    TZrBool isNotification;
} SZrJsonRpcEnvelope;

typedef enum EZrLspHandlerStatus {
    ZR_LSP_HANDLER_OK = 0,
    ZR_LSP_HANDLER_INVALID_PARAMS,
    ZR_LSP_HANDLER_CANCELLED,
    ZR_LSP_HANDLER_CONTENT_MODIFIED,
    ZR_LSP_HANDLER_INTERNAL_ERROR,
} EZrLspHandlerStatus;
```

- [ ] handler 返回 status + result；“没有语义结果”可返回合法 null/empty，“解析失败”只能是 InvalidParams。
- [x] Sub05：call/type hierarchy 的参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍返回合法空数组；[记录](2026-09-07-plan01-task02-sub05-hierarchy-invalid-params.md)。
- [x] Sub06：implementation、foldingRange、selectionRange、documentLink 和 codeLens 的参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍返回合法空数组；[记录](2026-09-07-plan01-task02-sub06-editor-feature-invalid-params.md)。
- [x] Sub07：formatting、onTypeFormatting 和 codeAction 的 `textDocument` 参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍返回合法空数组；[记录](2026-09-07-plan01-task02-sub07-editing-invalid-params.md)。
- [x] Sub08：`completionItem/resolve` 的 item、label、resolve data URI 或 position 参数解析失败返回 `-32602 InvalidParams`，合法但未匹配 item 仍返回既有成功结果；[记录](2026-09-07-plan01-task02-sub08-completion-resolve-invalid-params.md)。
- [x] Sub09：`inlineValue`、`moniker` 和 `linkedEditingRange` 的 URI/position/range 参数解析失败返回 `-32602 InvalidParams`，各 provider 无结果仍保留合法空数组或 null；[记录](2026-09-07-plan01-task02-sub09-additional-editor-invalid-params.md)。
- [x] Sub10：semantic tokens full、full/delta 和 range 的 URI/position/range 参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍保留合法响应；[记录](2026-09-07-plan01-task02-sub10-semantic-token-invalid-params.md)。
- [x] Sub11：`workspace/symbol` 缺失或非字符串 `query` 参数返回 `-32602 InvalidParams`，合法空字符串 query 仍可执行；[记录](2026-09-07-plan01-task02-sub11-workspace-symbol-invalid-params.md)。
- [x] Sub02：`initialize` 的 params 缺失、`null`、标量或数组时返回 `-32602 InvalidParams`，且不进入初始化生命周期；[记录](2026-09-07-plan01-task02-sub02-initialize-params.md)。
- [x] Sub03：numeric request id 只接受有限、整数且处于 JSON-safe 范围的值，fractional id 返回 `-32600 Invalid Request`；[记录](2026-09-07-plan01-task02-sub03-integer-request-ids.md)。
- [x] Sub04：直接回归验证 envelope 对顶层消息、jsonrpc 版本、typed id、params 形状和 request/notification 分类；[记录](2026-09-07-plan01-task02-sub04-envelope-api.md)。
- [x] `parse_size_value`、`parse_position`、`parse_range` 拒绝小数、NaN/Infinity、负数、超 `INT32_MAX`/`TZrSize` 和逆序 range；[Plan 01 Task 2 Sub01 record](2026-09-07-plan01-task02-sub01-strict-numeric-parsing.md)。

## Task 3：实现有界 frame reader

**Files:**
- Create: `zr_vm_language_server/stdio/stdio_frame_reader.h`
- Create: `zr_vm_language_server/stdio/stdio_frame_reader.c`
- Modify: `zr_vm_language_server/stdio/stdio_transport.c`
- Modify: `zr_vm_language_server/include/zr_vm_language_server/conf.h`
- Test: `tests/language_server/stdio_protocol_conformance.js`

- [x] 设定并集中定义 `ZR_LSP_MAX_HEADER_BYTES`、`ZR_LSP_MAX_HEADER_COUNT`、`ZR_LSP_MAX_MESSAGE_BYTES`；默认最大 payload 16 MiB，允许通过测试注入更小限制；[记录](2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md)。
- [x] RED：过长 header、Content-Length 缺失/重复/负数/带垃圾后缀/溢出、payload 截断、错误换行、超过上限均不得分配或静默退出；[记录](2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md)。
- [x] 用 `strtoull` + `errno` + end pointer + `SIZE_MAX - 1` 检查；分配前验证 `contentLength + 1`；[记录](2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md)。
- [x] 区分 `EOF`、`MALFORMED_HEADER`、`PAYLOAD_TRUNCATED`、`TOO_LARGE`、`IO_ERROR`。只有干净 EOF 才关闭输入；可恢复的 JSON payload parse error 返回 `-32700`；[记录](2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md)。
- [x] 接受规范允许的 Content-Type/charset，拒绝显式非 UTF-8 charset；未知扩展 header 可忽略但计入总大小；[记录](2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md)。
- [x] Sub01：帧头读取在 C 字符串处理前拒绝 NUL，并检查全部显式 `charset` 参数，拒绝冲突或非 UTF-8 值；[记录](2026-09-07-plan01-task03-sub01-header-exactness.md)。
- [x] Sub02：显式但无值的 `charset` 参数返回 `MALFORMED_HEADER`，未知扩展参数继续保持可忽略；[记录](2026-09-07-plan01-task03-sub02-charset-parameter-syntax.md)。

## Task 4：修复 request id、取消与 ContentModified

**Files:**
- Create: `zr_vm_language_server/stdio/stdio_request_registry.h`
- Create: `zr_vm_language_server/stdio/stdio_request_registry.c`
- Modify: `zr_vm_language_server/stdio/stdio_transport.c`
- Modify: `zr_vm_language_server/stdio/stdio_requests.c`
- Test: `tests/language_server/stdio_protocol_conformance.js`
- Create: `tests/language_server/stdio_document_sync_conformance.js`

- [x] registry 以 JSON-RPC id 的类型和值为 key；数字 `1` 与字符串 `"1"` 不得冲突；[Task 4 Sub04 回归](2026-09-07-plan01-task04-sub04-request-registry-identity.md)。
- [x] Sub02：数字 request id 限定在 `+/-ZR_LSP_JSON_SAFE_INTEGER_MAX`，安全边界精确回显，字符串和数字类型继续分离；[记录](2026-09-07-plan01-task04-sub02-safe-numeric-request-ids.md)。
- [x] 重复活动 id 返回 InvalidRequest，不能复用同一个 cancellation node；[Task 4 Sub04 回归](2026-09-07-plan01-task04-sub04-request-registry-identity.md)。
- [x] `$/cancelRequest` 只标记匹配 id；未知 id 是无响应 no-op；[Task 4 Sub04 回归](2026-09-07-plan01-task04-sub04-request-registry-identity.md)。
- [x] Sub04：registry 直接按 JSON-RPC ID 类型和值区分活动请求，重复同类型 ID、精确取消、未知取消和完成后复用均有回归；[记录](2026-09-07-plan01-task04-sub04-request-registry-identity.md)。
- [ ] 从 request context 删除全局 inputGeneration 比较。ContentModified 由计划 02 的 dependency fence 判断；在该计划完成前只保留精确 cancellation，不发布可能误报的 `-32801`。
- [ ] 给 workspace diagnostics、workspace symbol、references、rename、hierarchy 等循环增加统一 cancellation callback，不只在 diagnostics bucket 循环里检查。
- [ ] request context 解析 `workDoneToken` 与 `partialResultToken`；长查询通过统一 progress sink 发送 `$/progress`，并在每批结果之间检查 cancellation/content fence。
- [x] Sub03：字符串和安全范围内的数字 `workDoneToken`/`partialResultToken` 在 progress notification 中保持原始 JSON identity，超界值在发送通知前返回 `-32602`；[记录](2026-09-07-plan01-task04-sub03-progress-token-identity.md)。
- [x] 处理 `$/setTrace` 并将协议 trace 写到 stderr/客户端 trace channel；任何 trace 都不得污染 stdout frame；[记录](2026-09-07-plan01-task04-sub04-set-trace-channel.md)。
- [ ] 明确串行执行模型的限制；若后续改线程池，core snapshot 必须先变为不可变且 thread-safe，本计划不提前并发 core。

## Task 5：确定性 teardown

**Files:**
- Create: `zr_vm_language_server/stdio/stdio_server.h`
- Create: `zr_vm_language_server/stdio/stdio_server.c`
- Modify: `zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h`
- Modify: `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c`
- Modify: `zr_vm_language_server/stdio/stdio_transport.c`
- Create: `tests/language_server/test_stdio_server_lifecycle.c`

- [x] RED：在同一进程中连续 New/Start/Shutdown/Free 100 次；禁止依赖 process exit；[当前 replay](2026-09-07-plan01-task05-deterministic-teardown-current.md)。
- [x] `SZrStdioRequestInputState` 保存 thread handle/id 与 stop flag；Free 顺序固定为 stop reader → join → drain messages/requests → destroy cond/mutex → free caches → free LSP context → free global state；[当前 replay](2026-09-07-plan01-task05-deterministic-teardown-current.md)。
- [x] 调试现有 context/global teardown access violation，定位首个 invalid free/use-after-free；不得保留“让 OS 回收”的注释作为 workaround；实现和历史 sanitizer/leak 结论见[当前 replay](2026-09-07-plan01-task05-deterministic-teardown-current.md)。
- [x] 对启动中途失败使用同一 teardown path，覆盖 global 创建后、context 创建后、input init 后、thread start 后的 fault injection；[当前 replay](2026-09-07-plan01-task05-deterministic-teardown-current.md)。
- [x] Windows 与 pthread 两端都测试 join；不再 `CloseHandle`/`pthread_detach` 后遗失 reader 所有权；[当前 replay](2026-09-07-plan01-task05-deterministic-teardown-current.md)。

## Task 6：验证门禁

- [ ] 运行 protocol conformance 全部负向用例，stderr 不得混入 stdout frame。
- [ ] WSL GCC/Clang ASan+UBSan 通过 lifecycle loop 与 stdio smoke。
- [ ] Valgrind `--leak-check=full --errors-for-leak-kinds=definite,indirect` 返回 0。
- [ ] MSVC Debug + Application Verifier 或 ASan（可用时）通过同一 lifecycle 测试。

```powershell
wsl.exe bash -lc 'cmake -S /mnt/e/Git/zr_vm -B /tmp/zr_vm-build-lsp-protocol-asan -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON -DBUILD_STATIC_LIB=OFF -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -no-pie"'
wsl.exe bash -lc 'cmake --build /tmp/zr_vm-build-lsp-protocol-asan --target zr_vm_language_server_stdio zr_vm_language_server_stdio_server_lifecycle_test --parallel 8'
wsl.exe bash -lc 'ctest --test-dir /tmp/zr_vm-build-lsp-protocol-asan --output-on-failure -R "language_server_stdio_(protocol|server_lifecycle|smoke)"'
```

在当前 WSL2 Clang 14 runtime 中，ASan 的 PIE allocator 保留区会与随机
可执行文件布局发生启动期冲突；验证构建需保留 `-no-pie` 以得到稳定的
sanitizer 入口。源码位于 Windows 挂载盘时，建议把构建目录放在 WSL ext4
（例如 `/tmp/zr_vm-build-lsp-protocol-asan`），否则动态库加载延迟可能触发
协议测试 3 秒响应期限的偶发超时；这不应被当作协议通过。上述选项不关闭
ASan/UBSan/LeakSanitizer，也不改变协议断言。

- [ ] 完成后更新 module docs，记录 lifecycle 状态、frame limits、error mapping 和 teardown ownership。
