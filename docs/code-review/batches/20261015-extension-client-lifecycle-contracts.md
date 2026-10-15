---
related_code:
  - zr_vm_language_server_extension/src/languageClientLifecycle.ts
  - zr_vm_language_server_extension/src/languageServerSession.ts
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/browser.ts
  - zr_vm_language_server_extension/test/languageClientLifecycle.test.js
  - zr_vm_language_server_extension/test/extensionRestart.test.js
implementation_files:
  - zr_vm_language_server_extension/src/languageClientLifecycle.ts
  - zr_vm_language_server_extension/test/languageClientLifecycle.test.js
  - docs/code-review/coverage/zr_vm_language_server_extension_client_lifecycle.tsv
plan_sources:
  - docs/code-review/comment-standard.md
  - docs/code-review/batches/20261013-review-plan.md
  - "user: extension client lifecycle contract lease r1, 2026-10-15"
tests:
  - zr_vm_language_server_extension/test/languageClientLifecycle.test.js
  - zr_vm_language_server_extension/test/extensionRestart.test.js
doc_type: workflow-detail
---

# Extension 客户端生命周期与传输恢复契约

## 范围与单元清点

本批审查 `languageClientLifecycle.ts` 及其现有 Node 测试，按当前 AST 和实际调用链登记
46 个单元。旧台账 30 行中保留 28 个精确 key；将状态/动作常量行拆成三个协议域、将
FakeClient 的 `error` 与 `closed` 合并行拆成两个回调 key，再补齐 9 个接口方法签名、2 个
TypeScript 回调和 2 个测试回调，共净增 16 个单元。没有把两个函数类型注解额外算作实现。

最终包括 31 个 TypeScript 单元和 15 个测试单元：3 个常量域、6 个类型/接口、9 个接口方法、
12 个有函数体的 TypeScript 单元、1 个闭包状态契约，以及测试中的 1 个 FakeClient 类型和
14 个函数体。Leaf 对应 46 行。三类旧问题仍由五个细分 key 承载：SDK 枚举升级核对分为状态、
ErrorAction 与 CloseAction 三域；FakeClient 不能证明 SDK 重启策略；close 测试没有断言可区分的
委托结果和 STOPPED 中间值。其余 40 项为 `commented`，1 项简单 FakeClient 构造为 `no-comment`，
没有新增或推测 `BUG`。

净新增的 13 个 callable key：

- `DisposableLike::dispose`、`ErrorHandlerLike::error`、`ErrorHandlerLike::closed`、
  `LanguageClientLike::stop`、`LanguageClientLike::createDefaultErrorHandler`、
  `LanguageClientLike::onDidChangeState`、
  `TransportAwareLanguageClientLifecycle::attachClient`、
  `TransportAwareLanguageClientLifecycle::isTransportBroken`、
  `TransportAwareLanguageClientLifecycle::dispose`：9 个接口方法签名。
- `createTransportAwareLanguageClientLifecycle::isRetired-default` 与
  `createTransportAwareLanguageClientLifecycle::attachClient::onDidChangeState-listener`：
  默认参数箭头和状态通知回调。
- `FakeClient.onDidChangeState::dispose::listeners-filter-entry-not-listener` 与
  `a retired session never delegates automatic restart after connection closure::isRetired-reader`：
  listener 身份过滤谓词和测试传入的退休状态闭包。

此外，`FakeClient.defaultErrorHandler.error` 和 `.closed` 是旧合并 key 的拆分结果，不计入 13 个
净新增 callable；协议常量拆成三个领域增加 2 行，fake handler 拆分增加 1 行，因此总计为
`30 + 3 + 13 = 46`。

导出的常量、类型、接口、方法和函数现有中文 JSDoc 按其适用范围说明 Markdown 契约小节。
9 个接口方法各自解释职责、参数、结果、调用时机、借用和失败边界。内部回调也分别登记；
测试的固定委托、同步状态事件和纯身份筛选没有被描述成真实 SDK 或传输行为。

## 调用链与生命周期

桌面 `extension.ts` 与 Web `browser.ts` 在创建客户端前建立 lifecycle，把 `errorHandler` 放入
`clientOptions` 并由 `session.own` 登记释放；客户端构造后、启动前调用 `attachClient`。绑定时
读取客户端的 `createDefaultErrorHandler(maxRestartCount)` 并订阅 `onDidChangeState`。错误和关闭
事件由 vscode-languageclient 派发：`handleConnectionError` await `errorHandler.error`，
`handleConnectionClosed` await `errorHandler.closed`。前者对已毁流写错误返回 Continue 并置观察标记，
普通连接错误交给 SDK 默认策略；后者同步读取动态 `session.retired`，检查时已退休则返回 DoNotRestart，
仍活跃时返回 SDK 默认关闭策略。适配器不给 SDK 的重启时机或成功状态作保证。

`LanguageServerSession.dispose` 先 abort，使 `retired` 同步变为 true，再以逆序执行已登记清理。
由于 lifecycle 在 `startClient` 前登记，客户端清理先于 observer 释放。停止过程中到达的关闭通知
仍可能访问 SDK 持有的 errorHandler，因此退休判断在回调时动态读取。lifecycle.dispose 只释放本层
状态监听并清除委托/标记；它不停止客户端，也不会把已经交给 SDK 的 handler 对象变成不可调用。

错误处理器是 async 函数：同步异常分类和状态更新发生在每次调用中，SDK 委托的 Promise 结果交回 SDK；
拒绝不会在适配层吞掉。当前锁定 SDK 的 `handleConnectionClosed` 捕获 `closed()` 拒绝，而局部结果初值为
DoNotRestart，因此该版本会走停止清理路径；这是本地依赖源码事实，不是本批 Node 用例覆盖。状态 listener
则同步消费转换事件的 `newState`，不保存事件，也不声明额外的并发或重入保证。默认 `isRetired` 闭包恒为
false；返回的 `errorHandler.closed` 会捕获工厂收到的谓词。`lifecycle.dispose` 清空 delegate 和 listener，
但不会清掉该闭包；只要返回对象、client options 或 SDK 仍保留 handler，谓词在 dispose 后仍可能被调用。
调用方须让谓词读取的状态覆盖迟到回调的生命周期。独立调用者若需要阻止旧客户端重启，必须传入动态谓词。 重绑仍共用原 errorHandler 和可变 delegate，旧客户端须停止经同一 handler 派发，否则其迟到回调会读取新委托。状态监听释放仅撤销订阅，已进入 SDK 派发快照的回调仍可能完成；本层不承诺撤回该轮调用。

## 测试证据与限制

本文件的 4 个 Node 测试覆盖错误分类、毁流写错误的 Continue 与标记、关闭后状态事件最终清标记、
以及退休值变化后禁止返回 Restart。FakeClient 的普通错误 handler 固定返回动作 99，但现有用例从不
进入或断言该 delegate。FakeClient 的 close delegate 与 lifecycle 未绑定时回退都返回 Restart；一个测试
丢弃 `closed()` 的结果，因此没有证明调用了 delegate，也没有断言 STOPPED 到达的即时观察值。
测试最后只在 RUNNING 后检查 false，不能将其归因于特定一次状态转换。重启动作的第一次断言同样无法
区分 SDK delegate 与适配器回退。

FakeClient 的 `createDefaultErrorHandler` 忽略重启预算；其事件派发为同步数组遍历；本测试没有断言
listener 释放后数组内容。仓内 `test/extensionRestart.test.js` 和 `test/helpers/extensionHost.js` 提供
退休会话集成线索，但 helper 仍是固定动作替身，属于既有 session-restart 审查范围，不扩充本批写路径。
本批测试不证明真实 vscode-languageclient 的 transport 关闭顺序、延迟启动、预算耗尽、VS Code 宿主行为
或多次 SDK 重启时序。

本地锁定依赖为 `vscode-languageclient` 8.1.0。随仓安装的
`node_modules/vscode-languageclient/lib/common/client.js` 显示 `createDefaultErrorHandler` 对未指定预算使用
4，负预算同步抛错（:494–498）。默认 `error` 处理器只在 `count` 为 truthy 且 `count <= 3` 时返回
Continue，其余输入返回 Shutdown（:126–130）。默认 `closed` 每次记录时间：重启次数不超过预算时返回
Restart；超预算且首末时间相差不超过 3 分钟时返回 DoNotRestart，否则移除最早记录并返回 Restart
（:132–145）。SDK 收到 Restart 后调用 `start().catch(...)`，不等待启动完成（:1133–1140）；关闭 handler
拒绝则保留初始 DoNotRestart 结果，进入停止清理（:1111–1129）。这些是锁定包源码的静态事实，不是本批
FakeClient 或真实宿主/transport 用例的覆盖，也不证明所有已发布环境行为；升级 enum 与策略仍须独立复核。

## 锚点与验证

旧 30 行保留 28 个精确 key，其中 27 项 decision 保持不变；FakeClient.onDidChangeState::dispose 因新增就近注释从 no-comment 升为 commented。常量协议域与 fake 默认处理方法采用 canonical 拆分；新增 13 个 callable 各自有定义、调用或测试断言锚点。所有本批源码自引用与相关测试锚点已按当前
注释后行号登记。producer 的源码与语义台账租约仅包括 `languageClientLifecycle.ts`、本测试、此 coverage leaf 和本文件四个路径；
Root 的最终提交事务另含既有 session_restart coverage 的三处反向行号修正，以及 inventory 两条选定行的元数据更新。
这两项维护不增加本模块 46 个单元的计数。extension/browser/session、SDK 依赖和 restart helper 继续作为只读证据。

Root 的注释前 R2 adoption 证据为
`E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/extension-client-lifecycle-preedit-adoption-r1.json`
（SHA-256 `fd5f010f9bee6bd2e7e33c78a0f545bd9b0ff3f44041541cb2ba04c9687e2567`）：选定的 3 个 TS
文件构建/emit 成功，3 个原有 Node 测试文件共 9/9 通过；所有输入和生成 JS hash 与先前记录一致，
两个子进程已等待退出。该基线不代表本批注释后的验证，也不是完整扩展构建或真实 VS Code/传输覆盖；
producer 未运行构建或测试。冻结后 Root 已完成同范围严格等价性和 Node 验证，独立复核随后绑定最终快照。

本批源码只新增/调整注释和空白，没有改动程序 token、声明、签名、表达式或断言。最终源 SHA 及实际验证如下；共享 inventory、manifest、构建和 Git 事务均由 Root 管理。

## Root 最终验证与适用边界

修改后复用同一实际 checkout 与 E 根扩展输出，3 个 TS 文件实际构建 exit 0（5.640 秒），原 3 个 Node 文件 9/9 PASS（3.844 秒），包括本模块 4 例。用例编号、名称、结果同修改前基线；3 份生成 JavaScript 的 hash 和字节数相同。250 个保守输入的 7 项 raw 差异为本批 2 源和 1 叶，以及已提交 project discovery 的 3 份源注释细改和 1 叶，其余 243 项相同。运行窗口内 250 输入与 10 冻结文件稳定，两个实际子进程与 driver 均已等待退出。

实际运行后又按独审收窄两份源码的同物理行注释：Promise 仅在成功时解析为结果对象；closed 同步读取一次退休状态。最终严格 R3 对照同一 HEAD 验证 1,207 个 kind/raw token、完整语法树及 removeComments 编译后的 token/树相同，产品 TS emit 字节同上述实际输出；测试夹具 emitter 的格式差异单独报告。已开始的异步委托结果不会因后续退休被撤回，宿主实际启动还由 session 的 active 检查约束；该限制不标为生产 BUG。

- `zr_vm_language_server_extension/src/languageClientLifecycle.ts`：SHA-256 `556991a6b0d5f49748a73ba9000dade2beb6c93441d87a53c0bfeb164a12e99c`。
- `zr_vm_language_server_extension/test/languageClientLifecycle.test.js`：SHA-256 `e82895dd61c1bd05e7d17def909c7aed11b7c7675e6b2e7499afafdbe1a39e63`。

- `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/extension-client-lifecycle-final-comment-equivalence-r3.json`：SHA-256 `3cf4c6938bcef3e501d79de93e620eae9fecdcf2cc4c562f5708a1ea451a5e69`。
- `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/extension-client-lifecycle-final-node-r1-summary.json`：SHA-256 `3539fa56a37d4c2c34365cacd443e0d21dbac990d819743df40ba062591a2383`。
- `E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/extension-client-lifecycle-final-node-r1-comparison.json`：SHA-256 `eb3d784fbb7c27324f870033e07d175265309d482f090b69582d9082e47916c1`。

本批没有完整扩展构建、真实 VS Code/SDK/transport、崩溃预算或多次异步关闭时序的执行信用；只给现有实际断言和已读依赖源码各自对应的证据。独立审查与提交计划固定最终 leaf/batch 字节，本文不自含哈希。

最后的台账 reason 与跨表分类说明属于元数据校正：未改源码，实际运行窗口的输入稳定性仍按当时冻结值报告；Root caller/unit audit 另固定最终 leaf 的 hash 与唯一一次 no-comment→commented 分类变化。
