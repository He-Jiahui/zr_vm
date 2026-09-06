# ZR LSP 实现审查与优化计划

> 审查日期：2026-08-22
>
> 审查对象：当前工作树，包括未提交的 L8 external-callable 语义改动
>
> 协议基线：LSP 3.17；3.18 能力只能作为客户端协商后的可选扩展

## 1. 结论

当前 LSP 已经拥有较宽的协议表面、项目索引、增量文档快照、编译器语义事实接入、原生 stdio 与 WASM 两种运行时，以及较多语言特性测试。但它还不能视为“能力声明可信、语义来源唯一、原生/Web 行为一致”的完整实现。

最主要的问题不是缺少若干孤立 handler，而是下面四条基础约束尚未建立：

1. `initialize` 返回的能力没有由客户端能力、协议版本和真实实现共同决定，存在明显的过度声明。
2. LSP 层仍维护第二套符号、类型、引用和控制流推断；编译器的 `TypeId`、`SymbolId`、`PlaceId` 与结构化诊断尚未成为唯一事实来源。
3. 请求没有绑定精确的文档、项目和 provider 代际，诊断缓存与 content-modified 判断可能错误。
4. stdio 与 WASM 是两套手工维护的协议适配器，功能、错误语义、工作区行为和版本号持续漂移。

本目录将整改拆为七个有严格依赖的计划。不得跳过前置里程碑去继续堆叠上层 provider。

## 当前执行状态（2026-09-06）

已建立[旧计划与全部 leaf 对照](2026-09-05-plan00-task01-sub01-execution-crosswalk.md)，
逐项记录 completed/pending/superseded、历史证据与后续责任；文档对照完成不代表
当前集成运行基线或父任务已验收。
Task 1 Sub02 已归档[冻结 GCC 失败基线](2026-09-05-plan00-task01-sub02-gcc-baseline.md)：
83 个 aggregate 成员中 73 pass、10 fail，收集器额外捕获 exit 0 的伪成功，
精确失败和责任层均保留；等待活动语义提交后的集成重验。
Task 1 Sub03 修复 project features 测试的 exit-zero 伪成功，三工具链均保留
14 个失败并返回 1；[harness 记录](2026-09-05-plan00-task01-sub03-project-test-exit.md)
不提升语义验收状态。

2026-09-06 的[current GCC replay](2026-09-06-plan00-task01-current-gcc-replay.md)
在独立当前 checkout 完成 focused build `841/841`、协议 conformance `30/30`、
inventory `3/3`、extension unit `41/41` 和 noEmit；aggregate suite 仍在缺少
`zr_vm_language_server_symbol_table_test` 处停止，三个 semantic focused executable
保留 8/14/1 个失败，因此 Task 1 全量门禁继续 pending。
随后 [Task 1 Sub04](2026-09-06-plan00-task01-sub04-current-gcc-aggregate.md)
补齐全部 83 个 aggregate 目标，完成 `375/375` 构建并收集到 74 pass / 9 fail /
64 failure blocks；缺失产物问题已排除，source-contract executable 通过。
该结果来自包含活动 overlay 的工作树，不能视作冻结提交的完整验收。

Task 2 Sub01 完成[能力登记表实际实现元数据](2026-09-05-plan00-task02-sub01-registry-metadata.md)：
明确 core/native adapter 归属及 runtime 字段约束，修正入口、导出、测试 ID 和
3.18 标记；GCC/Clang/MSVC 专项各 9/9。随后 Task 2 Sub02 完成[编译期 native
inventory](2026-09-05-plan00-task02-sub02-compiled-native-inventory.md)：实际
registry 30 条、native route 43 条、metadata-only control 3 个、orphan 0，
四个协商 profile 和三工具链 focused 各 14/14。WASM export/worker 映射、
control notification 行为和完整语义 acceptance 仍未闭合。
Task 2 Sub03 完成 [WASM 静态 capability inventory](2026-09-06-plan00-task02-sub03-wasm-static-inventory.md)：
检查 CMake 导出列表、C++ 定义/声明、bridge `ccall`、worker 路由和 13 项
semantic-token legend，并修正 Web legend。真实 `.wasm` 链接表仍因隔离 overlay
的真实构建在补齐另一会话未提交的 `semantic_scope_facts.h` 后仍因系统内存
压力在 `execution_dispatch.c` 处被 `Killed`，未生成链接资产，故仍待验收。

Plan 00 Task 5 的 [integrated capability inventory](2026-09-06-plan00-task05-integrated-inventory.md)
已将 native 四 profile 与 WASM source-level worker/bridge wiring 映射合并到同一个
machine-readable runner；当前 GCC、Clang 构建的 inventory/集成/regression CTest
为 3/3，MSVC 的两个 inventory CTest 为 2/2，WASM source mutation regression
为 9/9。该记录只关闭 source-level integrated
inventory 子项，linked asset、三种运行方式的完整 control/notification parity 和
完整语义验收仍保持 pending。运行器要求 Node 18+；完整命令与边界见
[WASM worker wiring acceptance](../../../../tests/acceptance/2026-09-06-lsp-wasm-worker-wiring.md)。
当前失败责任也已在 baseline/crosswalk 与 native inventory acceptance 中逐项链接
owner 和后续计划；GCC focused build、extension unit/noEmit 及 linked asset 仍是
独立门槛。

Plan 02 Task 1 的 [decoded-control Sub01](2026-09-06-plan02-task01-sub01-decoded-control-bytes.md)
补齐了 percent decode 后的 ASCII control-byte 拒绝边界，并验证失败时清空
 native path。GCC、Clang ASan+UBSan 与 MSVC 的 focused URI 矩阵分别通过 17/17、
17/17、19/19；该修复只关闭 Task 1 的一个边界子项，URI 全量矩阵、工作区和
文档快照父门禁仍保持 pending。

Plan 00 Task 2 Sub04 已完成 [WASM response contract correction](2026-09-07-plan00-task02-sub04-wasm-response-contract.md)：
WASM export payload 显式携带 JSON-RPC error code，bridge 在空指针、UTF-8 decode
或 JSON parse 后以 InternalError 失败并释放响应指针，worker 仅接受带 `data` 的成功
envelope。该修复取代了 [Plan 05 的初步记录](2026-09-06-plan05-task02-error-contract.md)，
但 Plan 05 Task 2 的 core status、versioned workspace edit 和 native/WASM golden
parity 仍未完成。

Task 3 Sub02 完成[3.17/3.18 optional capability 协商](2026-09-05-plan00-task03-sub02-optional-capabilities.md)：
inline completion 与 multi-range formatting 仅在客户端协商后发布，未协商请求返回
精确 `-32601`，并覆盖 cJSON 分配故障；三工具链各 11/11。完整协议生命周期和
Phase 00 inventory 尚未闭合。
Task 3 Sub03 完成当前提交后的[协议 conformance 重放](2026-09-06-plan00-task03-sub03-protocol-replay.md)：
GCC/Clang/MSVC 的 30 个协议案例均通过，并重放 MSVC workspace folder、save、
optional、resolve、file-operation 与 client-command smoke；父 Task 3 和集成
semantic baseline 仍未晋级。
Task 3 Sub04 完成[response envelope 验证](2026-09-06-plan00-task03-sub04-response-envelopes.md)：
三工具链的 30-case driver 均通过，11 个此前逃逸的响应反例均被原用例拒绝；
覆盖 typed ID、协议版本、result/error 互斥和错误消息。当前 `/mnt/e` CTest
仍观察到初始化超时，GCC 接受证据来自内容相同的 ext4 运行目录。

Plan 00 Task 4 Sub01 已完成[identity resolve 契约修复](2026-09-05-plan00-task04-sub01-identity-resolve.md)：
撤销四类 identity resolver，保留 native code-action snapshot 复验，按 runtime 描述
resolve 支持。专项通过 GCC/Clang/MSVC 与实际 Web worker callback 回归。
Task 4 Sub02 完成[definition alias 撤销](2026-09-05-plan00-task04-sub02-navigation-aliases.md)，
并验证保留的 local implementation 准确目标与范围；完整 definition/reaching-write
语义及跨 provider 矩阵仍未验收。
Task 4 Sub03 完成[空文件操作请求撤销](2026-09-05-plan00-task04-sub03-file-operations.md)，
保留操作的实际索引变化、准确版本化编辑和 stale disk 拒绝通过三工具链各 8/8。
下文为 2026-08-22 历史审查证据，不能直接当作当前实现状态。

Task 4 Sub04 完成[无类型事实的颜色能力撤销](2026-09-05-plan00-task04-sub04-untyped-color.md)：
移除 colorProvider、两个颜色方法和旧字符串扫描器；普通字符串/标识符保留准确
hover 与 definition。GCC/Clang/MSVC 各 12/12，扩展 unit 40/40/noEmit 通过。
源码契约仍是已归档的两项 constructor 失败，Plan 00 整体及后续阶段未晋级。

Task 4 Sub05 完成[无处理器的 willSave 声明撤销](2026-09-05-plan00-task04-sub05-save-notification.md)：
三工具链各 13/13，最终保存 fixture 各 6/6，磁盘刷新 mutation 证明 didSave
被忽略时会失败。完整编译后能力清单与集成语义基线仍待验收。

Plan 00 整体验收仍进行中；泛型 completion detail 与 possibly_uninitialized_read
stdio 用例仍为既有缺口。Task 4 Sub06 完成
[客户端空命令路由移除](2026-09-05-plan00-task04-sub06-client-commands.md)：
三工具链专项各 14/14，命令 fixture 各 10/10；综合 smoke 通过更新后的命令拒绝断言，
随后仍出现原有泛型 completion detail 失败。完整 compiled inventory 仍待单独验收。

Plan 00 整体验收仍进行中；泛型 completion detail 与 possibly_uninitialized_read
stdio 用例在已提交基线和本次修复后均失败。Task 3 Sub01 已修正
[取消 fixture 准备期限](2026-09-05-plan00-task03-sub01-cancellation-setup.md)，
三工具链各 30/30；该结果不代表活动查询 50 ms 取消预算已验收。
现有符号投影/类型查询会话的提交尚待接续，阶段 01-06 本轮晋级门槛未通过。
本轮 native inventory 的验收边界、已知失败责任和下一步 WASM owner 见
[acceptance record](../../../../tests/acceptance/2026-09-05-lsp-native-capability-inventory.md)。

2026-09-06 已记录 [Plan 01 Task 6 当前 sanitizer replay](2026-09-06-plan01-task06-sanitizer-replay.md)：
WSL ext4 上 Clang ASan/UBSan lifecycle、document sync 与 30-case protocol 连续
回放稳定通过，GCC Valgrind 生命周期和 MSVC stdio smoke 也有当前证据；Windows
挂载路径的 3 秒响应超时边界已保留，Plan 01 父级门禁仍等待完整跨工具链汇总，
WASM linked asset 仍未生成。

2026-09-07 已完成 [Plan 01 Task 2 Sub01 严格数值解析](2026-09-07-plan01-task02-sub01-strict-numeric-parsing.md)：
`parse_size_value`、`parse_position` 与 `parse_range` 现在有独立的边界回归矩阵，
并修复 64 位 `SIZE_MAX` 转换为 `double` 后舍入到 `2^64` 导致的 UBSan 未定义行为。
GCC、Clang ASan/UBSan 与 MSVC 专项均通过；Task 2 的 envelope、handler status
与完整协议父门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 3 Sub01 帧头精确性](2026-09-07-plan01-task03-sub01-header-exactness.md)：
reader 在 C 字符串处理前拒绝 header NUL，并检查所有显式 `charset` 参数，拒绝冲突或非 UTF-8
值；协议 30-case conformance 在 GCC、Clang ASan/UBSan 与 MSVC 均通过。Task 3 的完整限制、
失败分类、transport 与生命周期父门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 3 Sub02 charset 参数语法](2026-09-07-plan01-task03-sub02-charset-parameter-syntax.md)：
显式但无值的 `charset` 参数现在分类为 `MALFORMED_HEADER`，未知扩展参数仍可忽略；
GCC、Clang ASan/UBSan 的 33-case protocol conformance 与 lifecycle 专项通过。
Task 3 的完整限制、transport 和父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 3 Sub03 frame limits and failure classification](2026-09-07-plan01-task03-sub03-frame-limits-and-classification.md)：
集中 limits 使用 8 KiB header、32 个 header、16 MiB payload 默认值，`Content-Length`
在分配前经 `strtoull`/`errno`/end pointer/`SIZE_MAX - 1` 校验；oversize、malformed
header、截断 payload 和干净 EOF 分别映射到明确状态。GCC/MSVC 完整 33-case protocol
conformance 与 Clang ASan/UBSan frame focused 2/2 通过；Task 3 的上层生命周期父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 4 Sub02 数字 request ID 精度](2026-09-07-plan01-task04-sub02-safe-numeric-request-ids.md)：
数字 ID 限定在 `+/-ZR_LSP_JSON_SAFE_INTEGER_MAX`，安全上界保持精确回显，超界值返回
`-32600 Invalid Request`；字符串和数字 ID 继续按类型区分。GCC、Clang ASan/UBSan 与
MSVC 的 31-case protocol conformance 均通过。Task 4 的重复活动 ID、取消、进度和
ContentModified 父门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub02 initialize 参数形状](2026-09-07-plan01-task02-sub02-initialize-params.md)：
`initialize` 缺失、`null`、标量或数组 `params` 在生命周期转换前返回精确
`-32602 InvalidParams`；GCC protocol conformance 现为 31/31，生命周期专项通过。
该修复只关闭 initialize 的方法级参数边界，Task 2 的统一 envelope、handler status
及 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub03 integer request IDs](2026-09-07-plan01-task02-sub03-integer-request-ids.md)：
envelope 现在拒绝 fractional numeric request id，仍保留 JSON-safe 上下界和 typed ID
隔离；GCC 与 Clang ASan/UBSan 的 34-case protocol conformance 均通过。Task 2 的
统一 handler status/result 与父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub04 JSON-RPC envelope API](2026-09-07-plan01-task02-sub04-envelope-api.md)：
直接 C 回归覆盖顶层消息、jsonrpc 版本、typed id、params 形状、缺失 id notification
和显式 null id request 分类；GCC 与 Clang ASan/UBSan lifecycle CTest 均通过。Task 2
的统一 handler status/result 与父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Tasks 1-2 protocol negative replay](2026-09-07-plan01-task01-task02-protocol-negative-replay.md)：
当前 `2f94ce94` 的 GCC 与 Clang ASan/UBSan stdio protocol conformance 均为 34/34，
覆盖初始化前/重复初始化/shutdown/exit 顺序、malformed notification 无 response、
顶层/envelope/request-id/params 负向和 frame 分类。Task 2 的统一 handler status/result
与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub05 hierarchy invalid params](2026-09-07-plan01-task02-sub05-hierarchy-invalid-params.md)：
六个 call/type hierarchy request 的 malformed params 现在返回 `-32602`，provider 无结果
的合法空数组保持不变；GCC 与 Clang ASan/UBSan protocol conformance 均为 35/35，
lifecycle/protocol CTest 均为 2/2。统一 handler status/result 与 Plan 01 父级门禁仍保持
pending。

2026-09-07 已完成 [Plan 01 Task 1 Sub01 lifecycle notification 门禁](2026-09-07-plan01-task01-sub01-lifecycle-notifications.md)：
`$/setTrace` 在初始化前和 shutdown 后被忽略，`shutdown` 前后的 `exit` 分别返回
1/0；当前 GCC protocol conformance 33/33 且 lifecycle loop 通过。该记录只关闭
control notification 的状态边界，Task 1 其他状态记录和 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 1 Sub02 lifecycle 状态转移](2026-09-07-plan01-task01-sub02-state-transitions.md)：
独立 C 回归覆盖 `NEW`、`INITIALIZING`、`RUNNING`、`SHUTDOWN`、`EXITED`，并验证
`initializedNotificationReceived`、非法重初始化、重复 `initialized` 及 shutdown 前后
的精确退出码；GCC 与 Clang ASan/UBSan lifecycle 专项均通过。Task 1 的完整协议负向矩阵、
隐式状态清理审计和 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 4 Sub03 progress token identity](2026-09-07-plan01-task04-sub03-progress-token-identity.md)：
progress numeric token 使用 round-trip raw JSON 保留正负安全边界，超界 token 返回
`-32602` 且不发送通知；当前 GCC protocol conformance 33/33。Task 4 的完整
registry、取消、ContentModified 和统一 progress sink 父门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 4 Sub04 request registry identity](2026-09-07-plan01-task04-sub04-request-registry-identity.md)：
独立 C 回归锁定数字/字符串 ID 隔离、活动重复预留、精确取消、未知取消 no-op 和
完成后复用；GCC 与 Clang ASan/UBSan lifecycle 专项均通过。Task 4 的执行线性化、
活动查询取消、ContentModified 与统一 progress sink 父门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 5 deterministic teardown current replay](2026-09-07-plan01-task05-deterministic-teardown-current.md)：
当前 `2f94ce94` 源树重放 100 次同进程 `New/Start/Shutdown/Free`、`exit` 帧停止和四个
构造/reader-start fault point；GCC 与 Clang ASan/UBSan lifecycle CTest 均为 1/1，
直接目标均输出 `Pass - stdio server lifecycle`。Task 6 的完整当前提交跨工具链汇总仍保持
pending。

2026-09-07 已完成 [Plan 01 Task 4 Sub04 setTrace stderr channel](2026-09-07-plan01-task04-sub04-set-trace-channel.md)：
`$/setTrace` 在活动生命周期中接受 `off`、`messages`、`verbose` 三种 level，trace
只写 stderr，stdout 仍只包含 JSON-RPC frame；初始化前和 shutdown 后的 control
notification 无副作用。GCC、Clang ASan/UBSan focused、MSVC 协议回归通过，Task 4
的 registry、取消、ContentModified 和统一 progress sink 父门禁仍保持 pending。

2026-09-07 已完成 [Plan 03 Task 7 Astra P1 canonical symbol projection](2026-09-07-plan03-task07-canonical-symbol-projection.md)：
公开 `Lsp_FindSymbolAtUsageOrDefinition` 删除 `allScopes`、声明 range 和 retained
reference range fallback，普通 symbol 只消费 parser `SymbolAt` 与 stable `SymbolId`；
缺失 semantic context 和 identity mismatch 均 fail closed。GCC 与 Clang ASan/UBSan
parity 中新增两项均通过，整套 parity 保留基线四项失败和 Clang 既有 LSan 泄漏；Task 7
其余 consumer、`Task 7.63 ResolveTypeAtPosition`、完整矩阵和 Task 8 仍保持 pending。

## 2. 审查证据

### 2.1 协议与生命周期

- `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c:216-235` 只检查 `method` 是否为字符串，没有验证顶层对象、`jsonrpc == "2.0"`、请求 id 类型或 params 形状。
- `zr_vm_language_server/stdio/stdio_requests.c:25-79` 没有 initialize/initialized/shutdown 状态机：初始化前请求、重复 initialize、shutdown 后请求都可继续执行。
- `zr_vm_language_server/stdio/stdio_transport.c:499-548` 没有消息大小上限、整数溢出检查、重复 `Content-Length` 检查或严格 header 语法；畸形 frame 会被当成 EOF。
- `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c:250-258` 明确依赖进程退出回收全部状态，因为正常 teardown 会触发访问冲突。这是待修复缺陷，不是可接受的生命周期策略。
- `zr_vm_language_server/stdio/stdio_transport.c:269-291` 分离 reader thread 并丢弃句柄，无法 join 和确定性销毁。

### 2.2 能力声明

- `stdio_initialize.c:131-186` 和 `stdio_initialize_capabilities.c:16-77` 无条件声明几乎全部能力，没有保存或规范化 client capabilities。
- `stdio_initialize_capabilities.c:55-59` 声明 declaration/typeDefinition/implementation；但 `lsp_editor_features.c:1034-1055` 三者都直接调用 definition。
- documentLink、codeLens、inlayHint、workspaceSymbol 的 resolve provider 被声明为 `true`，对应 handler 只原样复制输入。
- `workspaceFolders.changeNotifications` 被声明为 `true`，而 `stdio_requests.c:97-101` 明确忽略 `workspace/didChangeWorkspaceFolders`。
- `textDocument/rangesFormatting` 已有 dispatch，但 initialize 没有声明 3.18 的 `documentRangesFormattingProvider`，客户端无法发现。
- 原生 server version、WASM server version 为 `0.0.1`，扩展 manifest 为 `0.0.6`。

### 2.3 文档、诊断和取消

- `stdio_diagnostics.c:73-109` 的 resultId 只包含当前文件版本、长度和内容 hash。依赖文件、选中项目或 metadata provider 改变时，诊断可能变化而 resultId 不变，客户端会收到错误的 `unchanged`。
- `stdio_transport.c:132-155,372-383` 用全局 `inputGeneration` 判断 ContentModified。任何文档或项目通知都会使所有活动请求失效，包括与请求无关的 URI。
- `stdio_lsp_parse.c:10-33` 接受小数并直接转整数，没有溢出检查；负 position 也会进入后续流程。
- `stdio_position_encoding.c:98-167` 将越界字符钳制到行尾、无效行保持原值；`stdio_document_content.c:39-63` 再把偏移钳制到内容边界。非法增量编辑没有被拒绝。
- 增量编辑忽略 `rangeLength`，无法发现客户端与服务端文本已经失步。
- `stdio_documents.c:123-153` 在 didClose 时直接删除 parser/analyzer 状态，没有把 workspace 文件从 open overlay 切回磁盘/project snapshot；关闭文件可能从 workspace diagnostics/query 集合消失。
- `incremental_parser.c:711,728` 仍对非等价 token 变更完整重解析，却将 incremental parse 标志设为启用。

### 2.4 URI、项目和工作区

- native path → file URI 在 `lsp_project.c:408-443` 与 `lsp_project_navigation.c:149-180` 重复实现，均不进行 URI 百分号编码。
- stdio 读盘在 `stdio_document_file.c:49-100` 维护第三套 URI 解码，并会将非 `file:` URI 当成本地路径。
- 工作区 folder capability 被声明但没有 root/workspaceFolders 状态；文件 watcher 和 file operation 不能正确限定到工作区集合。
- Web worker 只索引 `documents` 中已打开文档；其 `workspace/diagnostic` 因此不是实际工作区诊断。
- Web 客户端发送 `zrSelectedProjectUri` 和 `zr/selectedProject`，worker 没有处理；文件 watcher 通知也没有 handler。
- workspace symbol、references、diagnostics 等长查询没有统一消费 `workDoneToken`/`partialResultToken`，取消检查只覆盖少数循环。

### 2.5 语义与编辑能力

- `semantic_type_prototypes.c` 和 `semantic_analyzer*.c` 在 LSP 内重新解析/构造类型、作用域、引用和诊断，与 parser/compiler 的 canonical facts 并存。
- `semantic_analyzer_references.c:167-200` 将所有标识符默认标记为 read reference；default 分支还会跳过未覆盖 AST 节点。
- `semantic_analyzer_typecheck.c:2160-2206` 对 const 上下文使用已知不完整规则，并完全跳过函数参数兼容性检查。
- parser 的 `semantic_query.h` 已提供 TypeAt、CallAt、Definition、Declaration、References、Diagnostics、Property 查询，但缺少 visible-symbol、relation、implementation、call graph、type hierarchy、overload candidate 等项目级查询。
- `lsp_hierarchy.c` 通过名称和源文本扫描构造 call/type hierarchy，不能可靠处理重载、receiver、别名、泛型或跨文件关系。
- selection range 和 formatting 基于字符扫描；selection range 会把字符串/注释中的花括号当成块，formatting 忽略 `tabSize`、`insertSpaces` 等 FormattingOptions。
- document link 对 `.zrp` 使用字符串查找读取 JSON，而不是复用结构化 manifest parser。
- `reference_tracker.c:27-29` 在任一 URI 为空时把 source 视为相等，可能把不同文件的同坐标引用合并。

### 2.6 原生与 Web

| 能力 | 原生 stdio | Web/WASM | 审查结论 |
|---|---:|---:|---|
| completion resolve | 有 | 无 | 需要共享能力注册表 |
| signature help | 有 | 无 | WASM export 缺失 |
| semantic full/delta/range | 全部 | 仅 full | 缓存协议不一致 |
| declaration/typeDefinition/implementation | 伪实现 | 无 | 先撤销声明，再实现真实语义 |
| call/type hierarchy | 文本启发式 | 无 | 依赖 compiler relation facts |
| linked editing/moniker/inline value/color | 有 | 无 | 必须逐项决定共享、降级或移除 |
| workspace folders/watched files | 声明但不完整 | 忽略 | 两端都不满足声明 |
| pull diagnostics | parser file map | 仅打开文档 | resultId 与覆盖范围不一致 |
| provider error | JSON-RPC error/空结果混用 | 日志后返回空结果 | 必须统一错误分类 |
| rename/edit revalidation | 有 snapshot fence | 直接 `changes` | Web 有陈旧编辑风险 |

## 3. 风险优先级

### P0：先停止发布错误能力或错误结果

- 建立严格生命周期和 JSON-RPC 验证。
- 撤销 identity resolve provider 与 definition alias 等过度声明。
- 修复依赖变化后仍返回 unchanged diagnostics。
- 让 Web 的 selected project、watched files 和 workspace diagnostics 要么真实工作，要么不声明。
- 恢复可 join、可销毁、可 sanitizer 验证的 stdio 生命周期。

### P1：统一快照与语义身份

- 每个请求绑定 document version、project generation、provider generation 和 dependency set。
- 统一 file URI、native path、ModuleIdentity 和 workspace folder 规范化。
- 将上层 navigation/rename/hierarchy 从名称扫描迁移到 SymbolId/TypeId relation facts。
- 让 parser/compiler 结构化诊断替换 LSP 重复 typecheck。

### P2：补全用户能力和跨运行时一致性

- 基于语法树实现 selection/formatting/document links。
- 补齐或明确放弃 WASM provider；同一 capability registry 生成 initialize 结果。
- 增加大工作区性能、内存、取消、fuzz、原生/Web 金样对比。

## 4. 执行顺序

| 顺序 | 计划 | 完成门槛 |
|---:|---|---|
| 0 | [00-baseline-and-contract.md](00-baseline-and-contract.md) | 当前失败被冻结；能力声明与测试不再自证伪实现 |
| 1 | [01-protocol-lifecycle-and-transport.md](01-protocol-lifecycle-and-transport.md) | 生命周期、frame、JSON-RPC、取消与 teardown 通过负向测试和 sanitizer |
| 2 | [02-snapshots-workspaces-and-diagnostics.md](02-snapshots-workspaces-and-diagnostics.md) | URI/工作区/快照统一；诊断和编辑不会跨代际复用 |
| 3 | [03-canonical-semantic-query.md](03-canonical-semantic-query.md) | compiler query 覆盖上层所需事实；LSP 不再维护第二套 typecheck/reference 语义 |
| 4 | [04-editor-feature-correctness.md](04-editor-feature-correctness.md) | declaration/implementation/hierarchy/format 等能力具有真实语义和跨文件测试 |
| 5 | [05-native-web-capability-parity.md](05-native-web-capability-parity.md) | 单一 capability registry；原生/Web 对公共能力返回同构结果和错误 |
| 6 | [06-modularization-performance-and-acceptance.md](06-modularization-performance-and-acceptance.md) | 大文件按职责拆分；GCC/Clang/MSVC/WASM/sanitizer/perf 全门禁通过 |

## 5. 与既有 LSP 计划的关系

- `docs/plans/lsp/01-semantic-inference-core.md` 及 `01-semantic-core/`、`semantic-inference/` 下已经完成的 canonical facts 不重做；本目录的计划 03 只补查询缺口、统一 lifetime/exactness，并删除 LSP 重复推断。现有 ModuleIdentity/provider 成果作为计划 02/03 输入，当前 external-callable overlay 必须先完成自身 exact commit 与多编译器验证。
- `02-diagnostics-and-errors.md` 与 `02-diagnostics/` 已完成的 structured diagnostic/machine fix 成果保留；计划 02/03/04 修复 resultId、唯一诊断来源和 edit safety 的剩余问题。
- `03-lsp-robustness-and-position.md` 与 `03-robustness/` 中已完成的 position/snapshot/cancellation 用例保留；本目录增加的是严格协议状态机、依赖级 fence、真实增量 reparse 和 deterministic teardown。
- `04-debug-and-repl.md` 与 `04-debug-and-repl/` 的 debug/inline-value 契约继续独立推进；它们消费计划 02 的 snapshot 和计划 03 的 semantic facts，不能在 stdio/Web adapter 中自建语义扫描。
- `05-implementation-blueprint.md` 继续作为原路线图；执行本目录前应给旧 leaf 增加 cross-link，任何冲突以“能力声明必须真实、compiler facts 唯一、snapshot 验证优先”三条门禁为准。
- 不批量删除或重写既有 acceptance 记录；先建立 completed/pending/superseded crosswalk，保留历史证据与 commit identity。

## 6. 全局完成定义

- [ ] 所有声明的 capability 都有至少一个成功、一个 invalid params、一个 cancellation/stale snapshot 协议测试。
- [ ] 所有未实现或 identity-only 的 provider 均未声明。
- [ ] `TypeId`、`SymbolId`、`PlaceId`、ModuleIdentity、结构化诊断和 relation facts 均来自 parser/compiler 或 metadata projection，不由 LSP 文本重建。
- [ ] 原生与 Web 的公共请求使用同一语义 API、同一能力描述、同一错误分类和同一位置编码约束。
- [ ] workspace diagnostic 覆盖整个已索引工作区，resultId 包含所有语义依赖代际。
- [ ] request cancellation 只取消对应 id；ContentModified 只由该请求依赖的 snapshot 变化触发。
- [ ] stdio reader 可停止并 join；context/global/缓存可确定性释放；ASan/LSan/Valgrind 不依赖进程退出掩盖泄漏。
- [ ] 触及的生产文件原则上低于约 1000 行，超限文件有明确单一职责和书面例外。
- [ ] GCC、Clang、MSVC、WASM 和 VS Code desktop/web 验收记录引用同一 git commit 与构建配置。

## 7. 外部规范与仓库参考

- [LSP 3.17 Specification](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/)
- [LSP 3.18 draft specification](https://github.com/microsoft/language-server-protocol/blob/gh-pages/_specifications/lsp/3.18/specification.md)
- `lua/roslyn/src/Compilers/Core/Portable/Compilation/SemanticModel.cs` 展示 compiler-owned SymbolInfo、TypeInfo、DeclaredSymbol、LookupSymbols 和 Diagnostics 查询边界。
- `lua/src/lparser.c` 与 `lua/QuickJS-master/quickjs.c` 都在编译器作用域状态中解析变量身份；它们不支持在编辑器层用 token 名称重建绑定关系。
