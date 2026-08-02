# LSP 04：Debug Expression 与 REPL

## 共享编译路径

debug evaluate、watch、conditional breakpoint和REPL expression使用同一parser/binder/Canonical TypeRef/Place query，不建立“宽松脚本表达式”第二套语义。输入额外带：

```text
DebugEvaluationContext {
  moduleIdentity;
  function/scope/block;
  frameGeneration;
  visibleSymbolBindings;
  this/receiver;
  genericContext;
}
```

## 安全级别

- hover/preview默认只允许side-effect-free query。
- watch可按debugger policy允许property getter，但必须标明mayCall/mayAllocate/mayThrow。
- explicit evaluate允许普通call；仍受borrow/readonly/ref struct/native capability规则约束。
- time-travel/snapshot context拒绝会改变当前live runtime的操作。

## 结果

返回Canonical TypeId、display value、expandable reference、lifetime/generation和structured error。ref/owner/pool handle显示有效性但不暴露可伪造raw pointer。ModuleNamespace可展开exports并跳转到source/metadata virtual doc。

## REPL

REPL cell具有显式module/environment generation。每个cell末尾simple statement仍需`;`；交互前端可在提交操作时插入受控terminator，但语言parser本身不启用ASI。跨cell symbol、owner和ref-like value的持久化规则必须显式，ref/ref struct默认不能逃出cell/frame。

## 对接

LSP负责编辑器请求与expression diagnostics；`zr.debug`/DAP负责暂停态、frame、memory/value transport。双方通过DebugMap、SymbolId、TypeId和ModuleIdentity连接。

E2b6b 已发布 generation-checked paused-frame closure-capture resolver：它只从当前
VM closure 与 E2b6a typed capture identity读取 capture index、TypeRef、SymbolId、TypeId
和 declaration range，并在 resolve 时复验同一 frame generation、PC 和完整 identity。
E2b6c 已在正式 TypeEnvironment 与 `SZrSemanticReferenceFact` 发布
`CLOSURE_CAPTURE` origin、capture index、generation token、SymbolId、TypeId 和
declaration range。capture index 不复用 PlaceId，首个 capture 的 index `0` 有效；
缺失、stale、trimmed、incomplete 或 duplicate identity 均 fail closed。legacy capture
name 只在 core 已按 exact index 验证身份后作为 parser surface key，不能用于恢复 identity。
E2b6d 已让 formal Debug consumer 只消费该 reference fact：它重验source、capture index、
token、SymbolId、TypeId和whole declaration range后调用generation-checked resolver。任何
不匹配都使formal execution unavailable；LSP/DAP consumer 不得按 capture name、slot、AST
或文本回退。
E3a 让 DAP `evaluate.context` 成为 formal evaluation capability 的唯一入口：`hover`、缺失
或未知 context 只授予 pure query，`watch` 仅额外授予 property getter，`repl` 显式授予
getter/allocation/call/native-call。所有 context 都拒绝 mutation 和 owner mutation；请求必须
经 `ZrDebug_EvaluateWithCapabilities`，不能以 context、member name、AST 或文本选择兼容执行器。
E3b 将 conditional breakpoint 收敛到同一 formal policy，但其允许效果始终为空且关闭
legacy compatibility。非空条件只能进行纯读取；getter、allocation、call、native call、mutation
和 owner mutation 都 fail closed。空条件仍表示无条件命中；失败条件不命中并通过既有断点错误
输出路径报告，不能按表达式文本、member name 或 AST 选择替代执行器。
E3c 对每个 logpoint `{expression}` 应用相同的 zero-capability formal policy：纯表达式格式化为
console 插值值，任何需要 getter/allocation/call/native-call/mutation/owner mutation 的表达式保留既有
`<error:...>` 插值结果。模板格式器禁用legacy compatibility，断点分发层只发送完成格式化的输出。
E4a 将 formal evaluate 的 canonical result transport 显式化：结果和协议响应携带当前 paused
`stateId`，并只在 exact root-expression semantic query 返回有效TypeId时发布
`hasCanonicalType`与`canonicalTypeId`。变量handle与结果共享state generation；legacy compatibility
执行不伪造TypeId，保持identity unavailable。

## 实施与安全矩阵

输出为绑定到frame/snapshot generation的DebugEvaluationContext、typed evaluation plan、effect classification、structured result/error和可失效children handle。

1. **E1 context reconstruction**：从frame/DebugMap取得module、scope、receiver、generic context和visible SymbolIds；source checksum/generation不符时拒绝。
2. **E2 parse/bind/query**：表达式复用正式parser/binder，注入只读debug bindings；不允许局部语法fork或`any`兜底。
3. **E3 effect policy**：把纯读取、property getter、allocation/call、native/owner mutation分级；hover默认pure，watch/REPL按capability显式升级。
4. **E4 result transport**：返回TypeId、display、children handle、mayThrow/mayAllocate和structured failure；resume后handle失效。
5. **E5 REPL generations**：cell module/environment有generation；owner可按明确policy持久，ref/ref struct/PoolRef不能跨cell/frame。

验证以`tests/debug/test_debug_expression_diagnostics.c`、`tests/cli/test_cli_debug_e2e.c`和LSP protocol evaluate/repl cases为入口。必须覆盖optimized-out binding、trimmed metadata、stale frame、readonly/property side effect、owner move、native failure和semicolon提交行为。

退出条件：同一expression在compiler/LSP/debug evaluator得到相同type/diagnostic；默认hover不产生target side effect；权限不足fail closed；REPL不通过持久ref逃逸绕过borrow checker。
