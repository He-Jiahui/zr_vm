# 14 普通函数、测试元数据、断言与 TestManifest 实施计划

> **For agentic workers:** implement this document by milestone and reject any public test helper that is absent from the reference ledger in section 13.

**Goal:** 让测试复用普通 `fn` / `async fn`、第 11 章静态元数据、第 12 章 Task 和既有 artifact pipeline；compiler 只生成 typed `TestManifest`，runner 负责发现、隔离和报告。

**Architecture:** 测试不是新的函数种类。`#zr.testing.test#` 给普通函数附加 canonical metadata role；`#zr.testing.case(...)#` 生成 compile-time bound case descriptor；`#zr.testing.skip(...)#` 保留发现但跳过执行。production build 在完成完整 type-check 后裁掉 test roots，test build 把 manifest 交给独立 runner。

**Occam gate:** 不引入 `test` keyword、`test fn` AST、匿名 test block、generated `main`、TestContext、fixture class、per-test name/tag/serial/timeout 元数据或独立 assertion statement。所有函数都有显式返回 TypeRef；公共type/role/helper必须分别由第 13 节的仓库内 reference 支撑。

---

> 状态：设计第一版，等待按里程碑实施。
>
> 上游契约：[01 artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[06A migration inventory/frontend](./2026-07-18-06-percent-migration-lsp-fixtures-design.md)、[10R module/native descriptor foundation](./2026-07-19-10-native-ffi-module-package-design.md)、[11 compile-time metadata](./2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md)、[12 Task/Scheduler](./2026-07-20-12-async-task-job-scheduler-design.md)。本计划基于 10R 登记 `zr.testing` provider，不依赖 06B 最终仓库切换；本计划晋级后由 10C 做全局汇聚审计。

## 1. 语法：普通函数加最小元数据

### 1.1 无参数测试

```zr
let testing = import("zr.testing");

#zr.testing.test#
fn parsesEmptyInput(): void {
    testing.assert(parse("").isEmpty(), "expected empty syntax tree");
}
```

`FunctionDefinition`、name binding、参数、返回检查和 function body 全部复用普通函数。`test` 只是 compiler 已注册的 `AttributeRole::Test`，不是关键字或特殊 declaration。

### 1.2 参数化测试

```zr
#zr.testing.test#
#zr.testing.case(1, 2, 3)#
#zr.testing.case(-1, 1, 0)#
fn adds(lhs: int, rhs: int, expected: int): void {
    testing.equal(lhs + rhs, expected);
}
```

- 有参数的 test function 必须至少有一个 `case`。
- 每个 case argument 都必须是 comptime constant，并使用普通 call binder 完成 positional/named、arity 和 TypeRef 检查。
- 多个 case 共享同一个 function body；manifest 只保存 bound constant arguments，不复制函数。
- 不增加 data-source function、动态 case generator 或字符串表达式。

### 1.3 异步测试

```zr
#zr.testing.test#
async fn loadsFixture(): task.Task<void> {
    let document = await loadFixture();
    testing.assert(document.isValid(), "invalid fixture");
}
```

异步测试显式返回 `Task<void>`，与第 12 章一致；不允许 `async fn ...: void` 隐式改写。runner await 返回的 Task，并按普通 Task fault 规则取得失败。

### 1.4 跳过

```zr
#zr.testing.test#
#zr.testing.skip(reason: "requires external service")#
fn importsRemoteSchema(): void {
    importRemoteSchema();
}
```

`skip` 必须给出 compile-time non-empty reason。跳过项仍进入 discovery 和报告，body 仍完整 parse、bind 和 type-check。

## 2. 第一版只保留三个 metadata role

| 元数据 | target | 作用 |
| --- | --- | --- |
| `zr.testing.test` | module-scope ordinary function | 形成一个 TestEntry |
| `zr.testing.case(...constants)` | 已标记 test 的有参 function | 重复生成 TestCaseDescriptor |
| `zr.testing.skip(reason: string)` | 已标记 test 的 function | 保留 discovery，执行结果为 Skipped |

这些 schema 都由第 11 章的 `readonly struct` + `#zr.reflection.attributeUsage#` 表达；不增加 `attribute` declaration keyword。compiler 以 canonical AttributeId/role 绑定，不比较 `zr.testing.*` 文本。

### 2.1 N3 Test native module

`zr.testing`是第10章OfficialNative domain的N3 Test native host module：test/case/skip的readonly struct schema、`AssertionFailure`和三个assertion function全部由test host descriptor注册。上述schema仍具有普通readonly struct的Canonical TypeRef/field语义，但descriptor是规范真源，不另写一份ZR标准库实现。

TestEntry/TestCaseDescriptor/TestManifest是compiler/test runner共享的artifact schema，不成为runtime业务对象。普通Runtime phase解析`import("zr.testing")`必须报phase mismatch；test build使用同一ImportExpression绑定Test provider。禁止新增`zr.test`、`zr.test.runtime`或把assertion塞进`zr.builtin`。

第一版明确删除：

- `test fn`、`test async fn` 和 anonymous `test("name") { ... }`；
- `name`、`tag`、`serial`、per-test `timeout` 元数据；
- class-level setup/teardown、fixture inheritance 和 TestContext injection；
- expected-exception metadata；
- compiler-generated main；
- 通过源码文本扫描发现测试。

显示名固定来自 qualified function SymbolId；case suffix 来自稳定参数序号和值摘要。筛选直接使用 module/function qualified name，不制造第二套命名。

## 3. 合法签名

允许：

```text
#zr.testing.test# fn(): void
#zr.testing.test# fn(P1, ... Pn): void + one or more #zr.testing.case#
#zr.testing.test# async fn(): Task<void>
#zr.testing.test# async fn(P1, ... Pn): Task<void> + one or more #zr.testing.case#
```

表内省略了attribute application与function之间的换行，但使用的都是正式source spelling，不定义短别名。

禁止：

- generic test function；
- member/local/lambda/native/comptime/bodyless function；
- `ref` return、`in/ref/out` parameter、`ref struct` parameter；
- 任意非 `void` / `Task<void>` 结果；
- 默认依赖 runtime object 的 case argument；
- 同一 function 重复 `test` role。

限制的目的不是另造函数规则，而是让 TestManifest 可以在没有 runtime 反射和隐式对象构造的情况下直接调用普通 function。

## 4. 最小 `zr.testing` native函数

第一版descriptor只提供三个函数。下面是生成的interface projection，全部有显式返回TypeRef：

```zr
fn assert(condition: bool, message: string = ""): void;
fn equal<T>(actual: in T, expected: in T): void;
fn throws<E>(action: fn() -> void): E;
```

- `assert` 失败时抛出 typed `AssertionError`，记录 source span 和可选 message。
- `equal` 通过 canonical equality capability 比较，不做 dynamic member lookup；失败值使用已有 formatting capability。
- `throws<E>` 只接受显式 callable TypeRef `fn() -> void`，必须捕获精确 `E` 或其允许的 subtype，并返回该 exception 供进一步检查。
- 异步 expected exception 第一版使用普通 `try/catch` 和 `testing.assert`，不增加 `throwsAsync`。
- 不提供 `fail`、`notEqual`、`same`、`contains` 等可由 `assert` / `equal` 清楚表达的同义 helper。

assertion failure 是结构化数据，不靠解析 message：

```text
AssertionFailure {
    assertionKind;
    sourceSpan;
    message;
    expected?: ValueSnapshot;
    actual?: ValueSnapshot;
    exception?: ExceptionSnapshot;
}
```

ValueSnapshot 受长度、深度和 formatter fault 限制，runner 不因此持有被测对象的长期强引用。

## 5. TestManifest 与编译边界

compiler 在 test build 产生：

```text
TestEntry {
    functionSymbolId;
    functionTypeId;
    moduleId;
    sourceSpan;
    isAsync;
    skipReason?;
    cases[];
}

TestCaseDescriptor {
    ordinal;
    boundConstantArguments[];
}

TestManifest {
    schemaVersion;
    targetTriple;
    moduleGraphHash;
    entries[];
}
```

- manifest 从 bound symbols/attributes 生成，不扫描 source text。
- 普通 production build 仍完整 parse、bind、type-check test body，再从 reachability roots 排除 test-only function 和 TestManifest。
- production code 引用 test-only function 是 compile error，防止裁剪后留下 dangling call。
- test build 不生成 hidden `main`；`zr test` CLI 是独立 host，加载 artifact 和 manifest。
- manifest serializer、VM 和 AOT 使用同一 schema/version check。

## 6. Runner、隔离和调度

第一版默认顺序执行，减少共享状态、时序和诊断复杂度：

```text
load project
  -> compile test artifact
  -> read TestManifest
  -> order by module/function/case ordinal
  -> create module isolate
  -> invoke ordinary function
  -> await Task<void> when async
  -> collect structured result
  -> dispose isolate
```

- 每个 module 使用独立 VM isolate；同 module cases 默认共享 module artifact 但不共享前一个 case 的 mutable runtime state。
- `--jobs N` 是 runner 的显式并行选项，只并行独立 module isolates；source language 不增加 `serial` 或 `parallel` metadata。
- `--timeout duration` 是整次 runner/case 的 host policy，不进入 source metadata。超时先请求 isolate cooperative shutdown；无法停止时销毁整个独立 process/isolate，不在线程中强杀共享 VM state。
- 第一版没有 TaskGroup、detach 或 cancellation token API。runner 只等待测试函数返回的根 Task；未完成工作在 isolate dispose 时作为 leak/failure 记录。
- stdout/stderr 由 case id 分流；最终输出按 stable manifest order 合并，避免并发造成随机报告顺序。
- seed、jobs、timeout、target 和 module graph hash 写入 run metadata，便于复现。

## 7. 发现、筛选和退出码

CLI 最小 surface：

```text
zr test [project-or-module]
        [--filter qualified-pattern]
        [--jobs N]
        [--timeout duration]
        [--list]
```

筛选只匹配 canonical module/function/case id。第一版不引入 tag expression language。

结果状态固定为：

```text
Passed | Failed | Skipped | TimedOut | Crashed
```

进程退出码：

- 0：所有 executed cases Passed，允许存在 Skipped；
- 1：至少一个 Failed 或 TimedOut；
- 2：compile/discovery/configuration error；
- 3：runner/VM crash。

## 8. Semantic facts、LSP 与调试

AST 不新增 TestFunctionSyntax。binder 在普通 function symbol 上附加：

```text
TestRoleFact
TestCaseFact(boundArguments)
TestSkipFact(reason)
```

LSP 从 facts / manifest 提供 run、debug 和 case CodeLens，不扫描关键字。hover 显示原始普通函数签名与 test metadata；rename 只修改 symbol references，不维护独立 display-name 字符串。

debugger 以真实 function body/source map 断点。parameter case arguments 作为普通 parameters 展示；async test 使用 Task logical stack。

## 9. 旧 `%test` 迁移

- `%test("name") { body }` 转为稳定 identifier 的 `#zr.testing.test# fn generatedName(): void { body }`。
- 原字符串仅用于建议 identifier；第一版不保留 `name` metadata。冲突时 migration 使用稳定 source hash suffix，并标为 requires-review。
- 旧整数返回约定只有在能证明 `0 == pass` 且非零路径可改写为 assertion 时才自动迁移；否则保留 negative fixture 并要求人工处理。
- 已有 `test fn` 草案迁移为普通 function + `#zr.testing.test#`；`#zr.test.case#` 改为 `#zr.testing.case#`。
- formatter/writer 只输出当前 metadata + ordinary function 形式。

## 10. 分层里程碑

### M1 metadata role 与 TestManifest

复用第 11 章 attribute application 和普通 FunctionDefinition，实现 test/case/skip binder、signature checks、artifact roundtrip 和 production trimming。

### M2 最小 assertion library

实现 `assert`、`equal`、`throws` 与 structured AssertionFailure，补齐 source span、snapshot bounds 和 formatter fault isolation。

### M3 runner 与隔离

实现 discovery、stable ordering、sync/async invocation、module isolate、jobs、timeout、output capture 和退出码。

### M4 LSP、debug 与迁移

实现 CodeLens/debug request、manifest-based list/filter、`%test` / `test fn` migration 和 comprehensive reference fixture。

## 11. 实施任务

### Task 1: role binder 与 manifest

**Files:**

- Create: `zr_vm_parser/include/zr_vm_parser/test_contract.h`
- Create: `zr_vm_parser/src/zr_vm_parser/compiler/test_binding.c`
- Modify: `zr_vm_core/src/zr_vm_core/artifact_*.c`
- Test: `tests/testing/test_test_role_binding.c`
- Test: `tests/artifact/test_manifest_roundtrip.c`

- [ ] 断言测试仍是 ordinary FunctionDefinition。
- [ ] 覆盖 sync/async、有参 case、skip、非法 target/signature 和 constants。
- [ ] production artifact 无 test roots，test artifact manifest 可 roundtrip。

### Task 2: `zr.testing`

**Files:**

- Create: `zr_vm_lib_testing/CMakeLists.txt`
- Create: `zr_vm_lib_testing/include/zr_vm_lib_testing/module.h`
- Create: `zr_vm_lib_testing/src/zr_vm_lib_testing/module.c`
- Create: `zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/descriptor.c`
- Create: `zr_vm_lib_testing/src/zr_vm_lib_testing/runtime/assertions.c`
- Test: `tests/testing/test_assertions.c`

- [ ] descriptor登记Test/Case/Skip roles、AssertionFailure和三个function；不修改`zr_vm_lib_builtin`。
- [ ] 验证Test provider只在Test phase注册，Runtime/CompileTool import给出phase mismatch。
- [ ] 先写 assert/equal/throws success/failure/fault tests。
- [ ] 验证 AssertionFailure field、source span、bounded snapshot 和 exception identity。
- [ ] 不增加 ledger 外 convenience helper。

### Task 3: runner

**Files:**

- Create: `zr_vm_cli/src/commands/test_command.c`
- Create: `zr_vm_cli/src/testing/test_runner.c`
- Test: `tests/testing/test_runner.c`
- Project: `tests/fixtures/projects/testing_reference/`

- [ ] 覆盖 discovery order、filter/list、sync/async、skip、timeout/crash 和 exit code。
- [ ] 默认顺序执行；`--jobs` 只跨 module isolate。
- [ ] 每个 case 结束验证无 root Task、unobserved fault 或外部 root leak。

### Task 4: LSP、debug 与 migration

**Files:**

- Modify: `zr_vm_language_server/`
- Modify: `zr_vm_debugger/`
- Test: `tests/language_server/test_lsp_test_facts.c`
- Test: `tests/migration/test_percent_test_migration.c`

- [ ] CodeLens 数据来自 TestRoleFact/TestManifest。
- [ ] async logical stack 与 parameter values 可调试。
- [ ] migration 幂等，current fixture 不含 `test fn` 或 `#zr.test.*#`。

## 12. 晋级门

必须同时满足：

- test syntax 不新增 keyword 或 function AST kind。
- `zr.testing`只有N3 Test native owner；`zr.builtin`、`zr.test`和runtime source module没有重复schema/function。
- test host descriptor、TestManifest、LSP CodeLens和runner看到相同AttributeId/CallableContract；production graph不携带testing executable。
- 所有测试函数与 assertion helper 都有显式返回 TypeRef。
- 只存在 test/case/skip 三个 metadata role。
- production build 完整检查后移除 test roots。
- discovery/LSP/CLI 均使用 typed manifest/facts，不扫描文本。
- runner 默认确定性顺序，parallelism 只由 `--jobs` 显式启用。
- async test 显式返回 `Task<void>`。
- 第 13 节每个公共角色和函数的来源与测试路径都存在。
- legacy `%test`、`test fn`、`#zr.test.*#` 只存在 migration/negative fixture。

## 13. 公共 API reference ledger

每个 public role/function 至少有协议或实现来源，以及独立行为测试来源。路径均为仓库内 `./lua/` 镜像。

| ZR 类型、角色或函数 | 协议/实现来源 | 行为测试来源 | 采纳边界 |
| --- | --- | --- | --- |
| `TestEntry` / `TestCaseDescriptor` / `TestManifest` | `lua/rust/compiler/rustc_builtin_macros/src/test_harness.rs`；`lua/cpython/Lib/unittest/loader.py` | `lua/rust/tests/coverage/test_harness.rs`；`lua/cpython/Lib/test/test_unittest/test_discovery.py` | typed artifact/runner contract；不扫描源码或生成main |
| `AssertionFailure` | `lua/cpython/Lib/unittest/result.py`；`lua/jdk/src/java.base/share/classes/java/lang/AssertionError.java` | `lua/cpython/Lib/test/test_unittest/test_result.py`；`lua/cpython/Lib/test/test_unittest/test_assertions.py` | 结构化失败数据；不以message解析expected/actual |
| `zr.testing.test` role | `lua/rust/compiler/rustc_builtin_macros/src/test.rs`；`lua/cpython/Lib/unittest/loader.py` | `lua/rust/tests/coverage/test_harness.rs`；`lua/cpython/Lib/test/test_unittest/test_discovery.py` | 元数据标记 ordinary function；不生成特殊 function AST |
| `zr.testing.case(...)` role | `lua/cpython/Lib/unittest/case.py` 的 subtest/call binding；`lua/rust/compiler/rustc_builtin_macros/src/test_harness.rs` | `lua/cpython/Lib/test/test_unittest/test_case.py` | 编译期常量 descriptor；不复制 body 或执行动态 generator |
| `zr.testing.skip(reason)` role | `lua/cpython/Lib/unittest/case.py`；`lua/rust/compiler/rustc_builtin_macros/src/test.rs` | `lua/cpython/Lib/test/test_unittest/test_skipping.py`；`lua/rust/tests/run-make/test-harness/test-ignore-cfg.rs` | 保留 discovery 和 reason |
| `testing.assert(...): void` | `lua/cpython/Lib/unittest/case.py`；`lua/jdk/src/java.base/share/classes/java/lang/AssertionError.java` | `lua/cpython/Lib/test/test_unittest/test_assertions.py` | typed failure；不新增 assert statement |
| `testing.equal<T>(...): void` | `lua/cpython/Lib/unittest/case.py` | `lua/cpython/Lib/test/test_unittest/test_assertions.py` | 复用 canonical equality/formatting |
| `testing.throws<E>(...): E` | `lua/cpython/Lib/unittest/case.py` | `lua/cpython/Lib/test/test_unittest/test_assertions.py` | 同步 callable；异步使用普通 try/catch |
| async test invocation | `lua/cpython/Lib/unittest/async_case.py`；`lua/runtime/src/libraries/System.Private.CoreLib/src/System/Threading/Tasks/Task.cs` | `lua/cpython/Lib/test/test_unittest/test_async_case.py` | 显式 `Task<void>`，不隐式包装 |
| discovery/result/skip 状态 | `lua/cpython/Lib/unittest/loader.py`；`lua/cpython/Lib/unittest/result.py` | `lua/cpython/Lib/test/test_unittest/test_loader.py`；`lua/cpython/Lib/test/test_unittest/test_result.py` | ZR 改用 artifact manifest，不扫描运行时对象 |
| test harness entry | `lua/rust/compiler/rustc_builtin_macros/src/test_harness.rs` | `lua/rust/tests/coverage/test_harness.rs` | 独立 `zr test` host；不生成 source-level main |

任何新 assertion helper、fixture hook、metadata role 或 runner callback 都必须先补充必要性、仓库内 reference 和行为/negative tests；否则使用普通 function、`try/catch`、module isolate 或 CLI option 表达。
