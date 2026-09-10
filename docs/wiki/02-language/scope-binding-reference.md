---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_module_call_binding.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/01-canonical-type-place-cfg-artifact/m1-type-graph.md
  - docs/plans/syntax/05-property-unified-ast/m1-unified-ast-symbol.md
  - docs/plans/syntax/10-native-ffi-module-package/m1-specifier-foundation.md
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_typed_call_binding.c
  - tests/parser/test_project_import_canonicalization.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/parser/test_syntax_reference_v1.c
doc_type: language-reference
---

# 作用域、名称绑定与可见性参考

ZR 源码里的一个名字可能代表局部值、参数、函数 overload、类型、成员、module alias、generic
parameter、native provider export 或 declaration-transform 目标。parser 只记录 identifier
和 source range；binding 才决定这个名字落在哪个 namespace、它能否在当前位置使用、它的
canonical type/owner/receiver contract 是什么。本页描述这个阶段模型和程序员可依赖的规则。

## 名称不是类型，也不是地址

```zr
let math = import("zr.math");

struct Point {
    var x: f64;
}

fn length(point: Point): f64 {
    let squared = math.sqrt(point.x * point.x);
    return squared;
}
```

这里 `math` 是局部/module value binding，`Point` 是 type reference，`length` 是 callable
symbol，`point` 与 `squared` 是 function-local value binding，`x` 在 member access 后才按
receiver type 解析。它们可能显示为相同字符串，但 compiler 不会把它们都放进一个无类别的
`name -> pointer` 表。

绑定的基本结果包含：

| 名称类别 | 绑定所需的额外事实 | 后续消费者 |
| --- | --- | --- |
| 变量/参数 | `TypeId`、slot/Place、mutability、init/loan 状态 | expression、CFG、VM frame。 |
| 函数/方法 | overload set、参数 contract、return/effect、receiver | typed call binding、AOT/VM dispatch。 |
| 类型 | module identity、definition token、generic bindings、layout capability | canonical type、constructor、FFI、reflection。 |
| module alias | canonical module key、provider domain、export table | import/member/call binding。 |
| generic parameter | owner SymbolId、ordinal、kind/variance/constraints | generic instantiation、TypeId rebind。 |
| pattern binder | source place、move/borrow intent、case/guard scope | flow/ownership cleanup。 |

因此，不能通过 source text 的字符串相等性做跨模块 type 比较、native call dispatch 或
attribute target selection。应使用已经绑定的 symbol/canonical identity。

## 作用域层次

ZR 的可见性至少需要区分下列边界：

```text
project/module
  -> type declaration / member environment
    -> function or lambda signature
      -> function/lambda body block
        -> nested block, branch, loop, catch, using guard, pattern case
```

编译器的 type environment 带 parent 指针；进入类型/函数/lambda 时创建或切换环境，离开后恢复
父环境。lambda binding 还会把它的 parameter 先登记进 lambda environment，再处理 body；
这就是 lambda 可以引用参数、同时又需要对外部捕获做单独 escape 分析的原因。

运行时 slot scope 是另一条链。`compiler_scope.c` 在进入 block 时记录 local variable 起点、
cleanup registration 和 depth；退出时标记该 scope 的局部 dead offset，并按逆序执行 cleanup。
“名字离开 source scope”与“对象立刻释放”并不是同义：shared/GC owner 可能继续存活，ref/loan
则可能因为其 source region 已结束而不能再使用。

## 声明、初始化与遮蔽

```zr
let label = "outer";
{
    let label = "inner";
    print(label);
}
print(label);
```

parser 将两个 `label` 都记录为 identifier；是否允许同名、它们在特定语言版本/作用域中的
遮蔽规则、以及 diagnostic 的严重程度由 binding/type environment 处理。编写稳定代码时建议
把下列情形视为高风险，即便当前编译配置没有把每一种都升级为错误：

- 在同一声明列表中重复同一绑定名；
- 用局部变量遮蔽 import alias 或类型名，使读者分不清 namespace；
- 在 pattern/catch/using guard 内外复用同一 resource owner 名；
- 在 closure 中捕获随后被 move/drop 的外部绑定；
- 依赖一个绑定在同一表达式的声明前就可见。

初始化分析也不是纯名称解析。`var value: T;` 得到一个 Place，但在读取前可能需要所有 CFG
路径证明已初始化；`out` parameter 的目标在 call 后能否读取，需要正常返回边上的 definite
assignment 事实。详情见[控制流、模式与清理参考](control-flow-pattern-cleanup-reference.md)。

## 函数、overload 与 callable value

```zr
fn format(value: int): string { return "int"; }
fn format(value: string): string { return value; }

let selected = format(7);
let callable = format;
let result = callable(7);
```

函数声明会登记进 function/overload namespace；call binding 再根据实际 argument、generic
argument、named argument、receiver 和 parameter passing form 选择候选。把函数赋给变量后，
compiler 可把它登记为 callable value function，使后续 `callable(...)` 仍通过函数类型 contract
验证，而不是当作动态 `object` 随意调用。

有以下差别：

| 写法 | 绑定关注点 |
| --- | --- |
| `format(7)` | overload set、argument conversion、选择结果。 |
| `object.format(7)` | receiver type/access、member lookup、可能的 virtual/protocol dispatch。 |
| `callback(7)` | value 的 function type、closure/native binding contract。 |
| `callback?.(7)` | null/optional receiver 分支与后续参数求值短路。 |
| `format<T>(value)` | generic declaration/const argument、constraint、rebound signature。 |

只有 resolved call binding 才能安全地传给 backend/native dispatcher。名称本身不能作为 ABI
契约；这也是为什么 native registry 提供 call-binding identity API，而不是仅按字符串调用。

## module alias 和 import scope

```zr
let renderer = import("engine.render");
let task = import("zr.task");

async fn draw(): task.Task<void> {
    renderer.draw();
}
```

import expression 首先是一个静态 module request；把它赋给 `renderer` 后，binding 会将 alias
作为当前 environment 中的 module-like value，并与 resolved module identity 关联。`renderer.draw`
不是编译器把 `renderer` 文本拼接成路径，而是 member/call binding 从 alias 的 export/provider
facts 中查找。

alias 的可见范围通常就是它的声明 scope。为了让 public API 不依赖局部路径拼写，跨 module
暴露的 type/function 仍应使用 resolver/canonical identity。import 具体语法和 provider domain
见[模块导入与 Native FFI](module-import-native-ffi-reference.md)。

## generic、成员与访问修饰

generic parameter 与运行时值绑定不同：

```zr
fn choose<T>(left: T, right: T): T {
    return left;
}

class Session {
    pri var secret: string;
    pub fn title(): string { return "session"; }
}
```

`T` 绑定到 generic owner symbol/ordinal，随后从实际 generic arguments 重新绑定 function
signature；不能把它用作普通局部 value。`pub`、`pri`、`pro` 在 AST 中记录 access modifier，
具体 member/module access 由 binder 根据 receiver、声明位置和 target context 判断。外部 C
插件也不应只因为一个 descriptor 名称可见就跳过 registry/provider phase 的访问和 contract
验证。

property 和 member access 还要处理 getter/setter、readonly receiver 与 ref-return contract。对
`object.member`，parser 不知道它是 field、property、method 或 native export；这种区别只会在
类型和 symbol 已绑定后确定。

## closure、借用和悬挂绑定

```zr
fn makeReader(): fn() -> int {
    let stable = 7;
    return fn(): int => stable;
}

async fn invalid(task: zr.task.Task<int>): zr.task.Task<int> {
    var owner = "value";
    var view: ref readonly string = ref owner;
    await task;
    return view.length();
}
```

第一个例子要求 closure capture layout 将可存活的 value 绑定进 callable object。第二个例子
虽然 `view` 在 text scope 中仍可见，借用 region 却不能越过 await suspension。binding 不会因为
一个 identifier 被解析成功就延长它的 owner/loan lifetime；escape/loan analysis 需要看 return、
heap store、closure capture、native callback capture、thread transfer 和 suspend edge。

同样地，pattern 中的 `move` binder 将影响源 Place 的后续可用性。名称从模式中出现不等于获得
复制品，使用后可能已使原 owner 进入 moved 状态。

## C 工具和宿主的边界

公开语义 API 提供 context 生命周期和 symbol/type 预留/登记接口：

```c
SZrSemanticContext *semantic = ZrParser_SemanticContext_New(state);
TZrSymbolId symbolId = ZrParser_Semantic_ReserveSymbolId(semantic);
TZrTypeId typeId = ZrParser_Semantic_ReserveTypeId(semantic);

/* Compiler-owned AST/environment objects are consumed while this context is live. */
ZrParser_SemanticContext_Free(semantic);
```

这类 API 面向 compiler、LSP、fixture 和高级工具，而不是 native callback 的常规调用路径。
callback 应接收已绑定的 `ZrLibCallContext`，用 descriptor/type helper 处理参数，详见
[原生 Registry 与调用绑定](../03-modules/native-registry-call-binding.md)。不要直接调用
compiler 内部的 `enter_scope`、`exit_scope` 或复用 `SZrScope` 布局：它们不是稳定 public ABI。

context、AST、type environment、candidate array 和 source string 的借用期由对应 owner 决定。
若要把诊断/索引数据跨 generation 或跨线程保存，应复制公共、稳定的投影，而不是保留内部节点
地址或临时 `TZrTypeId` 的解释结果。

## 排错顺序

1. **名字未解析**：先检查 import literal/alias scope、拼写、声明是否在当前 module/branch 中。
2. **名称解析了但调用失败**：检查 overload、generic arguments、named/ref/out marker 和 receiver access。
3. **读取绑定失败**：检查 definite assignment、move/drop、`out` 返回路径与 pattern guard。
4. **closure/async 后失败**：检查 capture escape、loan region、Task/Iterator suspend boundary。
5. **native call 找不到**：检查 module/provider identity 和 registry call-binding contract，而非仅函数名。

这种顺序能避免把一个 provider 解析问题误诊为 type error，或把一个生命周期问题误诊为
“局部变量作用域太短”。
