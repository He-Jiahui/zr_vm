---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - docs/zr_language_specification.md
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_switch_patterns.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_intrinsic.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_core/src/zr_vm_core/ownership.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求详细介绍语法规则和用例
  - docs/plans/syntax/README.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_parser_recovery_ownership.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_ownership_intrinsic_member_separation.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/parser/test_numeric_foreach_cardinality_dataflow.c
doc_type: language-recipes
---

# 模式、所有权与资源清理配方

本页把容易在真实项目中混淆的三件事拆开：模式声明“绑定哪些名字”，ownership qualifier
决定“谁负责值”，`using`/`await` 决定“何时可以离开当前作用域”。parser 先建立 pattern
和 type AST；compiler 再做 shape、definite assignment、loan、drop 和 frame-safety 检查。
因此“能解析”不等于“可以移动或跨 await”。

## 1. 模式的语法形状

```ebnf
pattern          ::= identifier
                   | "{" pattern-entry {("," | ";") pattern-entry} "}"
                   | "[" [identifier {"," identifier}] "]"
pattern-entry    ::= identifier [":" identifier]
variable         ::= [access] ("var" | "let" | "const") pattern
                     [":" TypeRef] ["=" expression] ";"
foreach          ::= "for" "(" ("var" | "let") pattern [":" TypeRef]
                     "in" expression ")" block
```

当前 parser 的对象 pattern key 可以和局部名相同或不同，数组 pattern 按位置绑定：

```zr
let {host, port} = endpoint;
let {remote: localHost, port: localPort} = endpoint;
let [first, second] = pair;

for (let {name, size} in files) {
    print(name, size);
}
```

pattern 节点记录 source range、entry 顺序和 optional type/value；它不是运行时 object literal。
右侧值必须在绑定前可读取，重复局部名、未知 key、数组 arity 和不可解构类型由 compiler
报告，而不是把缺失字段默认为 null。

## 2. `var`、`let`、`const` 和初始化

当前 AST 用 `isConst` 表示不可重新赋值；`let` 是推荐的不可变局部写法，`const` 保留为
兼容输入。下面三种初始化路径的后续规则不同：

```zr
var inferred = makeValue();       // 从 initializer 推断
let count: int = 0;               // 显式类型并初始化
var handle: FileStream;           // 仅声明，等待后续 definite assignment
```

`var handle` 只有在 flow 能证明每个读取路径已写入时才可用。`let`/`const` 不能在声明后
再次赋值；构造函数中的 `const` 字段可由 compiler 追踪一次性初始化。`var const x` 不
是合法的“var + 后缀 const”语法。赋值右侧若包含 `await`、throw 或移交 ownership，flow
会把初始化拆成不同 basic block，确保失败路径不释放未初始化 slot。

## 3. ownership qualifier 和 intrinsic

类型引用可携带 `owner`、`unique`、`shared`、`weak`、`borrowed` 等语义（具体组合受
canonical type 约束）。ownership intrinsic 用函数形状表达，不是普通成员调用：

```zr
let uniqueFile = own File("data.txt");
let sharedFile = share(uniqueFile);
let weakFile = degrade(sharedFile);
let borrowedFile = wake(weakFile);
let gcOwner = intoGc(uniqueFile);
drop(gcOwner);
```

其意图是：

| 形式 | 语义 | 失败条件 |
| --- | --- | --- |
| `own(expr)` | 建立唯一 owner | expr 不是可拥有 resource/value |
| `share(owner)` | 增加共享控制块引用 | owner 已移动或不支持 shared |
| `degrade(shared)` | 产生 weak handle，不延长寿命 | 非 shared owner |
| `wake(weak)` | 尝试升级为可用借用/共享视图 | 对象已回收，结果可能是 null |
| `intoGc(owner)` | 将可管理对象交给 GC 生命周期 | native-only/raw pointer |
| `drop(owner)` | 显式结束当前 ownership | 仍有活动 borrow/loan |

这些 intrinsic 会生成 ownership SemIR fact 和 drop edge；不要把它们改写成任意函数名，
否则 backend 无法保留 move/borrow 语义。`drop` 后继续读取原名字是编译错误，弱引用升级
失败必须显式检查 null。

## 4. `using` 的三种形式

### 4.1 资源绑定块

```zr
using (let stream = init import("zr.system.fs").File("out.txt").open("w")) {
    stream.writeText("hello");
}
```

compiler 为 block 建立 cleanup registration；正常落出、`return`、`break`、`continue`、
`throw` 和 `yield` 都先走同一个 close/drop edge。`close` 是否幂等由 resource descriptor
定义，但重复 close 不应成为依赖；推荐让 `using` 独占资源的最终释放职责。

### 4.2 expression guard

```zr
using (lockGuard = mutex.lock()) {
    updateSharedState(lockGuard);
}
```

guard expression 必须产生带 cleanup contract 的值。若没有 block，当前 parser 只接受对象
或数组 pattern 的 guard 形式，便于绑定声明范围；裸 `using (open())` 无 block 会被拒绝。

### 4.3 多资源和错误路径

```zr
using (let input = openInput(); let output = openOutput()) {
    copy(input, output);
}
```

资源按声明的逆序释放；如果第二个 initializer 失败，第一个已成功资源立即清理。cleanup
本身抛错时，runtime 按 exception policy 保留原始异常并把 cleanup failure 作为 related
information；不要在 finally 中吞掉两者。

## 5. 函数参数 passing mode

```zr
fn inspect(value: in Record): int { return value.id; }
fn replace(value: out Record): void { value = Record(); }
fn mutate(value: ref Record): void { value.id += 1; }
fn consume(value: scoped Resource): void { drop(value); }
```

| mode | 调用者要求 | callee 能做什么 | 是否可跨 await |
| --- | --- | --- | --- |
| 默认 value | 提供可复制/移动值 | 读取或取得自己的 ownership | 取决于结果类型 |
| `in` | 提供可读 place | 只读借用 | 不能保留活动 borrow |
| `out` | 提供可写 place | 初始化并写回 | 不能带未完成 loan |
| `ref` | 提供兼容可写 place | 读写原 place | 活动 ref 禁止跨 await |
| `scoped` | 提供作用域内 resource/view | 只在当前 scope 使用 | 必须在 scope 前结束 |

passing mode 同时写入 AST `SZrParameter`、canonical signature、native descriptor 和
call-binding hash。默认值可用于 value 参数；`out`/`ref` 与默认值组合会在 parameter
validation 阶段失败。调用端不能用临时 literal 充当 `ref`/`out` place。

## 6. 类型推断和模式 shape

推断按“initializer -> pattern shape -> declared TypeRef -> generic constraint”顺序进行：

```zr
let {x, y} = point;             // 从 point 的字段推断 x/y
let [head, tail]: [int, int] = pair;
let values = init zr.container.Array<int>();
for (let value: int in values) {
    use(value);
}
```

若显式类型与 shape 冲突，报告 pattern mismatch；若类型是 open generic 或 unknown，
compiler 不能用字段名猜测其 shape。对象 pattern 的 `remote: local` 只改变局部绑定名，
不改变 source key；数组 pattern 不会自动收集剩余元素，超出声明的元素需由容器 API 另行
访问。

## 7. union 和 switch pattern

union 变体在 `switch` 中通过 pattern 选择：

```zr
union Result<T> {
    Ok(T);
    Error(string);
}

fn unwrap<T>(result: Result<T>): T {
    switch (result) {
        (Ok(value)) { return value; }
        (Error(message)) { throw message; }
    }
}
```

parser 为每个 case 保存 pattern AST；semantic phase 检查 variant 名称、payload shape 和
穷尽性。空 pattern `()` 可作为 fallback；它不能访问不存在的 payload。不同 variant 的
局部绑定只在各自 case block 生效，离开 case 后不应读取。缺失 case、重复 variant 或
payload 类型不匹配属于 semantic error，不是 parser error。

## 8. async、frame safety 和所有权

```zr
async fn readFirst(path: string): zr.task.Task<string> {
    let file = init File(path);
    using (let stream = file.open("r")) {
        let line = await stream.readLineAsync();
        return line;
    }
}
```

`async fn` 必须显式返回 `zr.task.Task<T>`。compiler 在每个 `await` 建立 suspend/resume
edge，并检查：

- 活动 `ref`、`in` borrow、`scoped` guard 已结束或可安全 frame 化；
- resource owner 被放入挂起 frame，恢复时仍属于同一 state/domain；
- native pointer、open socket、thread-affine lock 未被隐式复制；
- finally/using cleanup 在正常返回、fault 和 cancellation 上都有路径。

如果值不能 frame 化，应在 await 前复制成独立 owned value，或把异步操作移到不持有该 loan
的 helper。不要用“把指针存进全局”逃过检查；runtime 的 generation/GC 校验仍会拒绝 stale
访问。

## 9. closure、move 和捕获

```zr
fn makeCounter(): fn() -> int {
    var total = 0;
    return () => {
        total += 1;
        return total;
    };
}
```

lambda parser 产生 child function 和 capture reference；compiler 的
`ExternalVariables_Analyze` 决定 capture 是 value、borrow 还是 ownership move。返回闭包
时，借用局部不能逃逸；可变 capture 需要唯一/受控共享语义。闭包中的 resource capture
会延长 owner 到闭包释放，但不能跨 isolated thread transfer，除非类型满足 Send/Sync。

## 10. 综合配方：安全文件处理

```zr
fn copyText(sourcePath: string, targetPath: string): bool {
    using (let source = init File(sourcePath).open("r")) {
        using (let target = init File(targetPath).open("w")) {
            while (true) {
                let chunk = source.readText(4096);
                if (chunk == null) { break; }
                target.writeText(chunk);
            }
        }
    }
    return true;
}
```

这里的静态事实是：`source`/`target` 在各自 block 内可用，inner target 先关闭，读取 EOF
用 ZR `null` 表示，I/O failure 走 exception；C 层的 `ZR_NULL` 指针只用于 API 失败检查。
如果把 `source` 存入返回值、跨 await 或传给另一个 thread，compiler 会分别报告 ownership
escape、frame-safety 或 Send/Sync 问题。

## 11. C/AST 验证对应关系

模式和 ownership 的 C 查询不应从 token 文本重建：

| 需求 | 应读取 |
| --- | --- |
| 绑定名字和范围 | `SZrAstNode` pattern/declaration 节点（当前 parse snapshot 内） |
| 最终类型和 ownership | canonical `TypeId`、`SZrInferredType`、semantic ownership fact |
| cleanup 数量和顺序 | compiler scope/CFG cleanup registrations |
| 参数 mode | `SZrParameter.passingMode`、native `ZrLibParameterDescriptor` |
| 可跨 await 结论 | SemIR region/loan 和 task effect validation |

AST 释放使用 `ZrParser_Ast_Free`；semantic query 的字符串、fact pointer 和 declaration node
只在当前 generation 借用。C native callback 不能把 pattern 的 AST 地址或 ownership control
写进 module descriptor。宿主接口的 root/pin 规则见 [Native Callback 实战配方](../05-interop/native-callback-recipes.md)。

## 12. 错误到修复的对照表

| 诊断类别 | 典型代码 | 修复 |
| --- | --- | --- |
| pattern shape | unknown field、arity mismatch、variant mismatch | 修正 key/variant 或显式转换 |
| definite assignment | read before initialized | 在所有 flow 分支初始化，或改为 nullable |
| ownership move | use after move、double drop | 复制、share 或调整 owner 作用域 |
| borrow escape | return/field/closure 保存 borrowed view | 缩短 scope 或复制 owned value |
| cleanup | guard 未关闭、非 resource using | 实现/声明 cleanup contract，使用 using |
| async safety | loan/lock/resource 跨 await | await 前释放或 frame-safe 迁移 |
| thread safety | T 不满足 Send/Sync | 使用 transfer/clone 或 attached protocol |

这些诊断可能在不同阶段出现；先修复最早的 parser/shape 错误，再重新运行 semantic/flow，
不要用强制 cast、裸 native pointer 或关闭检查来掩盖 ownership 问题。
