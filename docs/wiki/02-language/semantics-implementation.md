---
related_code:
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/place.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/execution_control.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_place_cfg_graph.c
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_reference_loan_nll.c
  - tests/parser/test_resource_unique_drop.c
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_call_binding_runtime.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
doc_type: language-reference
---

# 语义与执行模型

**状态：`current`；跨后端优化、跨 GC domain 传递和部分 AOT 快路径标为 `experimental`。**

语法手册回答“这段源代码能否被 parser 接受”，本页回答“接受后如何变成可验证、可执行的
语义”。ZrVm 不把 AST 直接交给 VM：类型、Place、借用、控制流、清理和调用契约必须先在
语义阶段固化，VM、AOT C、AOT LLVM、LSP 和反射工具再共享这些事实。

## 三层结果

同一段源码可能在三个不同阶段得到不同结果：

| 阶段 | 产物 | 能证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| Parser | AST、token range、structured diagnostic | 括号、分号、声明形状和字面量合法 | 类型、provider、借用、布局和 ABI |
| Semantic/compiler | `TypeId`、`PlaceId`、CFG、Loan、SemIR、call contract | 类型兼容、初始化、ownership/escape、控制边和调用目标 | 外部文件存在、动态库符号仍可加载、运行时资源不会失败 |
| Runtime/backend | frame、GC root、指令、artifact、exception state | 在当前 generation、domain 和 provider contract 下执行 | 下一次 reload 或外部系统状态 |

因此 `parser.hasError == false` 不是“程序一定能运行”的承诺；反过来，出现
`hasFatalError` 时也不能把 partial AST 送入 semantic lowering。

## 总体流水线

```text
UTF-8 bytes
  -> lexer: token + literal + file range
  -> parser: AST + recovery diagnostics
  -> binding: SymbolId / module identity
  -> canonicalization: TypeId / layout capability
  -> CFG + flow: reachability / initialization / loan / cleanup
  -> SemIR: typed Place/Value/Region/Loan operations
  -> final compiler: frame / instructions / handlers / call cache
  -> VM or AOT: checked execution and provider bridge
```

每一步只应消费前一步已经验证的对象。比如后端不得根据 `share` 的 token 自己推断 owner
类型；它必须读取 SemIR 中的 ownership operation 和 canonical type。artifact writer 也不能
序列化进程地址，而要写入可重定位的 symbol、signature、layout 和 module identity。

## Canonical Type：把 TypeRef 变成身份

源码中的 `Point`、`Array<int>` 或 `ref readonly Buffer` 只是写法。canonical type interner
会将其转换为一个语义上下文内稳定的 `TZrTypeId`。节点类别由
`EZrCanonicalTypeKind` 区分，包括 primitive、nominal、generic instance、array、tuple、
union、ref、owner、readonly view、nullable 和 function。

一个 canonical 节点至少要保留下列信息：

| 信息 | 用途 |
| --- | --- |
| module identity、定义 token、名称 | 防止不同模块中同名类型相撞 |
| generic 参数（类型或常量） | 区分 `Box<int>` 与 `Box<string>` |
| passing form | 区分 value、`in`、`ref`、`ref readonly`、`out` |
| owner/ref/readonly kind | 驱动借用、Drop 和 receiver 权限 |
| capability bits | 表示 value、GC class、resource、ref-like、Send/Sync 等能力 |
| layout/GC scan kind | 生成 inline frame、字段偏移和 root map |

普通源码没有把 `T?` 作为 TypeRef 后缀语法；nullable 是 canonical 类型节点或 provider/API
结果。`wake(weak)` 返回“可为空的 shared”是语义结论，不应在 parser 中把 `?` 当作通用
类型修饰符。

常用 C API 是：

```c
TZrTypeId pointId = ZrParser_CanonicalType_FromName(context, pointName);
const SZrCanonicalTypeNode *point =
    ZrParser_CanonicalType_Find(context, pointId);
TZrBool isResource = ZrParser_CanonicalType_HasCapabilities(
    context, pointId, ZR_CANONICAL_TYPE_CAPABILITY_RESOURCE_CLASS);
```

上例只展示调用形状；`context`、名称对象和 capability mask 的真实所有权以
`canonical_type.h` 为准。`Find` 返回的节点属于 semantic context，context reset/free 后不能
继续使用。

## Place、Value 与 Loan

### Place 图

Place 表示可寻址的存储位置，而不是当前存储的值。`place.h` 将一个 Place 拆成 base 和
projection：

```text
local `account`
  -> field `.balance`
  -> index `[slot]`
  -> dereference `*ref`
```

base 可以是 local、parameter、`this`、static、temporary、return slot 或 external handle；
projection 可以是 field、index、union variant、tuple element 或 dereference。两个 Place 的
overlap 结果是 equal、disjoint、overlap 或 unknown。只有可写且不冲突的 Place 才能成为
assignment/ref/out 的目标。

```zr
var account: Account = init Account();
account.balance = 10;       // field/property Place
var slot = 0;
account.entries[slot] += 1;  // index projection
ref view = ref account.balanceStorage;
```

property get/set 可能最终映射到 accessor callable；只有 accessor 明确提供 ref contract 时，
它才能作为可写 Place。parser 不会仅凭 `.` 字符判断这一点。

### Value 与 Loan

| 语义对象 | 典型来源 | 生命周期结论 |
| --- | --- | --- |
| Value | literal、函数返回、copy | 可以放入目标 Place；是否复制由类型/contract 决定 |
| shared loan | `in`、`ref readonly`、共享读取 | 多个读取可重叠，不能通过它写入 |
| mutable loan | `ref`、可写 property ref | 同一时间通常只能有一个活动写借用 |
| move | `own`、`move` pattern、owner 传递 | 源 Place 被标记 moved，后续读取必须拒绝 |
| escape fact | return、closure capture、heap store、await/native capture | 源 region 必须覆盖目标 escape bound |

两阶段 mutable loan 可能先 reserve、后 activate；NLL（non-lexical lifetime）会在最后一次
使用后结束 loan，而不是盲目延长到整个 block。借用分析失败属于 semantic diagnostic，
不是 parser 语法错误。

## CFG、异常和清理

`ZrParser_Cfg_Build` 把 AST 切成带类型的 block 和 edge。除了普通/真假分支，还明确区分
switch case/default、exception、cleanup、return、suspend/resume 等边：

```text
entry
  -> body --return/throw/break--> pending-control
  -> cleanup scope(s)
  -> finally/outer handler
  -> target or exit
```

`SZrState.pendingControl` 保存控制种类、目标 `SZrCallInfo`、目标 instruction offset、
return/break/continue value slot 和可选 value。这样 `return`、`throw`、`break`、`continue`
不需要各自实现一套展开逻辑。

`using`、`finally` 和 resource Drop 都注册 cleanup。离开作用域时执行顺序是内层到外层；
cleanup 自身抛错时，运行时保留原 pending control，并把新异常状态交给外层 handler。故意
在每个 `return` 前手写 `drop` 不能替代 cleanup CFG，因为它遗漏异常、`break`、`continue`
和 AOT 异常边。

可从 C 侧查看 CFG：

```c
SZrParserCfg cfg;
ZrParser_Cfg_Init(state, &cfg);
if (ZrParser_Cfg_Build(state, &cfg, ast)) {
    (void)ZrParser_Cfg_EmitReachabilityFacts(semanticContext, &cfg);
}
ZrParser_Cfg_Free(state, &cfg);
```

`SZrParserCfg` 中的 block/edge 指针借用 `cfg`；释放后不要把它们缓存到 LSP 或调试器。

## SemIR：后端共享的语义事实

SemIR 不再携带“可能是什么”的语法猜测，而是显式记录 typed operation、source range 和
effect。常见 opcode 与源码的对应关系如下：

| 源码/事实 | SemIR 操作 | 后端责任 |
| --- | --- | --- |
| `let x = 1` | `CONSTANT` + `PLACE_BASE` + `INITIALIZE` | 分配 slot 并保持初始化 bitmap |
| `x.field` | `PLACE_PROJECT` / `LOAD` | 使用 canonical layout 的字段偏移 |
| `ref x` | `BORROW_SHARED` 或 `BORROW_MUT` | 维护 region、loan 和逃逸检查 |
| `own T(...)` | `OWN_CONSTRUCT` | 建立 resource owner 和 Drop registration |
| `a = b` | `LOAD` + `STORE` 或 `MOVE` | 按 copy/move/assignment contract 操作 |
| `f(x)` | `CALL_TYPED`/`CALL_VIRTUAL`/`CALL_DYNAMIC` | 验证 call binding 后再 dispatch |
| `using`/`finally` | `SCOPE_ENTER` + `CLEANUP` | 正常和异常边都执行清理 |
| `yield`/`await` | `YIELD_*` 或 suspend edge | 保存 frame/root/loan 状态并可恢复 |

`ZrParser_SemanticIrFunction_Init/Free` 管理 IR 容器；在送入 backend 前应完成 IR invariant
校验。后端可以把已证明安全的指令 quicken 成快路径，但 guard miss 必须回到 checked
path，不能改变原 SemIR 的 ownership、异常或布局含义。

如需查看 CFG block/edge、全部 SemIR opcode、Place/Value/Loan/Region 数据结构、flow result
和 C 查询 API，请参阅[Semantic IR、CFG 与数据流事实参考](../08-compiler-semantic-ir-facts-reference.md)。

## 所有权、资源和 GC 的边界

所有权世界与 GC 世界是两套生命周期：

```zr
resource class FileHandle { pub fn close(): void { } }

let unique: Unique<FileHandle> = own FileHandle();
let shared: Shared<FileHandle> = share(unique);
let weak: Weak<FileHandle> = degrade(shared);
let live = wake(weak);
drop(shared);
```

- `own` 创建唯一 owner；move 后源 Place 不可再次读取。
- `share`/`degrade`/`wake` 是保留 intrinsic，具体原子性和 domain 规则由 runtime/provider
  实现。
- `intoGc` 通过显式 bridge 把 resource 置于 GC 管理的 box；它不是隐式转换。
- `drop` 消费 owner 并立即进入 resource Drop contract；GC finalization 不等价于 Drop。

GC 侧的 class、字符串、数组、闭包和 descriptor 必须由 root、barrier 或 pin 保持可达：

| 场景 | 必须做的事 |
| --- | --- |
| 写入 managed 字段/数组 | 使用对象 helper 或 `ZrCore_Gc_WriteBarrier` |
| native callback 暂存对象/value | `ZrCore_Gc_NativeCallPinObject/Value`，完成后 unpin |
| AOT frame 中保存 GC 值 | push `ZrCore_Gc_AotRootFrame`，返回时 pop |
| 跨 GC domain 传递 | 使用 bridge/transport 和 domain identity，不能裸传地址 |

这些规则解释了为什么 C API 返回的 `SZrObject*`、`SZrString*` 或 `SZrTypeValue*` 不能脱离
state/global 长期保存。

## await、yield 与可恢复 frame

`await` 和 `yield` 都会把普通直线执行切成 suspension/resume 边，但 carrier 不同：

| 结构 | carrier | frame 必须保存 |
| --- | --- | --- |
| `async fn`/`await` | `Task<T>`（由 provider 定义） | program counter、局部/closure、异常和 GC roots |
| `yield` | `Iterator<T>` 或异步 iterator | program counter、yield value、局部、终止状态和 cleanup |

semantic flow 会拒绝把不能跨 suspension 的 `ref struct`、活动 mutable loan、lock guard 或
未完成 owner 放入 frame。`async fn` 的返回 TypeRef 仍须显式写 Task carrier；parser 接受
`async fn f(): int` 不代表 task effect 合法。

## 调用契约与 generation

每个静态或动态 call site 最终形成 `SZrCallBindingContract`，至少包括 target/signature
token、参数 passing form、module signature hash、layout version/hash、operation、dispatch
slot 和 relocation kind。运行时 witness（VM function、native callback 或 AOT thunk）属于当前
module/function generation，不应写进 `.zri`/`.zro`。

模块 reload 或函数图替换时 generation 递增；旧 witness 进入 stale 状态，下一次调用必须
重新验证。typed callable value 没有固定 target，但仍必须按结构签名重新检查，不能复用上次
调用的 witness。

## 从源码到指令的追踪示例

以 `using` 中的一次调用为例：

```zr
fn read(value: in int): int { return value + 1; }

fn run(): int {
    using (let file = openFile("data.txt")) {
        return read(file.size());
    }
}
```

实现阶段会产生类似下列事实（名称是说明用的，不是稳定序列化格式）：

1. lexer 产生 `fn`、`in`、`using`、字符串和显式分号，并为每个 token 保留 range。
2. parser 建立两个 function declaration、using AST、member call AST 和 `return` AST。
3. binding 为 `file` 建立 local `PlaceId`，为 `openFile`/`size`/`read` 解析 symbol；`read` 的
   参数 contract 记录 `PASSING_IN`。
4. canonicalization 检查 `file` 的 resource capability 和 `read` 的 `int` 参数/返回
   `TypeId`；若 `file.size()` 不是 int，这一步产生 type mismatch diagnostic。
5. CFG 建立 body、return edge 和 cleanup edge；即使 `return` 发生，也必须先关闭 `file`。
6. SemIR 发出 resource construction、typed member call、borrow/load、typed call、return 和
   cleanup 操作。
7. compiler 生成 frame layout、exception handler、Drop registration 和 call-binding row；VM
   或 AOT 只执行这些已验证的事实。

## C 宿主的最小边界

宿主应把“解析失败”“语义失败”和“执行失败”分开处理，并在同一个 state/global owner
下释放对象：

```c
SZrParserState parser;
SZrAstNode *ast;
ZrParser_State_Init(&parser, state, sourceBytes, sourceLength, sourceName);
parser.structuredErrorCallback = onDiagnostic;
parser.errorUserData = userData;

ast = ZrParser_ParseWithState(&parser);
if (ast == ZR_NULL || parser.hasError || parser.hasFatalError) {
    ZrParser_Ast_Free(state, ast);
    ZrParser_State_Free(&parser);
    return ZR_FALSE;
}

SZrFunction *function = ZrParser_Compiler_Compile(state, ast);
ZrParser_Ast_Free(state, ast);
ZrParser_State_Free(&parser);
if (function == ZR_NULL) {
    /* compiler diagnostics are still owned by the current state/context */
    return ZR_FALSE;
}
return ZR_TRUE;
```

需要构建/执行项目时，在 parser 前创建 `SZrGlobalState`/`SZrState`，再注册 library/native
provider；需要生成 `.zri/.zro/.zrs` 时，继续调用 writer 并检查 `EZrArtifactStatus`。诊断
回调中的 `SZrStructuredDiagnostic` 只在回调窗口借用；跨线程或跨 generation 请使用
`ZrParser_StructuredDiagnostic_Copy`。

## 排查顺序

遇到“能解析但不能运行”时，按下列顺序收集证据：

1. 确认 parser `hasError/hasFatalError`、diagnostic code 和 source range。
2. 查询 canonical `TypeId`、passing form、capability 和 layout hash。
3. 查看 CFG 是否存在 unreachable、未初始化、异常或 cleanup edge。
4. 查看 Loan/escape diagnostic，尤其是 return、closure、await、yield 和 native capture。
5. 查看 call-binding contract 的 module/signature/layout/generation 是否匹配。
6. 最后检查 provider、动态库、GC domain、native status 或外部资源；不要先在 backend 中
   添加绕过检查的特殊分支。

相关的逐条语法见[声明与类型构造](declarations.md)、[表达式与运算符](expressions.md)和
[控制流与资源清理](control-flow.md)；可执行宿主顺序见[Parser/Compiler C API](../05-interop/c-api-parser.md)、
[Core C API](../05-interop/c-api-core.md)和[嵌入式宿主生命周期](../05-interop/embedding-lifecycle.md)。
