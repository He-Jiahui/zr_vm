# ZRVM 语义分析与 LSP 优化实施计划

## 目标与约束

以 [optimize/index.md](E:/Git/zr_vm/docs/plans/lsp/optimize/index.md) 为执行主线，覆盖当前 syntax 设计规定的语义分析、诊断、跳转及相关编辑能力。发生冲突时遵循 [syntax 计划](E:/Git/zr_vm/docs/plans/syntax/README.md)，并记录冲突条款、采用的规则和对应测试。

当前 plan03 的 Task 3、Task 7、Task 8 尚未整体验收。接续现有会话的符号投影和类型查询提交，在现有 `main` 上推进；保留其他会话的未提交修改。

最终覆盖桌面 native、浏览器 WASM、桌面 Web 三种运行方式。`auto` 在桌面选择 native，在浏览器选择 WASM。

## 执行阶段

沿用原计划 Task 编号、依赖和验收条件。每个未完成 Task 为独立子里程碑；plan03 Task 7 按下列 consumer 组进一步拆分提交。前置门槛未通过，后续阶段保持未开始。

| 阶段                  | 实施内容与晋级条件                                           |
| --------------------- | ------------------------------------------------------------ |
| 00：基线与能力契约    | 接续已有提交后，建立 completed/pending/superseded 对照表，关联历史记录、当前实现和测试。重建失败清单；核对 registry、initialize、handler 和 WASM export，关闭 identity-only resolve 等过度声明。 |
| 01：协议与生命周期    | 按 Task 1–6 重验并修复状态机、JSON-RPC、frame、请求身份、取消、进度和 teardown。完成负向协议、重复启动销毁、故障注入及内存检查后晋级。 |
| 02：快照与工作区      | 按 Task 1–7 闭合 URI、multi-root、依赖快照、文档同步、增量解析和诊断缓存。验证关闭文件回到磁盘快照、provider reload 改变诊断代际、无关文件编辑不使请求失效。 |
| 03：统一语义事实      | 先闭合 Task 3 的无源码 external origin/virtual URI、真实非零 provider generation、多定义及跨 provider 矩阵；重验已完成的 Task 1、2、4、5、6。随后完成 Task 7 和 Task 8。 |
| 04：编辑功能          | 按 Task 1–7 分别完成四类跳转、调用/类型层次、引用与重命名、语法范围、formatter、code action/document link、语义展示；Task 8 验收通过后晋级。 |
| 05：native/Web 一致性 | 按 Task 1–6 完成共享能力清单、请求与错误契约、Web 工作区同步、provider 导出、桌面 Web 模式及协议差分测试；Task 7 验收三种运行方式。 |
| 06：最终验收          | 按职责拆分超大文件和测试，完成 fuzz、故障注入、性能与内存测量、跨平台及真实编辑器验证、独立审查和最终证据归档。 |

plan03 Task 7 的提交顺序固定为：导航/引用/高亮/重命名 → completion → hover/signature/inlay → semantic tokens → diagnostics → 删除剩余重复语义与建立源码边界检查。每组补全 source、binary、native、stale、unresolved 测试后提交。

## 接口与语义规则

- 扩展现有 compiler `SemanticQuery`、relation、call、display 和 diagnostic 契约。事实由 parser/compiler 或 metadata projection 发布，LSP 执行快照获取、只读查询和协议投影。
- `TypeId/SymbolId/PlaceId` 绑定所属快照；跨快照使用 ModuleIdentity、provider generation 和稳定 metadata 身份关联。跨 provider 测试比较规范契约，不直接比较不同上下文的数字 ID。
- 无源码声明由 metadata projection 提供 virtual URI 和准确范围；区分 declaration、definition、typeDefinition、implementation。动态调用、歧义、缺失或过期身份保留明确 unresolved 状态。
- 将剩余 LSP 类型、作用域和引用推断迁入 compiler 分析阶段。查询不得通过 AST 重推断、名称匹配或类型显示文本补造身份；语法恢复只能提供恢复信息。
- 按 syntax 01–14 建立检查项矩阵，覆盖泛型、重载、property、borrow/move/readonly/out、控制流、模块与 FFI、reflection、pooling、编译期生成、async、iterator 和 testing phase。
- 参考本地 Roslyn `SemanticModel`、位置查找测试，以及 Rust `TypeckResults`、rust-analyzer 导航实现和测试；记录参考依据与 ZR 的有意差异。
- native/WASM 共用能力描述、错误分类、诊断身份及 versioned edit plan。稳定协议基线为 3.17；3.18 能力独立协商。核心语义检查和跳转属于必交能力。

## 验证门槛

- 缺陷先建立可复现失败，再验证修复；先运行 compiler/query 单元，再运行 LSP consumer、项目和协议测试。记录具体失败名称、预期、实际及责任层。
- 语义矩阵覆盖遮蔽、同名跨模块、重载、泛型实例、receiver、alias chain、无源码声明、多实现、缺失事实、冲突身份和 provider reload；断言准确目标集合、范围及诊断内容。
- 快照矩阵覆盖 UTF-8/UTF-16、CRLF、非法增量编辑原子回滚、10,000 次编辑差分、直接/传递依赖、取消和旧编辑整体拒绝。Formatter 验证幂等及语义等价。
- 每项运行受影响的 GCC/Clang 检查和 MSVC 兼容验证；阶段验收运行完整规定矩阵。plan03 必须闭合原有 16-target 门槛及三工具链 stdio/CLI 测试。
- 最终运行扩展 unit/noEmit、WASM 构建、三种编辑器 smoke、native/WASM 公共协议零差异、sanitizer/Valgrind，以及包内版本和资产 hash 验证。
- 保持已冻结预算：warm hover p95 ≤50ms，completion/signature ≤100ms，单文件诊断 ≤250ms，100-file 增量诊断 ≤500ms，取消观察 ≤50ms，语义缓存 ≤256MiB，native 峰值内存 ≤512MiB。记录规模负载的 p50/p95/p99、缓存命中和 full-parse 比例。

## 状态记录与 Git

- 本轮统一采用 `plan_id=optimize`，逐项记录写入 `E:/Git/zr_vm/docs/plans/lsp/optimize/YYYY-MM-DD-planNN-taskNN-subNN-description.md`，复用既有记录时保留其历史证据。
- 每份记录包含 `## 状态与产出记录`，记录开始时间、实际完成时间（`+08:00`）、状态、完成项目、验证命令及结果、源码版本、产出路径和剩余门槛。未验收项保持“进行中”或“阻塞”，完成时间留空。
- 同步更新原计划 checkbox、索引链接和相关模块文档；模块文档包含代码、测试、计划来源以及 lifetime/exactness/ownership 契约。
- 每完成一个修复或功能子里程碑，立即提交其代码、测试、模块文档和完成记录；纯验收子项提交验证资产与记录。只暂存该子项明确拥有的路径。
- 提交前检查 diff、测试证据和文件清单；提交信息包含计划与子项编号。最终对同一已提交代码版本执行完整验收，并在汇总记录中关联各子项提交。