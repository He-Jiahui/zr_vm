# LSP stdio JSON string ingress

本修复以已合并的 `9ec28ea5` 为基线。先提交真实 stdio 回归用例并开启 Draft，再修改生产入口；初始提交只通过 JavaScript 语法与差异检查，尚未执行服务端或证明 Native RED/GREEN。

## 问题边界

帧的 `Content-Length` 是原始 JSON 的字节长度，不是 cJSON 字符串解码后的长度。当前 cJSON 节点只暴露以零结尾的字符串。合法 JSON 字符串中的活动 `\u0000` 解码后可能使后续 `strlen`、比较及复制只消费前缀。

这影响文档正文、文档 URI 和请求身份：不得把被截断的正文提交为新版本，不得把无法保真的 URI 当成已有文档，也不得回显或登记被截短的请求 ID。物理零字节与错误十六进制 Unicode escape 是不同的非法 JSON 输入；不能假定当前第三方解析器一定拒绝它们。

## 本轮回归契约

- 含解码零字符的 `didOpen` 不建立前缀 overlay。同一 URI 和初始版本随后发送的合法打开必须真正生效。
- 可信 URI 的无效 `didChange` 保留原内容、客户端版本及诊断输入代际，沿用现有失同步规则：文档查询返回 `ContentModified`，合法的单项全文替换恢复同步。保留旧事实不代表允许继续返回旧的 Definition 响应。
- 批次编辑中的一个无效 replacement 使整批不提交。原有的两个声明必须仍可作为工作区状态见证，不能只断言隐藏尾部没有出现。
- 不可表示的文档 URI 返回 `InvalidParams`；通知不得修改、关闭或使其字符串前缀所对应的真实文档失同步。
- 不可保真的字符串请求 ID 返回 `InvalidRequest` 与 null ID，不登记或回显其前缀，也不执行原请求。普通字符串、数字、显式 null 和通知身份的原有规则保留。
- 非法 JSON 中的物理零与错误 Unicode hex 返回 `ParseError`；下一合法帧仍能被处理。
- 字面反斜杠加 `u0000`、合法代理对及多字节字符继续被接受。新增正向用例分别真实协商 UTF-16 和 UTF-8，并以非 BMP 字符之后的增量位置检查编码差异。

新增用例复用 [document-sync suite](../../tests/language_server/stdio_document_sync_conformance.js)、[protocol suite](../../tests/language_server/stdio_protocol_conformance.js) 和现有协议客户端。每个场景只向该套件独占启动的本机测试进程 stdin 写入帧；文档 URI 属于新建的独占临时目录。通知后的正常响应、精确符号及快照状态才是见证，通知无响应不能单独证明拒绝。用例完成时正常 shutdown/exit，并保留 finally 清理。

## 计划的实现范围

在原始 body 仍有效时进行有界校验，必须早于请求预留、取消处理、退出控制及文档分派。需要按真实 JSON string token 和 escape 边界区分活动零、字面反斜杠以及合法代理对，并把相关原始 token 精确归属到已解析字段。拒绝路径使用现有队列和错误发送器，不由读线程抢写 stdout。

不修改第三方 cJSON 的公开布局，不用原始帧长度越界扫描 `valuestring`，不修改 URI 等价规则或 Native 快照算法。不将本修复扩大为所有 JSON / UTF-8 输入的重写；尤其保留现有非法 UTF-8 文档变更的失同步与全文恢复协议。

未知 opaque 配置字段、并发取消时序以及其他尚未审查的字符串消费者不在初始回归覆盖中，不将这些用例描述成整个协议已无边界问题。

## 后续验收

先在匹配基线的 stdio 可执行文件和 DLL 上记录旧实现的具体状态断言失败，再实现入口修复。随后运行完整 document-sync 和 protocol CTest，以及文档关闭、编码和生命周期相关回归。初始 Draft 的语法检查不替代任何运行结果；回归通过和精确构建树确认后才转 ready 与合并。
