---
related_code:
  - tests/fixtures/projects/syntax_reference_v1/src/main.zr
  - tests/fixtures/projects/syntax_reference_v1/src/model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/object_model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/modules.zr
  - tests/fixtures/projects/syntax_reference_v1/src/async_jobs.zr
  - tests/fixtures/projects/syntax_reference_v1/src/iterators.zr
  - tests/fixtures/projects/syntax_reference_v1/src/native_ffi.zr
  - tests/fixtures/projects/syntax_reference_v1/src/ownership.zr
  - tests/fixtures/projects/syntax_reference_v1/negative/function_delimiters.zr
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
tests:
  - tests/parser/test_syntax_reference_v1.c
  - tests/library/test_project_import_resolver.c
  - tests/task/test_task_runtime.c
  - tests/iterator/test_yield_syntax.c
  - tests/ffi/test_native_extern_contract.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
doc_type: tutorial-reference
---

# 语法实用配方

本页把前几页的规则组合成可组合的示例片段。每个片段都说明 parser 关注的语法边界、预期结果和一个错误用法。`User`、`Result`、`Array`、`openFile` 等业务类型或 provider 函数是宿主/前文声明的占位名称，接入实际项目时应替换为真实定义。片段中的 `zr.task`、`zr.iteration`、`zr.ffi` 等 provider 需要在宿主/CLI 中注册；没有 provider 时，parser 仍可建立 AST，但项目解析或执行会在后续阶段失败。

## 配方一：最小入口与显式分号

```zr
module demo.main;

pub fn main(): int {
    let left: int = 40;
    let right: int = 2;
    return left + right;
}

return main();
```

**预期结果**：source parser 产生一个 module declaration、一个 public function 和一个顶层 return；若宿主把 `main` 作为 entry 执行，返回整数 `42`。每个简单 statement 都有 `;`，换行不会隐式结束 statement。

**错误用法**：

```zr
let left = 40
let right = 2;
```

第一行缺少分号，会报告 `missing_statement_semicolon` 类诊断；不要依赖自动分号插入。

## 配方二：值构造、property 与 callable

```zr
struct Point {
    var x: int;
    var y: int;
}

class Counter {
    pri var stored: int = 0;
    pub property value: int {
        pub get { return this.stored; }
        pri set { this.stored = value; }
    }
    pub fn add(delta: int): int {
        this.stored = this.stored + delta;
        return this.stored;
    }
}

fn apply(factory: fn(int) -> int, value: int): int {
    return factory(value);
}

let point: Point = init Point(1, 2);
let twice = fn(value: int): int => value * 2;
let result = apply(twice, point.x);
```

**预期结果**：`init Point(1, 2)` 选择值构造，`Counter.value` 通过 getter/setter symbol 访问 backing field，`twice` 的类型是 `fn(int) -> int`。property 不会自动生成名为 `value` 的存储字段。

**错误用法**：

```zr
fn apply(factory: fn(int) => int): int { return 0; }
```

function type 必须使用 `->`；`=>` 只用于匿名函数的 expression body。

## 配方三：optional chain 与条件表达式

```zr
fn displayName(maybeUser: User): string {
    let name = maybeUser?.profile?.name;
    return name != null ? name : "anonymous";
}

fn callIfPresent(callback: fn(int) -> int, value: int): int {
    let answer = callback?.(value);
    return answer != null ? answer : 0;
}
```

**预期结果**：当 receiver 或 callable 缺失时，optional suffix 短路，后续成员/参数表达式不求值；`?:` 只求值所选分支。`?.[` 不是合法的可选索引形式。

**错误用法**：

```zr
let item = maybeList?.[index];
```

parser 会报告 optional computed access 不支持。先保存/检查容器，再使用普通 `[]`。

## 配方四：资源作用域与 using guard

```zr
resource class FileHandle {
    pub fn read(): string { return "data"; }
}

fn load(path: string): string {
    using (let file = openFile(path)) {
        return file.read();
    }
}

fn useOptionalPlugin(): void {
    using (let plugin = import("native:engine.render")) {
        plugin.render();
    } else {
        log("plugin unavailable");
    }
}
```

**预期结果**：离开 `using` body 的正常、异常、return、break 或 continue 边都进入统一 cleanup CFG；带 `let plugin = ...` 的 guard 才允许 `else`。资源的 Drop/close 语义由 resource class/provider descriptor 定义。

**错误用法**：

```zr
using (openFile(path)) {
    readAll();
} else {
    recover();
}
```

无 guard 的 using 不能附 `else`，会得到 `using_else_without_guard`。

## 配方五：for、foreach 与 switch

```zr
fn sum(values: Array<int>): int {
    var total: int = 0;
    for (let item in values) {
        total += item;
    }
    return total;
}

fn classify(choice: Result<int>): string {
    switch (choice) {
        (Ok(value)) {
            return value > 0 ? "positive" : "zero-or-negative";
        }
        (Error(message)) {
            return message;
        }
        () {
            return "unknown";
        }
    }
}
```

**预期结果**：`for (let item in values)` 走 foreach parser，绑定器从 iterable/iterator contract 推导元素类型；switch 使用括号 case，`() { ... }` 是 default。variant payload 的 move binding 只在 pattern 位置使用 `move`。

**错误用法**：

```zr
switch (choice) {
    case Ok(value): return value;
}
```

当前语法没有 `case` 关键字，也不接受没有 body brace 的单行 case；改用 `(Ok(value)) { ... }`。

## 配方六：async/await 与 iterator

```zr
let task = import("zr.task");
let iteration = import("zr.iteration");

async fn fetch(value: int): task.Task<int> {
    return value + 1;
}

async fn run(value: int): task.Task<int> {
    let answer = await fetch(value);
    return answer;
}

fn numbers(value: int): iteration.Iterator<int> {
    yield value;
    yield value + 1;
}
```

**预期结果**：`async fn` 标记 Task-producing function，`await` 形成 suspension boundary；`yield` 把普通函数变成可恢复 iterator frame。borrowed/ref/affine guard 能否跨边界由 semantic flow 决定，不能只看 parser 是否成功。

**错误用法**：

```zr
async fn fetch(value: int): int { return value; }
```

async declaration 需要显式 `Task<T>`（或 provider 定义的兼容 task carrier）返回类型；类型不匹配会在 semantic/task effect 阶段失败。

## 配方七：native extern 与模块别名

```zr
native extern("syntax_reference_native") {
    #zr.ffi.entry("syntax_reference_render_value")#
    fn nativeValue(): i32;
}

fn checksum(): i32 {
    return nativeValue();
}
```

**预期结果**：parser 生成带 library literal、entry decorator、参数/返回 TypeRef 的 native import contract；FFI provider 注册并验证 descriptor 后，runtime 才解析动态库 symbol。manifest alias 通过 `import("#nativeRender")` 展开为 registered-native identity。

**错误用法**：

```zr
native extern(libName) {
    fn nativeValue(): i32;
}
```

extern library spec 必须是字符串 literal；动态库路径和版本选择应交给 FFI/library API。

## 配方八：从 source 到 C 宿主

```c
SZrParserState parser;
SZrAstNode *ast;

ZrParser_State_Init(&parser, state, sourceBytes, sourceLength, sourceName);
parser.structuredErrorCallback = onDiagnostic;
parser.errorUserData = userData;
ast = ZrParser_ParseWithState(&parser);
if (parser.hasError || parser.hasFatalError || ast == ZR_NULL) {
    ZrParser_Ast_Free(state, ast);
    ZrParser_State_Free(&parser);
    return ZR_FALSE;
}

/* Compile/write only after all diagnostics have been consumed. */
SZrFunction *compiled = ZrParser_Compiler_Compile(state, ast);
ZrParser_Ast_Free(state, ast);
ZrParser_State_Free(&parser);
return compiled != ZR_NULL;
```

**预期结果**：宿主在同一 state/global 中完成 parser、diagnostic、compiler 和 AST 清理；失败时不读取 partial AST。若需要 `.zro`/`.zri`，再调用 writer API，并检查其返回状态。

**错误用法**：

```c
/* 错误：把 AST 或 SZrStructuredDiagnostic 指针保存到 parser 释放之后。 */
savedAst = ast;
ZrParser_State_Free(&parser);
use(savedAst);
```

AST、诊断中的字符串/数组以及 parser source range 的 owner/lifetime 由公共头文件约定；跨窗口保存必须复制到同一 global 管理的对象。完整 C API 见 [Parser/Compiler C API](../05-interop/c-api-parser.md)。

## 运行这些配方

1. 先运行源文档校验：`python scripts/validate_wiki.py --root .`。
2. 使用 `tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp` 作为 source/project resolver 的基线。
3. parser-only 验证关注 AST 和 diagnostics；provider-dependent 配方还要启用 task/iteration/ffi/native registry。
4. 对 VM 与 AOT 同时运行时，比较结果和 diagnostics，而不是只比较生成文件大小。

fixture 的 `golden/coverage.json` 记录每个配方对应的 feature、AST 形状、semantic consumer 和状态；若源码行为变更，应先更新 fixture/test，再更新本页说明。
