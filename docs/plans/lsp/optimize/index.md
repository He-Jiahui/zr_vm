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

## 当前执行状态（2026-09-05）

已建立[旧计划与全部 leaf 对照](2026-09-05-plan00-task01-sub01-execution-crosswalk.md)，
逐项记录 completed/pending/superseded、历史证据与后续责任；文档对照完成不代表
当前集成运行基线或父任务已验收。
Task 1 Sub02 已归档[冻结 GCC 失败基线](2026-09-05-plan00-task01-sub02-gcc-baseline.md)：
83 个 aggregate 成员中 73 pass、10 fail，收集器额外捕获 exit 0 的伪成功，
精确失败和责任层均保留；等待活动语义提交后的集成重验。

Plan 00 Task 4 Sub01 已完成[identity resolve 契约修复](2026-09-05-plan00-task04-sub01-identity-resolve.md)：
撤销四类 identity resolver，保留 native code-action snapshot 复验，按 runtime 描述
resolve 支持。专项通过 GCC/Clang/MSVC 与实际 Web worker callback 回归。
Task 4 Sub02 完成[definition alias 撤销](2026-09-05-plan00-task04-sub02-navigation-aliases.md)，
并验证保留的 local implementation 准确目标与范围；完整 definition/reaching-write
语义及跨 provider 矩阵仍未验收。
Task 4 Sub03 完成[空文件操作请求撤销](2026-09-05-plan00-task04-sub03-file-operations.md)，
保留操作的实际索引变化、准确版本化编辑和 stale disk 拒绝通过三工具链各 8/8。
下文为 2026-08-22 历史审查证据，不能直接当作当前实现状态。

Plan 00 整体验收仍进行中；泛型 completion detail 与 possibly_uninitialized_read
stdio 用例在已提交基线和本次修复后均失败，Clang 取消用例有一次并发验证超时。
现有符号投影/类型查询会话的提交尚待接续，阶段 01-06 本轮晋级门槛未通过。

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
