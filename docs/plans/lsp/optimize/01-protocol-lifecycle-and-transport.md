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

- [x] handler 返回 status + result；“没有语义结果”可返回合法 null/empty，“解析失败”只能是 InvalidParams。Sub22–Sub27 已覆盖四十二个普通 handler、initialize、response envelope 和生命周期/通知发布状态；内部 serializer/runtime 分配和父级门禁仍按独立子项推进；[Sub27 记录](2026-09-07-plan01-task02-sub27-publication-state.md)。
- [x] Sub22：navigation 十个 handler 迁移为显式 status/result，JSON 根节点分配失败返回内部错误、取消返回 CANCELLED 并释放 JSON，正常空结果和参数错误保持；三工具链 11/11 handler 回归及八项相关检查通过，Valgrind 0 字节/0 错误；[记录](2026-09-07-plan01-task02-sub22-navigation-handler-status.md)。
- [x] Sub23：hierarchy、rename 和基础 editor query 的十三个 handler 迁移为显式 status/result，移除 rename 序列化失败的成功 null fallback；累计二十三个方法参与状态矩阵，三工具链九项目标通过，Valgrind 0 字节/0 错误；[记录](2026-09-07-plan01-task02-sub23-query-handler-status.md)。
- [x] Sub24：剩余十九个普通 handler 迁移完成，dispatcher 的四十三条路由全部转发显式状态；补齐 linked editing 取消清理及 resolve/report 根分配失败分类。三工具链 handler 14/14、扩展 CTest 各 16/17，诊断 smoke 保留底层失败；Valgrind 0 字节/0 错误。内部 serializer/runtime 分配和父级门禁仍待完成；[记录](2026-09-07-plan01-task02-sub24-dispatch-handler-status.md)。
- [x] Sub25：initialize 返回显式 status/result；base/optional capability 树的 223/227 个 JSON 分配点各覆盖 transient/persistent 故障注入，共 900 次注入，结果、协商字段和取消路径均无泄漏。GCC、Clang ASan/UBSan、MSVC 的 12 项 CTest 均通过，GCC Valgrind 0 字节/0 错误；workspace runtime、response publication 和其余嵌套 serializer/runtime 错误分类继续待办；[记录](2026-09-07-plan01-task02-sub25-initialize-result-contract.md)。
- [x] Sub26：result/error/notification response envelope 在所有嵌套 JSON 分配或挂接失败时原子失败、消费已转移的 JSON，并区分 BUILD_ERROR 与 IO_ERROR。58 个输出分配点的 transient/persistent 注入均无半帧或泄漏，三工具链 focused CTest 通过，GCC Valgrind 0 字节/0 错误；lifecycle、progress 与 diagnostics cache 对发送状态的 publication fence 继续待办；[记录](2026-09-07-plan01-task02-sub26-response-envelope-ownership.md)。
- [x] Sub27：initialize/shutdown 仅在响应写出成功后推进生命周期；progress 发送失败保留原结果，诊断失败不更新已发布版本缓存。新增 213 个 JSON 分配点的单次/持续注入、实际写出失败、shutdown 取消与后续重试均通过；三工具链七项 CTest 通过，三个直接测试 Valgrind 均为 0 字节/0 错误；[记录](2026-09-07-plan01-task02-sub27-publication-state.md)。
- [x] Sub05：call/type hierarchy 的参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍返回合法空数组；[记录](2026-09-07-plan01-task02-sub05-hierarchy-invalid-params.md)。
- [x] Sub06：implementation、foldingRange、selectionRange、documentLink 和 codeLens 的参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍返回合法空数组；[记录](2026-09-07-plan01-task02-sub06-editor-feature-invalid-params.md)。
- [x] Sub07：formatting、onTypeFormatting 和 codeAction 的 `textDocument` 参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍返回合法空数组；[记录](2026-09-07-plan01-task02-sub07-editing-invalid-params.md)。
- [x] Sub08：`completionItem/resolve` 的 item、label、resolve data URI 或 position 参数解析失败返回 `-32602 InvalidParams`，合法但未匹配 item 仍返回既有成功结果；[记录](2026-09-07-plan01-task02-sub08-completion-resolve-invalid-params.md)。
- [x] Sub09：`inlineValue`、`moniker` 和 `linkedEditingRange` 的 URI/position/range 参数解析失败返回 `-32602 InvalidParams`，各 provider 无结果仍保留合法空数组或 null；[记录](2026-09-07-plan01-task02-sub09-additional-editor-invalid-params.md)。
- [x] Sub10：semantic tokens full、full/delta 和 range 的 URI/position/range 参数解析失败返回 `-32602 InvalidParams`，provider 无结果仍保留合法响应；[记录](2026-09-07-plan01-task02-sub10-semantic-token-invalid-params.md)。
- [x] Sub11：`workspace/symbol` 缺失或非字符串 `query` 参数返回 `-32602 InvalidParams`，合法空字符串 query 仍可执行；[记录](2026-09-07-plan01-task02-sub11-workspace-symbol-invalid-params.md)。
- [x] Sub12：`workspace/diagnostic` 缺失、`null`、标量或数组 params 返回 `-32602 InvalidParams`，合法 object params 继续保留 report/progress 语义；[记录](2026-09-07-plan01-task02-sub12-workspace-diagnostic-invalid-params.md)。
- [x] Sub13：`workspace/willRenameFiles` 缺失或畸形 params、files 或文件 URI 返回 `-32602 InvalidParams`，合法无编辑路径继续返回 `null`；[记录](2026-09-07-plan01-task02-sub13-workspace-will-rename-invalid-params.md)。
- [x] Sub14：diagnostic 的 `previousResultId`、`identifier` 和 `previousResultIds` 可选字段严格校验字符串/数组及条目结构，畸形值返回 `-32602 InvalidParams`；[记录](2026-09-07-plan01-task02-sub14-diagnostic-optional-fields.md)。
- [x] Sub15：`semanticTokens/full/delta` 的 `previousResultId` 严格要求字符串，缺失或畸形值返回 `-32602 InvalidParams`，合法 delta identity 继续保留；[记录](2026-09-07-plan01-task02-sub15-semantic-token-delta-result-id.md)。
- [x] Sub16：`textDocument/references` 的 `context` object 与 `includeDeclaration` boolean 严格校验，缺失或畸形值返回 `-32602 InvalidParams`，合法引用结果/partial result 保持；[记录](2026-09-07-plan01-task02-sub16-references-context.md)。
- [x] Sub17：协商启用的 `textDocument/inlineCompletion` 缺失或畸形 params 返回 `-32602 InvalidParams`，合法 keyword-prefix 结果和 code-span 过滤保持；[记录](2026-09-07-plan01-task02-sub17-inline-completion-params.md)。
- [x] Sub18：`textDocument/codeAction` 的 `range` 严格通过 canonical parser，缺失或畸形值返回 `-32602 InvalidParams`，合法 action 的请求 range 与 snapshot 语义保持；[记录](2026-09-07-plan01-task02-sub18-code-action-range.md)。
- [x] Sub19：`codeAction/resolve` 缺失或畸形 item/data 返回 `-32602 InvalidParams`，合法 current snapshot 正常 resolve，stale snapshot 继续返回 disabled action；[记录](2026-09-07-plan01-task02-sub19-code-action-resolve-params.md)。
- [x] Sub20：`textDocument/codeAction` 的 `context.diagnostics` 严格要求 object array，`only` 若出现则要求 string array，畸形值返回 `-32602 InvalidParams`；[记录](2026-09-07-plan01-task02-sub20-code-action-context.md)。
- [x] Sub21：协商启用的 `textDocument/rangesFormatting` 要求 object params、URI 和 ranges array，所有 range 通过 canonical parser；缺失或畸形值返回 `-32602 InvalidParams`，合法空 ranges 仍返回成功空数组；[记录](2026-09-07-plan01-task02-sub21-ranges-formatting-params.md)。
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
- [x] Sub05：progress 模块独立管理 work-done/partial 编排；request cancellation callback 保留到 partial 发布和最终状态判定结束，首批与末批取消均返回 `-32800`；三工具链协议 52/52、同步 C 回归 6/6 和 CTest 4/4 通过；[记录](2026-09-07-plan01-task04-sub05-partial-result-cancellation.md)。
- [x] Sub06：七项同步 provider 回归覆盖 workspace/document symbols、references、rename、incoming/outgoing calls 和 subtypes，在首条结果后取消并验证清除回调后恢复完整查询；GCC、Clang ASan/UBSan、MSVC 均 7/7；[记录](2026-09-07-plan01-task04-sub06-provider-loop-cancellation.md)。
- [x] Sub07：修复 references、rename、workspace/document symbols 和 highlights 的取消结果泄漏；五个取消场景与正常对照均验证 runtime 分配归零，三工具链 CTest 6/6，Valgrind 0 字节/0 错误；[记录](2026-09-07-plan01-task04-sub07-cancelled-handler-cleanup.md)。
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
- [x] Valgrind `--leak-check=full --errors-for-leak-kinds=definite,indirect` 返回 0；当前 100-cycle lifecycle 为 0 bytes/0 errors，handler 取消清理也通过；[Sub01](2026-09-07-plan01-task06-sub01-native-memory-matrix.md)。
- [x] MSVC Debug + ASan 通过同一 lifecycle 测试；独立 `/fsanitize=address` 构建覆盖 context/global/reader 清理和启动失败注入；[Sub01](2026-09-07-plan01-task06-sub01-native-memory-matrix.md)。
- [x] Sub01：GCC ASan/UBSan、Clang ASan/UBSan 与 MSVC ASan 的 lifecycle/provider/handler/progress 验证通过；GCC 和 Clang 六目标通过，MSVC 六目标首轮通过但 protocol 复跑存在启动请求超时，完整 smoke 保留历史泛型补全失败；[记录](2026-09-07-plan01-task06-sub01-native-memory-matrix.md)。
- [x] Sub02：修复 diagnostic-fix smoke 暴露的 parser 错误恢复子节点泄漏；三工具链清理用例 15/15、既有 parser 用例 74/74、相关 CTest 5/5，Valgrind 0 字节/0 错误。Clang 原始回放 server exit 0、stderr 为空，但缺失诊断的历史断言仍失败，完整 smoke 门禁保持未完成；[记录](2026-09-07-plan01-task06-sub02-parser-recovery-ownership.md)。

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
