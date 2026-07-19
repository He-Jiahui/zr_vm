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

## 实施与安全矩阵

输出为绑定到frame/snapshot generation的DebugEvaluationContext、typed evaluation plan、effect classification、structured result/error和可失效children handle。

1. **E1 context reconstruction**：从frame/DebugMap取得module、scope、receiver、generic context和visible SymbolIds；source checksum/generation不符时拒绝。
2. **E2 parse/bind/query**：表达式复用正式parser/binder，注入只读debug bindings；不允许局部语法fork或`any`兜底。
3. **E3 effect policy**：把纯读取、property getter、allocation/call、native/owner mutation分级；hover默认pure，watch/REPL按capability显式升级。
4. **E4 result transport**：返回TypeId、display、children handle、mayThrow/mayAllocate和structured failure；resume后handle失效。
5. **E5 REPL generations**：cell module/environment有generation；owner可按明确policy持久，ref/ref struct/PoolRef不能跨cell/frame。

验证以`tests/debug/test_debug_expression_diagnostics.c`、`tests/cli/test_cli_debug_e2e.c`和LSP protocol evaluate/repl cases为入口。必须覆盖optimized-out binding、trimmed metadata、stale frame、readonly/property side effect、owner move、native failure和semicolon提交行为。

退出条件：同一expression在compiler/LSP/debug evaluator得到相同type/diagnostic；默认hover不产生target side effect；权限不足fail closed；REPL不通过持久ref逃逸绕过borrow checker。
