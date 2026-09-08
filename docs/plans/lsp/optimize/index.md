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

2026-09-07 已完成 [Plan 01 Task 2 Sub06 editor feature invalid params](2026-09-07-plan01-task02-sub06-editor-feature-invalid-params.md)：
implementation、foldingRange、selectionRange、documentLink 和 codeLens 的 malformed
params 现在返回 `-32602`，provider 无结果的合法空数组保持不变；GCC 与 Clang
ASan/UBSan protocol conformance 均为 36/36，lifecycle/protocol CTest 均为 2/2。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub07 editing invalid params](2026-09-07-plan01-task02-sub07-editing-invalid-params.md)：
formatting、onTypeFormatting 和 codeAction 的缺失 `textDocument` 参数现在返回
`-32602`，provider 无结果的合法空数组保持不变；当前 GCC 与 Clang ASan/UBSan
protocol conformance 均为 37/37，lifecycle/protocol CTest 均为 2/2。未声明能力的
`rangesFormatting` 仍由 capability gate 维持 `Method not found`，不纳入本子项。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub08 completion resolve invalid params](2026-09-07-plan01-task02-sub08-completion-resolve-invalid-params.md)：
`completionItem/resolve` 对空对象以及缺失或畸形 label/resolve data URI/position
现在返回 `-32602`，合法但未匹配 item 的成功结果保持不变；当前 GCC 与 Clang
ASan/UBSan protocol conformance 均为 38/38，lifecycle/protocol CTest 均为 2/2。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub09 additional editor invalid params](2026-09-07-plan01-task02-sub09-additional-editor-invalid-params.md)：
`inlineValue`、`moniker` 和 `linkedEditingRange` 的缺失或畸形 URI/position/range
参数现在返回 `-32602`，provider 无结果的合法空数组或 `null` 保持不变；当前 GCC
与 Clang ASan/UBSan protocol conformance 均为 39/39，lifecycle/protocol CTest 均为
2/2。统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub10 semantic token invalid params](2026-09-07-plan01-task02-sub10-semantic-token-invalid-params.md)：
semantic tokens full、full/delta 和 range 的缺失或畸形 URI/position/range 参数现在返回
`-32602`，provider 无结果的合法响应保持不变；当前 GCC 与 Clang ASan/UBSan protocol
conformance 均为 40/40，lifecycle/protocol CTest 均为 2/2。统一 handler status/result
与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub11 workspace symbol invalid params](2026-09-07-plan01-task02-sub11-workspace-symbol-invalid-params.md)：
`workspace/symbol` 现在要求字符串 `query`，缺失或非字符串参数返回 `-32602`，合法
空字符串 query 仍可执行；当前 GCC 与 Clang ASan/UBSan protocol conformance 均为
41/41，lifecycle/protocol CTest 均为 2/2。统一 handler status/result 与 Plan 01
父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub12 workspace diagnostic invalid params](2026-09-07-plan01-task02-sub12-workspace-diagnostic-invalid-params.md)：
`workspace/diagnostic` 现在要求 object params，缺失、`null`、标量或数组参数返回
`-32602`，合法空 object、`previousResultIds` 与 progress 字段继续保留 workspace
report 语义；当前 GCC 与 Clang ASan/UBSan protocol conformance 均为 42/42，
lifecycle/protocol CTest 均为 2/2。统一 handler status/result 与 Plan 01 父级门禁仍
保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub13 workspace willRenameFiles invalid params](2026-09-07-plan01-task02-sub13-workspace-will-rename-invalid-params.md)：
`workspace/willRenameFiles` 现在要求 object params、数组 `files` 以及每个文件项的
字符串 `oldUri` / `newUri`，缺失或畸形输入返回 `-32602`；合法空 files 与无编辑 rename
继续返回成功 `null`。当前 GCC 与 Clang ASan/UBSan protocol conformance 均为 43/43，
lifecycle/protocol CTest 均为 2/2。统一 handler status/result 与 Plan 01 父级门禁仍
保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub14 diagnostic optional fields](2026-09-07-plan01-task02-sub14-diagnostic-optional-fields.md)：
`textDocument/diagnostic.previousResultId`、`workspace/diagnostic.identifier` 及
`previousResultIds` 的可选值现在严格校验字符串、数组和 `{uri,value}` 条目结构，畸形
字段返回 `-32602`，省略字段与合法空数组保持既有 report 语义。当前 GCC 与 Clang
ASan/UBSan protocol conformance 均为 44/44，lifecycle/protocol CTest 均为 2/2。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub15 semantic token delta result id](2026-09-07-plan01-task02-sub15-semantic-token-delta-result-id.md)：
`semanticTokens/full/delta` 现在要求字符串 `previousResultId`，缺失或 `null`、数字、
数组值返回 `-32602`，合法 result id 的 unchanged/minimal delta 继续保持。当前 GCC
与 Clang ASan/UBSan protocol conformance 均为 45/45，lifecycle/protocol CTest 均为 2/2。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub16 references context](2026-09-07-plan01-task02-sub16-references-context.md)：
`textDocument/references` 现在要求 object `context` 以及 boolean `includeDeclaration`，
缺失或畸形输入返回 `-32602`；合法 references/partial-result 语义继续保持。当前 GCC
与 Clang ASan/UBSan protocol conformance 均为 46/46，lifecycle/protocol CTest 均为 2/2。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub17 inline completion params](2026-09-07-plan01-task02-sub17-inline-completion-params.md)：
协商启用 `textDocument/inlineCompletion` 后，缺失或畸形 params 返回 `-32602`，合法
keyword-prefix 结果和 code-span 过滤继续保持。当前 GCC 与 Clang ASan/UBSan protocol
conformance 均为 47/47，lifecycle/protocol CTest 均为 2/2。统一 handler status/result
与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub18 code action range](2026-09-07-plan01-task02-sub18-code-action-range.md)：
`textDocument/codeAction` 现在严格解析 canonical `range`，缺失、`null`、标量、数组或
逆序输入返回 `-32602`，合法 action 的请求 range 与 snapshot 语义继续保持。当前 GCC
与 Clang ASan/UBSan protocol conformance 均为 48/48，lifecycle/protocol CTest 均为 2/2。
统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub19 code action resolve params](2026-09-07-plan01-task02-sub19-code-action-resolve-params.md)：
`codeAction/resolve` 现在要求完整 snapshot data 的 object item，缺失或畸形输入返回
`-32602`；合法 current snapshot 正常 resolve，stale snapshot 继续返回 disabled action。
当前 GCC 与 Clang ASan/UBSan protocol conformance 均为 49/49，lifecycle/protocol CTest
均为 2/2。统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub20 code action context](2026-09-07-plan01-task02-sub20-code-action-context.md)：
`textDocument/codeAction` 现在要求 object `context`、object diagnostics array 以及可选
string `only` array，缺失或畸形输入返回 `-32602`；合法 quickfix/organize-import 过滤
继续保持。当前 GCC 与 Clang ASan/UBSan protocol conformance 均为 50/50，lifecycle/protocol
CTest 均为 2/2。统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub21 ranges formatting params](2026-09-07-plan01-task02-sub21-ranges-formatting-params.md)：
协商启用的 `textDocument/rangesFormatting` 对缺失或畸形 params、URI、ranges array
及任一 range 返回 `-32602`，已聚合编辑在失败时整体释放；合法空 ranges 仍返回空数组。
实现提交 `367e9b5a`。GCC、Clang ASan/UBSan、MSVC protocol conformance 均为 51/51，
lifecycle/protocol/optional-capability CTest 均为 3/3。Clang 使用独立 WSL ext4 构建
排除共享产物写入竞争。统一 handler status/result 与 Plan 01 父级门禁仍保持 pending。

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

2026-09-07 已完成 [Plan 01 Task 4 Sub05 partial-result cancellation](2026-09-07-plan01-task04-sub05-partial-result-cancellation.md)：
实现提交 `10fe1e20` 将 progress 编排独立为模块，并将 request cancellation callback
延长到 partial 发布与最终状态判定结束；首批或末批发送期间观测到精确请求 ID
取消时返回 `-32800`。GCC、Clang ASan/UBSan、MSVC 协议回放均为 52/52，同步通知
接收器驱动的 C 回归均为 6/6，progress/lifecycle/protocol/optional-capability CTest
均为 4/4。provider 长循环、content fence 与 Task 4/6 父门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 4 Sub06 provider-loop cancellation](2026-09-07-plan01-task04-sub06-provider-loop-cancellation.md)：
七项同步回归验证 workspace/document symbols、references、rename、incoming/outgoing
calls 和 subtypes 在首条结果后停止，清除 callback 后完整结果恢复。GCC、Clang
ASan/UBSan 和 MSVC 均为 7/7，Clang 无 sanitizer 报告。此项未修改 provider 实现；
parser 内部扫描、workspace diagnostics 和 50 ms 延迟门禁仍待验收。

2026-09-07 已完成 [Plan 01 Task 4 Sub07 cancelled-handler cleanup](2026-09-07-plan01-task04-sub07-cancelled-handler-cleanup.md)：
references、rename、workspace/document symbols 和 highlights 的 provider 取消后，
stdio handler 现在释放部分结果。RED 中五条路径各残留两块 runtime 分配；修复后
正常对照及五个取消场景均归零。三工具链 CTest 6/6，Clang 无 sanitizer 报告，
GCC handler 测试 Valgrind 为 0 字节/0 错误。Task 4/6 完整门禁仍 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub22 navigation handler status](2026-09-07-plan01-task02-sub22-navigation-handler-status.md)：
navigation 十个 handler 采用显式 status/result，区分根 JSON 分配失败、精确取消、
参数错误和合法空值。三工具链 handler 回归 11/11，八项相关目标均有通过证据；
Clang inventory 修正 Node 12 前置条件后单独通过。Valgrind 356,758 次分配全部释放、
0 字节/0 错误。其他 handler 和内部 serializer/runtime 分配错误仍 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub23 query handler status](2026-09-07-plan01-task02-sub23-query-handler-status.md)：
六个 hierarchy 方法、prepareRename/rename 和五个基础 editor query 采用显式状态。
测试矩阵累计覆盖二十三个 handler；GCC、Clang ASan/UBSan、MSVC 的九个相关目标
全部通过，Valgrind 712,914 次分配全部释放、0 字节/0 错误。剩余十九个普通请求
handler、内部序列化分配失败和 Plan 01 父级门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub24 dispatch handler status](2026-09-07-plan01-task02-sub24-dispatch-handler-status.md)：
剩余十九个普通 handler 迁移完成，四十二个 handler / 四十三条路由全部返回显式状态。
linked editing 取消清理和 resolve/report 根分配失败有直接回归；三工具链 handler
14/14，Valgrind 919,240 次分配全部释放、0 字节/0 错误。扩展 CTest 各 16/17；
诊断 smoke 的 GCC/MSVC 缺失诊断与 Clang parser 恢复泄漏均保留为失败，后者移除
所有普通请求后仍有 4,056 字节/20 次分配泄漏。initialize 和父级完整验收仍 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub25 initialize result contract](2026-09-07-plan01-task02-sub25-initialize-result-contract.md)：
initialize 现在返回显式 status/result，base/optional capability 树的 223/227 个 JSON
分配点各覆盖 transient/persistent 故障注入，共 900 次注入；失败结果、协商字段回滚、
预取消/末次分配取消和 controller 生命周期均有直接回归。GCC、Clang ASan/UBSan、MSVC
的相关 CTest 均为 12/12，GCC Valgrind 为 0 字节/0 错误。workspace runtime、response
publication、其余嵌套 serializer/runtime 分类和 Plan 01 父级门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub26 response envelope ownership](2026-09-07-plan01-task02-sub26-response-envelope-ownership.md)：
result/error/notification 的 JSON-RPC envelope 现在在任一嵌套 JSON 构造或挂接失败时
原子失败，所有拥有的 JSON 都由 transport 消费，并精确区分 BUILD_ERROR 与 IO_ERROR。
五类输出合计 58 个 allocation points 的 transient/persistent 注入均为零字节半帧、零
活动 allocation；typed id 和实际 stdout 写入失败都有直接回归。GCC、Clang ASan/UBSan
与 MSVC focused CTest 均为 1/1，GCC 相关 stdio 组为 4/4，Valgrind 1,563 alloc/free、
0 字节/0 错误。lifecycle、progress 和 diagnostics cache 对发送状态的 publication
fence，以及其余 serializer/runtime 分类和 Plan 01 父级门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub27 publication state](2026-09-07-plan01-task02-sub27-publication-state.md)：
initialize/shutdown 的生命周期状态只在成功写出响应后提交，shutdown 取消保持运行状态；
progress 失败停止后续批次并保留原结果，诊断去重缓存只记录成功发布的版本。新增 213 个
JSON 分配点各经历单次/持续故障，共 426 次注入；真实 stdout 失败、响应构造失败、
重试和取消有直接回归。GCC、Clang ASan/UBSan、MSVC 七项 CTest 均通过，initialize
15/15、progress 11/11、diagnostic publication 5/5，三项 Valgrind 均为 0 字节/0 错误。
工作区 runtime 回滚、其他嵌套 serializer 分类和 Plan 01 父级完整门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 2 Sub28 diagnostic JSON ownership](2026-09-07-plan01-task02-sub28-diagnostic-json-ownership.md)：
position/range/location、diagnostic content 以及 document/workspace full/unchanged
report 现在逐层检查 JSON 构造与所有权，任一子项失败会删除部分树并返回内部错误。
直接回归 9/9、发布回归 6/6，新增 717 个分配点和 1,434 次故障注入；三工具链
扩展 CTest 均为 13/13，Valgrind 两项均为 0 字节/0 错误。其他 handler serializer、
运行时分配错误分类和 Plan 01 父级完整验收继续 pending。

2026-09-07 已完成 [Plan 01 Task 6 Sub01 native memory matrix](2026-09-07-plan01-task06-sub01-native-memory-matrix.md)：
当前 `4b07a398` overlay 的 lifecycle 在 Valgrind 下完成约 330 万次分配/释放，
退出时 0 字节/0 错误。新增 GCC ASan/UBSan 与 MSVC Debug ASan 构建，结合 Clang
证据验收 lifecycle、provider、handler 和 progress 清理。GCC/Clang 六目标通过；
MSVC 六目标首轮通过，复跑 protocol 有一次启动请求超时，完整 smoke 仍受已登记
generic completion detail 失败限制。Task 6 仅勾选 Valgrind 与 MSVC lifecycle 条目。

2026-09-07 已完成 [Plan 01 Task 6 Sub02 parser recovery ownership](2026-09-07-plan01-task06-sub02-parser-recovery-ownership.md)：
修复数组、对象、括号表达式和函数声明的错误恢复清理。三工具链新测试 15/15、
既有 parser 测试 74/74、五个相关 CTest 全部通过；Valgrind 7,539 次分配全部释放，
0 字节/0 错误。Clang 原始 diagnostic-fix 回放中的 4,056 字节泄漏消失，server
exit 0 且 stderr 为空；缺失 possibly_uninitialized_read 的历史语义断言仍失败，
完整诊断与 Plan 01 门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 6 Sub03 generic type-use identity](2026-09-07-plan01-task06-sub03-generic-type-use-identity.md)：
parser 共享 producer 同时保留使用处实例 TypeId 和准确声明 SymbolId，泛型转换成功
后才发布根节点与嵌套类型引用；completion 按 SymbolId 投影实例详情。三工具链相关
单元均为 89/89，Valgrind 两项均为 0 字节/0 错误。GCC/MSVC 六个协议目标通过，
Clang 的 cancellation setup 超时复现并保持登记。三工具链完整 smoke 均越过原始
泛型失败，停于 MissingType callable detail 断言；Plan 01 和 Plan 03 父门禁未完成。

2026-09-07 已完成 [Plan 01 Task 6 Sub05 closed project diagnostic target](2026-09-07-plan01-task06-sub05-closed-project-diagnostic-target.md)：
关闭释放文件时同步清除匹配的项目 source record，工作区内磁盘恢复和其他 open
overlay 保持有效。三工具链 C 关闭回归 4/4、相关 CTest 3/3、关闭相关协议 3/3，
Valgrind 0 字节/0 错误。完整 smoke 均通过工作区诊断与随后取消/stale churn，推进
至 binary member definition 断言；父门禁保持未完成。

2026-09-07 已完成 [Plan 01 Task 6 Sub04 unresolved callable display](2026-09-07-plan01-task06-sub04-unresolved-callable-display.md)：
parser signature producer 保留显式未解析或冲突的 TYPE fact，分析阶段仅向相同
SymbolId/TypeId 的引用发布签名。三个工具链相关单元各 129/129，两个 Valgrind
均为 0 字节/0 错误。四个独立 smoke 通过；完整 smoke 均通过 MissingType 补全与
悬停并推进到 workspace diagnostic 内部错误。原有 Clang cancellation setup 与
父级完整验证继续 pending。

2026-09-07 已完成 [Plan 01 Task 6 Sub06 cast operand semantic facts](2026-09-07-plan01-task06-sub06-cast-operand-semantic-facts.md)：
cast 分析操作数后发布自己的目标类型，内部调用的 canonical 身份保持。
三工具链专项 6/6、call query 32/32、type graph 19/19；两个 Valgrind 均为
0 字节/0 错误。MSVC 原版 smoke 通过 binary 跳转并推进至 rename 显示断言；
Linux CLI artifact member 身份与导入缓存泄漏另行跟进，父门禁继续 pending。

2026-09-07 已完成 [Plan 01 Task 6 Sub07 rename canonical type assertions](2026-09-07-plan01-task06-sub07-rename-canonical-type-assertions.md)：
rename 回归直接检查 canonical OBJECT 到 DOUBLE，并验证准确的 hover 类型段。
三工具链 project 单项和三文件协议均通过，MSVC 完整 smoke exit 0、峰值
44.39 MiB；project 十项历史失败、Linux CLI artifact 与 Clang 内存门禁继续待办。

2026-09-07 已完成 [Plan 01 Task 6 Sub08 type-test string boundary](2026-09-07-plan01-task06-sub08-type-test-string-boundary.md)：
修正既有 parser 测试 source-name literal 的硬编码长度，Clang ASan 越界消失。
三工具链功能均为 124/124，GCC/MSVC exit 0；Clang 继续报告导入模块缓存
5,187 bytes/44 allocations、exit 1，完整 sanitizer 门禁保持未完成。

2026-09-08 已完成 [Plan 01 Task 6 Sub09 binary metadata source](2026-09-07-plan01-task06-sub09-binary-metadata-source.md)：
parser 删除 `.zri` 文本摘要替代读取，统一保留 `.zro` 的身份、声明位置及参数元数据。
三工具链身份回归 7/7、IO 生命周期 3/3；两个 Valgrind 均为 0 字节/0 错误。
GCC/MSVC 完整 smoke exit 0，峰值 36.98/44.91 MiB；Clang 功能推进至最后内存
门槛，678.66 MiB 超过 512 MiB，完整 sanitizer 验收和其他扩展失败仍待闭合。

2026-09-08 已完成 [Plan 01 Task 6 Sub10 compile-time import ownership](2026-09-08-plan01-task06-sub10-compile-time-import-ownership.md)：
compile-time binary import 补齐成功和失败出口的临时 IO source 释放，清除类型推断的
5,187 字节/44 次分配泄漏。三工具链所有权 4/4、相关 CTest 3/3、类型推断 124/124，
结合 Sub11 后 parity 20/20，均 exit 0；Valgrind 4,767 次分配全部释放、0 错误。
完整 LSP 内存与项目历史失败仍待办。

2026-09-08 已完成 [Plan 01 Task 6 Sub11 parity fixture ownership](2026-09-08-plan01-task06-sub11-parity-fixture-ownership.md)：
parity 释放两份 TypeAt 深拷贝与 hover，三个 URI 比较改用公开 Core API，修复 MSVC
共享库链接。包含 Sub10 底层导入清理时，三工具链均为 20/20、exit 0，Clang 无泄漏；
Valgrind 1,295,828 次分配全部释放、0 错误，完整 LSP 内存与其他项目失败保持待办。

2026-09-08 已完成 [Plan 01 Task 6 Sub12 smoke budget teardown](2026-09-08-plan01-task06-sub12-smoke-budget-teardown.md)：
完整 smoke 在 exit 前采集 OS 峰值，完成状态/stderr 验证后再执行预算断言。
一字节负向预算仍失败且 server exit 0；GCC/MSVC 默认预算分别 37.03/45.06 MiB、
exit 0。Clang 完成正常退出且 stderr 为空，但 668.64 MiB 超过 512 MiB，保留该门禁。

2026-09-09 已完成 [Plan 01 Task 6 Sub13 Clang sanitizer smoke](2026-09-09-plan01-task06-sub13-clang-sanitizer-smoke.md)：
Clang ASan/UBSan/LSan 在既有 1 GiB sanitizer 预算下 full smoke exit 0，峰值 654.71 MiB，
server exit 0、stderr 为空且无 sanitizer 报告。生产 512 MiB 门禁仍保持独立待验收。

2026-09-09 已记录 [Plan 01 Task 6 Sub14 current GCC protocol conformance](2026-09-09-plan01-task06-sub14-gcc-protocol-conformance.md)：
当前工作树在 WSL ext4 GCC cache 上重放完整 52-case protocol conformance 与注册 CTest，
分别为 52/52 和 1/1，真实 exit 0。Clang 旧 cache 的 50/52 超时和当前 checkout 缺少
MSVC stdio binary 均不计入跨工具链父门禁；生产 512 MiB 峰值和同版本 Clang/MSVC 重放继续待办。

2026-09-07 已完成 [Plan 03 Task 7 Astra P1 canonical symbol projection](2026-09-07-plan03-task07-canonical-symbol-projection.md)：
公开 `Lsp_FindSymbolAtUsageOrDefinition` 删除 `allScopes`、声明 range 和 retained
reference range fallback，普通 symbol 只消费 parser `SymbolAt` 与 stable `SymbolId`；
缺失 semantic context 和 identity mismatch 均 fail closed。GCC 与 Clang ASan/UBSan
parity 中新增两项均通过，整套 parity 保留基线四项失败和 Clang 既有 LSan 泄漏；Task 7
其余 consumer、`Task 7.63 ResolveTypeAtPosition`、完整矩阵和 Task 8 仍保持 pending。

2026-09-07 已完成 [Plan 03 Task 7.63 canonical type query](2026-09-07-plan03-task07-canonical-type-query.md)：
`ResolveTypeAtPosition` 现在只消费 parser `CanonicalTypeAt`、canonical graph 和已发布
`TypeId` semantic record；精确 expression fact 与已解析 type reference 均按 snapshot
事实复制，AST/type-inference/symbol/builder request-time fallback 全部移除。approximate
事实与断开 semantic context 的 RED fixture、GCC/Clang source-contract 均通过；完整
semantic analyzer/interface 的既有 baseline failures 保持登记。相关模块边界见
[LSP Type Query Capability Boundary](../../../cli-and-tooling/lsp-type-query-capability-boundary.md)。
Task 7 其余 consumer、Task 3/8 和完整矩阵仍保持 pending。

2026-09-07 已完成 [Plan 03 Task 7.64 completion consumer no reanalysis](2026-09-07-plan03-task07-canonical-completion-no-reanalysis.md)：
`CollectCompletionItems` 删除 canonical completion 为空时的 scoped analyzer 创建、分析根定位和
`Analyze`/`AnalyzeScope` request-time fallback；缺失或 stale receiver fact 保持 fail closed，
不在请求期物化新的 semantic facts。GCC source-contract 通过，native construct completion/signature
fail-closed regression 保持 PASS。Task 7 其余 consumer、Task 3/8 和完整矩阵仍保持 pending；相关
边界见 [LSP Completion Capability Boundary](../../../cli-and-tooling/lsp-completion-capability-boundary.md)。

2026-09-07 已完成 [Plan 03 Task 7.65 public hover no analyzer fallback](2026-09-07-plan03-task07-canonical-hover-no-analyzer-fallback.md)：
`Lsp_GetHover` 删除 `SemanticAnalyzer_GetHoverInfo` 请求期 fallback，只保留 structured
hover/signature/local query 与当前 snapshot 的 symbol markdown 投影。GCC source-contract 通过，
canonical native receiver/call hover 与 native construct completion/signature 回归保持 PASS；
metadata provider 的独立 external hover 路径、Task 7 其余 consumer、Task 3/8 和完整矩阵仍保持
pending。相关边界见 [LSP Hover Capability Boundary](../../../cli-and-tooling/lsp-hover-capability-boundary.md)。

2026-09-07 已完成 [Plan 03 Task 7.66 metadata hover no analyzer fallback](2026-09-07-plan03-task07-metadata-hover-no-analyzer-fallback.md)：
`CreateImportedMemberHover` 删除 `SemanticAnalyzer_GetHoverInfo` 请求期 fallback，external
imported-member hover 继续使用 resolved declaration symbol、owned content snapshot 与
markdown/FFI/leading-comment/source-label projection，并保留 descriptor formatting fallback。
GCC source-contract 通过；完整 metadata refresh/generation、Task 7 其余 consumer、Task 3/8
和完整跨工具链矩阵仍保持 pending。相关边界见
[LSP Metadata Hover Capability Boundary](../../../cli-and-tooling/lsp-metadata-hover-capability-boundary.md)。

2026-09-07 已完成 [Plan 03 Task 7.67 receiver completion no AST reinference](2026-09-07-plan03-task07-receiver-completion-no-ast-reinference.md)：
`TryCollectReceiverCompletions` 删除 completion-local recursive receiver prototype
scan 与 `ExpressionType_Infer` AST reinference，继续消费 canonical reference type
facts、symbol/type-environment、explicit binding、imported metadata 与 class/import
projection。三工具链 source-contract 通过，GCC/MSVC 正向/缺失类型回归和 Clang GDB 下的
ASan focused 回归通过；完整 runner 仍有已登记的失败。class/import/name/type-env
identity、其它 receiver consumer、Task 3/8 与完整矩阵仍保持 pending。
相关边界见 [LSP Receiver Completion Capability Boundary](../../../cli-and-tooling/lsp-receiver-completion-capability-boundary.md)。

2026-09-07 已完成 [Plan 03 Task 7.68 scope symbol owner lifetime](2026-09-07-plan03-task07-scope-symbol-owner-lifetime.md)：
parser type/method scope visitor 在 generic parameter 发布扩容 symbol array 前保存
canonical owner ID。六项强制搬迁回归通过；GCC/Clang ASan/UBSan/MSVC 的 symbols、
facts、calls 分别为 30/30、17/17、31/31，完整 interface 已越过原 signature UAF。
三套对齐 native 模块后的功能失败名单均保持原有八项；Clang 仍有 18,528 字节泄漏，
Task 3/7/8 与完整 sanitizer 门禁继续 pending。契约见
[Source Scope Fact Ownership](../../../parser-and-semantics/semantic-scope-fact-ownership.md)。

2026-09-07 已完成 [Plan 03 Task 7.69 empty file-list qsort guard](2026-09-07-plan03-task07-file-list-qsort-empty.md)：
`ListDirectory` 与 `Glob` 仅在至少两项时调用 `qsort`，避免成功空结果的 NULL
数组触发 UBSan。GCC、Clang ASan/UBSan、MSVC 的 `file_list` CTest 均为 `5/5`；
完整 LSP interface 不再报告两处 `file.c` qsort 问题，但完整功能失败、泄漏及
其它并发 support issue 继续 pending。

2026-09-07 已完成 [Plan 03 Task 7.70 canonical local binding identity](2026-09-07-plan03-task07-canonical-local-binding-identity.md)：
LSP typecheck 对普通变量声明使用 identifier pattern range 查找已有 canonical
symbol，并以原 `SymbolId`/`TypeId`/declaration range 注册 inferred binding，避免重复
semantic record。新增 inferred、explicit typed、nested shadowing identity 回归；GCC、
Clang ASan/UBSan、MSVC 的 analyzer identity/interface/source-contract 窄验证通过，
interface local structured query/hover 转为 PASS，固定失败集合由八项降为六项。相关契约见
[LSP Typecheck Canonical Local Bindings](../../../parser-and-semantics/lsp-typecheck-canonical-binding.md)；
parity write/reference、Clang LSan、Task 3/7/8 完整矩阵继续 pending。

2026-09-07 已完成 [Plan 03 Task 7.71 call type lifetime](2026-09-07-plan03-task07-call-type-lifetime.md)：
parser call-fact producer 在 signature interning 前保存 declared-function validity，避免类型
数组搬迁后读取借用 record。强制搬迁回归锁定 readonly return contract；三套工具链 calls
`32/32`、canonical type graph `19/19`。Clang analyzer 不再触发原 UAF，仍有九项既有功能
失败和 448 字节泄漏；完整 consumer 与 Task 3/7/8 门禁继续进行中。

2026-09-07 已完成 [Plan 03 Task 7.72 assignment reference roles](2026-09-07-plan03-task07-assignment-reference-roles.md)：
parser 完整赋值推断在左值发布 Write、右值发布 Read 后调用详细兼容性检查，LSP
typecheck 只消费该完整事实并投影结构化诊断。canonical binding declaration 提供
expected range，右值提供 source range；旧的 `semantic_analyzer_expected_type` helper
已删除。新增 ownership、scalar、nominal 和 nullable 失败赋值回归，以及
updated-snapshot 高亮角色断言。GCC、Clang ASan/UBSan、MSVC 的 reference-fact
`11/11`、local hover `12/12` 均真实 exit 0；semantic-query parity 均为 `18/18`
功能通过，GCC/MSVC exit 0，Clang 因既有 544 字节/4 分配 LSan 泄漏 exit 1；
GCC/MSVC type-inference `124/124`。三套 broad analyzer 仍为九项、完整 interface
仍为六项既有失败，ownership diagnostics 仍为冻结的 13 项失败，Clang 继续报告
既有 LSan；Clang type-inference 在固定字符串 hash 的当前越界测试处提前终止。详见
[Semantic Assignment Fact Ownership](../../../parser-and-semantics/semantic-assignment-fact-ownership.md)。

2026-09-07 22:24 +08:00 已完成 [Plan 03 Task 7.73 cross-snapshot external references](2026-09-07-plan03-task07-cross-snapshot-external-references.md)：
导入成员引用以 parser external owner、generation、metadata/signature token/hash 和
target kind 跨快照匹配。不同别名、同名本地符号和移除 AST 的 binary/native 回归覆盖
八种候选身份失效与目标失效；GCC/MSVC parity `20/20`，干净 Clang parity 的 20 项
功能用例通过但既有 LSan `5069 bytes/41 allocations` 使进程 exit 1；三套 source
contracts 仅保留并行 stdio 的 `cJSON_CreateString("declaration")` 基线失败。
模块契约见 [Cross-Snapshot External References](../../../cli-and-tooling/lsp-cross-snapshot-external-references.md)。

2026-09-07 23:17 +08:00 已完成 [Plan 03 Task 7.74 external highlight identity](2026-09-07-plan03-task07-external-highlight-identity.md)：
外部成员高亮改为消费 parser `ExternalReferences`，与跨快照 references 共用完整身份比较。
相同 SymbolId 不再接纳失效的 metadata 候选；十种候选失效、读取/写入角色、精确范围和
移除 AST 的 binary/native 回归通过。GCC/MSVC parity `20/20`，Clang 20 项功能通过但
保留既有 `5069 bytes/41 allocations` LSan；三套 local hover `12/12`。local query
两项历史失败和 source contracts 的单项 stdio 构造方式断言继续登记，Task 3/7/8 总门禁未关闭。

2026-09-07 23:26 +08:00 已完成 [Plan 03 Task 7.75 legend contract test boundary](2026-09-07-plan03-task07-legend-contract-boundary.md)：
移除将 semantic-token canonical query 源码检查绑定到旧 cJSON 创建方式的断言；legend
值由现有真实 initialize 响应的完整快照断言负责。GCC、Clang ASan/UBSan、MSVC 的
完整 source contracts 全部 exit 0，三套 optional-capabilities 协议测试均 `25/25`，
Clang 该组无 sanitizer 报告。canonical query 的源码边界检查继续保留。

2026-09-08 00:39 +08:00 已完成 [Plan 03 Task 3.26 analysis provider generation](2026-09-07-plan03-task03-sub26-analysis-provider-generation.md)：
parser 在分析时将宿主提供的 64 位代际写入 external call/member facts，script reset 保留
输入，显式 context reset 归零。三套 symbols `37/37`、calls `32/32`、relations `29/29`、
source contracts `76/76`、parity `20/20` 均 exit 0；Clang 最终窄门禁包含并行 Sub10/Sub11
清理支持且无 sanitizer 报告。真实 reload 与跨项目代际、virtual URI 及父级总门禁继续未完成。

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
