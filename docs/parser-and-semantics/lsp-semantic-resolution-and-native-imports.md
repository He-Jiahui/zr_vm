---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c
plan_sources:
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
doc_type: module-detail
---

# LSP Semantic Resolution And Native Imports

## 范围

这份文档说明 language server 里最近补齐的三条关键语义链路：

1. `this` / `super` / compile-time / `%test` / lambda 等局部符号，必须按真实作用域命中。
2. hover / definition / references 不能再被宽范围声明误导，必须优先命中最具体的引用范围。
3. `%import("zr.math")` 这类导入必须在语义分析阶段就把 native/binary/source module metadata 预热进 parser/type inference，后续 LSP 才能正确解析 `$math.Vector3(...).y` 这类值类型构造链。

## 隐式接收者符号

`semantic_analyzer_symbols.c` 里的 `collect_function_like_scope(...)` 会为实例方法、构造函数、struct 方法建立隐式运行时符号：

- `this`
- `super`

修复前，这两个隐式符号复用了所属方法 AST 节点。结果是 hover 命中 `this` 时，LSP 会把它当成“方法符号”，显示方法签名而不是接收者类型。

当前行为改成：

- `this` / `super` 仍然是普通变量符号，参与 completion / hover / references。
- 但它们不再绑定到方法 AST 节点，因此 hover 会回落到变量类型展示。
- `this` 的 hover 现在展示当前 owner type，例如 `Derived`。
- `super` 的类型来自继承链上的 base type，后续 definition/reference 继续通过真实符号关系处理。

这条修复直接覆盖了类内 receiver 场景，不再需要 hover markdown 反推“当前是不是 method”。

## `super(...)` 构造函数签名帮助

`lsp_signature_help.c` 现在把 class meta function 里的 `super(...)` 识别成独立调用上下文，而不是要求它先降级成普通 `FunctionCall` AST。这个补口专门解决派生类构造函数里的场景：

```zr
class BossHero: BaseHero {
    pub @constructor(seed: int) super(seed) {
    }
}
```

当前行为是：

- signature help 会先识别当前位置是否落在 `super(...)` 参数区间。
- 若命中，LSP 直接解析当前 owner class 的 direct base type。
- 基类构造函数元方法优先走 compiler/type inference 里的结构化 member info。
- 如果当前 analyzer 尚未持有对应 prototype member，则回退到源码 AST 里的 `@constructor` 声明，按基类泛型实参做参数类型专门化。
- 最终 signature label 与 parameter list 统一走结构化 builder，所以 `super(seed)` 会展示 base constructor 的参数名和类型，而不是派生类构造函数自己的参数列表。
- `lsp_interface.c` 的 definition 路径也复用了同一套 `super(...)` 上下文识别；当光标落在 `super` 关键字或参数区时，goto definition 会直接跳到基类 `@constructor` 声明，而不是停在派生类构造函数作用域里的隐式 `super` 变量上。

这让 `super(...)` 不再是 signature help 的盲区，也避免了“必须先有完整 prototype 编译结果才能提示”的额外耦合。

## 最窄引用优先

`reference_tracker.c` 的 `FindReferenceAt(...)` 以前采用“第一个包含当前位置的引用即返回”。这个策略在有宽范围定义引用时会产生系统性误判，例如：

- `%test("scope")` 的声明范围覆盖整个 test body
- 更具体的局部变量 / compile-time 变量 usage 引用虽然也存在，但因为排在后面，永远命不中

当前行为改成：

- 先收集所有包含当前位置的引用
- 再按“范围越窄越优先”选择最佳候选
- 若范围相同，则优先非 definition 引用

这使得以下行为回到一致状态：

- compile-time 变量 hover 不再被 `%test` 声明覆盖
- 局部变量 / lambda capture / class 内 receiver 的引用命中更稳定
- LSP definition / hover / references 的命中逻辑更接近真实语义事实，而不是注册顺序

## Native Import Metadata 预热

`$math.Vector3(4.0, 5.0, 6.0).y` 之前在 parser 单测里可推断，但在 LSP 文档分析里失败。根因不是语法不支持，而是 semantic analyzer 在建立

```zr
var math = %import("zr.math");
```

这个变量的类型时，只写入了模块名字符串，没有触发 parser 的 import metadata 加载流程。

当前行为改成：

- `semantic_analyzer_symbols.c` 里的 `infer_symbol_expression_type(...)` 在遇到 `ZR_AST_IMPORT_EXPRESSION` 时，优先调用 `ZrParser_ExpressionType_Infer(...)`
- parser 的 import inference 会进入 `ensure_import_module_compile_info(...)`
- native/source/binary module 的 prototype/type descriptor 会被注册到 compilerState 的 type prototype 集合
- 随后 `lsp_interface_support.c` 对 receiver 前缀做 AST 推断时，就能把 `$math.Vector3(...)` 识别成真实 `Vector3` value type

结果是：

- `$math.Vector3(...).` completion 可以列出 `x/y/z`
- `$math.Vector3(...).y` hover 可以展示 `field y: float` 和 `Receiver: Vector3`
- 这条能力直接复用 parser/type inference/native descriptor 的结构化元信息，不再靠 LSP 特判库名

## 相关回归

本轮回归覆盖了下面几类样例：

- 类实例方法中的 `this` / `super` / locals hover 与 completion
- 派生类构造函数里的 `super(...)` base constructor signature help
- 派生类构造函数里的 `super(...)` goto definition 到 base constructor
- `%compileTime` 变量与函数作用域
- `%test("scope")` 的局部作用域
- typed lambda 的局部参数和 capture
- directive `%...` 与 meta method `@...` completion
- native value constructor `$math.Vector3(...).y`
- 类成员 definition / references / comment hover

对应证据见：

- `tests/language_server/test_semantic_analyzer.c`
- `tests/language_server/test_lsp_interface.c`
- `tests/language_server/test_lsp_project_features.c`

## 当前已知限制

- Windows MSVC 的 LSP 测试可执行体仍然存在独立的 `0xC0000005` 退出问题，这不是本轮 Linux/WSDL 语义修复引入的新回归。
- `zr_vm_language_server_lsp_interface_test` 在 WSL 通过时仍会打印两条 `Construct target must resolve to a registered prototype` 编译日志；当前没有导致目标测试失败，但说明 constructor prototype 解析路径仍有额外清理空间。
