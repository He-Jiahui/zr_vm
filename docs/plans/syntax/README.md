# ZR 语法重设计子计划索引

> 状态：总设计已完成分阶段晋级；截至 2026-08-05，06B 与 07B 已完成最终收口，01-14 的当前 gate ledger 全部有独立验收证据。
>
> 总设计：[ZR 语法、引用与内存模型重设计](./2026-07-18-zr-syntax-and-memory-model-redesign.md)

## 1. 目的

本目录把总设计拆成十四份设计文档。文档编号是稳定标识，不再等同于完整实施顺序；06、07、10 各自包含可独立验收的阶段节点。拆分的目标不是让多条实现线并行绕过基础层，而是让每个阶段都具有清晰输入、输出和晋级门。

## 2. 设计文档

| 编号 | 文档 | 主要产物 | 硬依赖 |
|---:|---|---|---|
| 1 | [Canonical TypeRef、Place IR、CFG facts 与 artifact schema](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | 规范类型、Place/Value IR、数据流事实、产物契约 | 无 |
| 2 | [`fn/ref/in/out/scoped/readonly` 与 borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md) | 新语法、引用契约、借用检查、诊断 | 1 |
| 3 | [struct/ref struct、receiver effect、Span 与 layout](./2026-07-18-03-struct-ref-struct-span-layout-design.md) | `init TypeRef(...)` 值构造、值布局、ref-like 限制、receiver effect、连续视图 | 1、2 |
| 4 | [resource class、Unique/Shared/Weak、Drop 与 GC bridge](./2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | 确定性生命周期、GcDomain、domain-local STW、TransferEnvelope owner handoff、跨世界/跨domain桥 | 1、2、3 的布局契约 |
| 5 | [property 统一 AST、显式字段与 ref-return property](./2026-07-18-05-property-unified-ast-design.md) | 统一属性模型、访问器、显式 `let/var` field、ref 返回 | 1、2、3 的 receiver/layout 契约 |
| 6 | [`%xxx` 迁移、LSP、文档与全项目 fixture](./2026-07-18-06-percent-migration-lsp-fixtures-design.md) | 06A：迁移盘点、frontend 与 dry-run；06B：最终仓库切换与清理 | 06A：1-5；06B：06A、8、10-14 |
| 7 | [目标语法全覆盖参考工程](./2026-07-19-07-comprehensive-syntax-reference-fixture-design.md) | 07A：fixture/manifest 骨架；07B：VM/AOT/artifact/LSP 的 current reference 晋级 | 07A：1-5、06A；07B：07A、06B、8-14 |
| 8 | [`zr.reflection` 独立反射库与运行时类型系统](./2026-07-19-08-reflection-library-type-system-design.md) | Type/TypeOf层级、成员查询、typeof/typeid、动态构造、metadata/AOT边界 | 语义：1、3-5；native 集成：10R |
| 9 | [generational PoolHandle/PoolRef 与连续池化内存](./2026-07-19-09-generational-pool-handle-ref-struct-design.md) | 弱handle、guarded direct ref、延迟复用、slab/GC scan contract | 语义：1-5；native/反射集成：8、10R |
| 10 | [Native extern、`zr.*`核心库、模块与包解析](./2026-07-19-10-native-ffi-module-package-design.md) | 10R：resolver/package；10F：FFI ABI；10C：native provider 汇聚 | 10R：1、06A；10F：1-4、10R；10C：8、9、11-14 的 provider contract |
| 11 | [编译期执行、条件编译、静态元数据与类型化声明生成](./2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | BuildPredicate、comptime sandbox、AttributeData、DeclarationPatch、conditional call | 1-5、06A、8、10R |
| 12 | [`async/await`、Task/Job/Scheduler 与线程协程模型](./2026-07-20-12-async-task-job-scheduler-design.md) | Async effect、hot Task/cold Job、scheduler domain policy、Send/Sync、DropOnFailure transport contract | 1-5、9、10R、11；AttachedDomain依赖04 M5，IsolatedDomain依赖04 M6 |
| 13 | [普通 `fn`、Enumerator、`yield` 与异步迭代](./2026-07-20-13-iterator-enumerator-yield-design.md) | 显式 Iterator TypeRef、yield state machine、for lowering、AsyncIterator | 1-5、12 |
| 14 | [普通函数、测试元数据、断言与 TestManifest](./2026-07-20-14-test-function-harness-design.md) | TestEntry/TestManifest、`zr.testing`、同步/异步/参数化 runner | 1、06A、10R、11、12 |

## 3. 依赖关系

```mermaid
flowchart TD
    A["01 Canonical TypeRef / Place / CFG / artifact"] --> B["02 引用语法与 borrow checker"]
    B --> C["03 struct / ref struct / Span / layout"]
    A --> C
    C --> D["04 ownership / Drop / GC bridge"]
    B --> D
    C --> E["05 property / explicit field / ref return"]
    B --> E
    A --> E
    D --> F["06A 迁移盘点 / frontend / dry-run"]
    E --> F
    C --> F
    F --> S["07A reference fixture / manifest 骨架"]
    A --> R["10R ModuleSpecifier / resolver / package"]
    F --> R
    A --> H["08 zr.reflection / typeof / typeid"]
    C --> H
    D --> H
    E --> H
    A --> I["09 zr.pooling / PoolHandle / PoolRef"]
    B --> I
    C --> I
    D --> I
    E --> I
    R --> H
    R --> I
    H --> I
    A --> J["10F native extern / FFI ABI"]
    B --> J
    C --> J
    D --> J
    R --> J
    A --> K["11 comptime / metadata / typed transform"]
    F --> K
    H --> K
    R --> K
    E --> K
    A --> L["12 async / Task / Job / Scheduler"]
    B --> L
    C --> L
    D --> L
    E --> L
    I --> L
    R --> L
    K --> L
    A --> M["13 fn returning Iterator / Enumerator / yield"]
    B --> M
    C --> M
    D --> M
    E --> M
    L --> M
    A --> N["14 test metadata / assertion / manifest"]
    F --> N
    R --> N
    K --> N
    L --> N
    H --> Q["10C official native provider convergence"]
    I --> Q
    J --> Q
    K --> Q
    L --> Q
    M --> Q
    N --> Q
    R --> Q
    F --> P["06B atomic repository cutover / cleanup"]
    H --> P
    Q --> P
    K --> P
    L --> P
    M --> P
    N --> P
    P --> G["07B current syntax reference promotion"]
    H --> G
    I --> G
    Q --> G
    K --> G
    L --> G
    M --> G
    N --> G
    S --> G
```

实施顺序以阶段节点而不是文档编号为准：第 4、5 项在第 1-3 项稳定后可以分别实施；06A 和 07A 只建立迁移/fixture 基础设施。10R 随后固定 module/package/resolver 与 descriptor substrate，第 8、9 项和 10F 再分别完成 reflection、pooling 与 FFI ABI。第 11 项固定编译阶段和静态 metadata。第 12 项的AttachedDomain worker必须等待04 M5的domain-local STW/multi-mutator gate，IsolatedDomain worker必须等待04 M6的transport gate；第13、14项再分别完成iterator与test harness。08、09、11-14 各自基于 10R 登记本计划拥有的 provider contract；10C 只汇聚并审计这些已晋级 contract 的 inventory、identity 和 phase 一致性。06B 必须等待 08、10-14 全部目标契约通过后才执行最终仓库切换；07B 又必须等待 07A、06B 和 08-14，才能把设计样例晋级为 current reference。下游引用“依赖 06”时必须写明 `06A` 或 `06B`，不得把二者视为同一个已完成 gate。

截至 2026-08-05，10R、10F、10C 均已完成三工具链晋级；06B 随后完成仓库 inventory v3、旧 source-intermediate AST/consumer 清理和 current spec 发布，07B 再完成零 pending coverage、真实项目 import、interp/binary checksum 与 owner-consumer 证据汇聚。根 Syntax 计划由此完成最终晋级；详细证据见 `tests/acceptance/2026-08-05-syntax-06b-repository-promotion.md` 与 `tests/acceptance/2026-08-05-syntax-07b-current-reference.md`。

## 4. 共同约束

十四份设计共同遵守以下规则：

1. AST 只记录语法，不承担规范类型、借用状态或运行时所有权状态。
2. Semantic IR 必须在 ExecBC/bytecode 之前形成；不能从执行指令反推语言语义。
3. VM、AOT、LSP、反射和 artifact writer 消费同一规范 TypeId、SymbolId、Place/Region facts 或其稳定投影。
4. shared foundation 不允许通过 `Span`、`Unique` 等具体类型名字符串触发特例。
5. borrow、move、readonly、ref struct 逃逸和 `out` 确定赋值必须静态完成。
6. 数组越界、Weak upgrade、动态 cast 和 native 输入等无法静态证明的状态仍保留运行时检查。
7. 每个里程碑先验证下层单元，再验证父层，最后验证项目和 CLI；上层 smoke 不能替代下层测试。
8. `.zro` 中只保存跨模块所需的稳定类型、布局和公开契约；局部 flow facts 默认只进入 `.zri` 和调试 sidecar。
9. 总设计中已经批准的 `const fn` 只读 receiver、普通 `fn` 可写 receiver 是所有子设计的共同前提。
10. 旧 `%xxx` 只允许存在于迁移输入、负例和历史文档中，不保留长期双轨语义。
11. 命名/匿名函数定义以 `:` 引出 return TypeRef，callable 类型以 `->` 表示输入到输出，`=>` 只引出 anonymous/expression body；返回 callable 写作 `fn make(...): fn(A) -> R`。
12. struct 值构造使用 `init TypeRef(...)`；普通 `expr(...)` 只执行 call/`@call`，旧 `$proto(...)` 动态原型构造退出核心语法。
13. 反射位于 `zr.reflection`；`typeid(TypeRef)` 是轻量身份，`typeof(expr)` 是运行时精确 descriptor。动态构造只通过 `ConstructibleType.createInstance(...constructionArgs: object): object`，不得回退到 `init`、`new`、`own` 或 `@call`。
14. 换行始终只是 trivia，不结束声明或语句，也不触发 automatic semicolon insertion。field/local/module/import binding、expression/assignment、`return/throw/break/continue`、bodyless declaration 和 expression accessor 等 simple form 必须显式以 `;` 结束。
15. 已由 `{ ... }` 闭合的 type/function/property等braced declaration可以省略一个尾随 `;`，规范 formatter默认省略；compound control-flow statement由完整`if/else`、`try/catch/finally`等grammar闭合，不消费declaration semicolon。若braced anonymous function是`let` initializer，结束的是外层binding，因此`}`后仍必须写`;`。
16. 静态导入使用模块级不可变绑定 `let alias = import("module.path");`。`import(...)` 是只接受string literal的专用ImportExpression；runtime/test provider返回对应phase的只读ModuleNamespace并记录依赖，CompileTool provider返回compiler-domain namespace且不进入runtime graph。runtime dynamic path使用`loadModule/loadPlugin`，不进入该语法。
17. concrete property 不生成 backing field；存储必须预先用 `pri/pro/pub let|var` 显式声明。ref-return property是getter-only，不存在ref setter或`property = ref other`；writable ref getter以独立`exportsWritableRef` effect区分“view自身readonly”和“referent可写”。
18. 池化标准库位于 `zr.pooling`：`PoolHandle<T>` 是可长期保存的 generational weak identity，`tryBorrow(handle, out PoolRef<T>): bool` 单次验证并初始化scoped direct ref；不用非法的`Option<PoolRef<T>>`。recycle立即使handle失效，slot复用延迟到active guard归零。
19. 只有 Canonical TypeLayout 证明 `GcFree` 的 closed type/slab 可以 `NoScan`；长期持有、old generation 或 pool 标记不能替代 GC pointer map、write barrier 和 remembered set。
20. `let/var { ... } = expr;`与`let/var [ ... ] = expr;`保留；RHS只求值一次，object alias为`local: field`，array采用“至少N项、额外忽略、不足失败”，第一版不含default/rest/nested pattern。
21. static native declaration使用`native extern("library") { ... }`；语言 callable contract和ABI `FfiSignature`在绑定期一次形成，VM/AOT共享，`zr.ffi`不在每次静态调用时重解析字符串签名。
22. module literal先解析为ModuleSpecifier再规范化为ModuleIdentity；`.`和`/`可作为segment分隔，`.`/`..`相对前缀具有独立语义。`.zrp` alias只使用单段`#identifier`。
23. 第三方包名只允许单段`@identifier`。`@math`是package root/default entry，`@math.matrix`与`@math/matrix`是同一子模块；第一版不引入组织作用域包名或最长匹配。
24. `zr.*`是不可被source/alias/package/custom native覆盖的OfficialNative保留根；所有正式核心库都由`ZrLibModuleDescriptor`或compiler/test host等价descriptor提供。`native:engine.render`进入RegisteredNative，普通`engine.render`进入Workspace，两者可同名并存；`file:`只定位声明了identity的`.zr/.zrp/.zrm`。`native:`/`file:`只是import literal scheme，不是关键字。N0-N3只控制build/link/load/phase，不进入语法、TypeId或runtime dispatch；native不等于每模块一个DLL。
25. 遵守 Occam gate：新增关键字、AST kind、公共类型、公共函数或metadata role前，必须证明现有`fn`、TypeRef、property、struct、attribute application、Drop、Task和typed data不能表达；无法证明必要性则不增加。
26. 所有function definition必须显式写`: ReturnType`，签名就是调用方和artifact看到的真实TypeRef；禁止把`: T`暗中改写为`Task<T>`、`Iterator<T>`等carrier。每个新增公共类型、constructor、函数和metadata role都必须分别列出仓库内reference implementation与behavior/compiler tests；不能用某个相关函数的来源替代类型本身的来源，无来源的定义不进入第一版。
27. 编译期用户扩展只能读取immutable declaration view，并以typed `DeclarationPatch` data返回新增声明；禁止token/source string、任意AST改写、修改已有function body和runtime module-init decoration。attribute schema是带`AttributeUsage` role的普通`readonly struct`，declaration transform是带role的普通`comptime fn(...): Patch`。
28. `#zr.compile.conditional("feature")#`只允许可直接静态绑定的`fn(...): void`；禁用时call与argument lowering整体消失。declaration/statement条件裁剪只使用`comptime if`，不增加`when`或重复的条件metadata。
29. `async fn(...): zr.task.Task<T>`显式声明hot Task carrier；cold work使用`zr.task.Job<T>`，由`zr.task.Scheduler.schedule(...): Task<T>`消费。`zr.thread`只拥有`ThreadScheduler`、`Send`和`Sync`，不复制Task/Job/Scheduler；语言不增加spawn/thread/coroutine/job/domain关键字。
30. 含`yield`的普通函数显式返回`zr.iteration.Iterator<T>`；异步迭代显式返回`zr.iteration.AsyncIterator<T>`。二者的public TypeId由`zr.iteration` native descriptor唯一拥有，function-private frame只进入artifact；`for`只消费同模块的Enumerator/Iterable capability。
31. 测试是带`#zr.testing.test#`metadata的普通`fn(...): void`或显式返回`zr.task.Task<void>`的`async fn`；`zr.testing`是N3 Test native host模块，compiler不增加`test`关键字或宏生成main，production graph不链接testing executable。
32. GC collection/pause scope是host配置的`GcDomain`，不与进程、OS thread或游戏实例强绑定。一个domain可含多个state/mutator，STW只等待该domain；same-domain跨mutatormove/share分别受Send/Sync约束，跨domain禁止普通GC edge并只走Canonical `DomainTransferKind` transport。host可选择全局、每实例或分组domain，语言不替部署决定成本策略。
33. 跨mutator owner handoff统一使用runtime-internal TransferEnvelope：source参数绑定后永久Moved，producer release发布、consumer acquire claim，envelope在commit前是唯一owner，失败恰好Drop一次。第一版`schedule(Job): Task`采用DropOnFailure，不隐式恢复源变量、不在claim后自动retry，也不把owner exactly-once误述为消息必达或Job副作用exactly-once。

## 5. 建议重点复核的细化决定

以下决定是在总方向下进一步具体化的内容，适合优先人工检查：

- 具体 struct 继承被取消，改用 interface 与组合，以保持内联布局稳定。
- readonly struct 内普通 `fn` 自动获得 readonly receiver；普通 class/struct 仍以 `const fn` 显式只读。
- `Shared<T>` 第一版非原子且不能跨线程，`AtomicShared<T>` 后续独立提供。
- `GcDomain`采用混合模型：host选择共享或隔离heap，每个domain内部以local-STW为正确性基线并可演进concurrent major；不存在隐式process-wide full GC。
- same-domain Unique handoff保持O(1) handle move，不退化为AtomicShared/refcount；cross-domain ResourceMove必须通过prepare/commit/abort token。调度失败只fault Task并Drop capture，recoverable submission若以后加入必须是独立public API。
- `Unique<T>.intoGc()` 生成 `GcBox<T>` 并明确放弃确定性释放；Shared 不支持该转换。
- resource class 持有 GC 对象必须通过 `Gc<T>` root handle。
- concrete property 必须显式代理预先声明的 field；ref-return property 必须显式 getter，且不允许 set/init/auto backing。
- 正式 compiler 不执行旧语义，旧 parser 只保留在 migration frontend。
- struct 构造只接受静态 TypeRef 并直接降低为 destination-first `ValueConstruct`；runtime `Type` 必须先通过 `ConstructibleType` capability check 才能进入显式 `createInstance` boundary。
- `ConstructibleType.createInstance` 第一版只按位置绑定 public constructor；普通 class 返回 GC object，struct 返回 boxed object，并拒绝 resource class、ref struct、interface、abstract type 和 open generic。
- `PoolHandle<T>` 不因为语义上是 weak identity 就声明为 ref struct；只有包含 direct ref/guard 的 `PoolRef<T>` 受 ref-like 存储限制。
- enum/union variant和其他无独立 block 的 member declaration统一使用 `;`；不保留 comma/newline terminator双轨。
- import binding的 alias既是 module namespace symbol，也是运行时只读 module object；只有原始 immutable import binding可以在 TypeRef中限定 imported type，普通 object alias不能伪装成 type namespace。
- 官方core TypeRef不允许跨模块re-export同名定义：Task/Job/Scheduler归`zr.task`，ThreadScheduler/Send/Sync归`zr.thread`，Iterable/Enumerator/Iterator/AsyncIterator归`zr.iteration`；compiler按descriptor role/capability id识别，不比较type-name字符串。
- generated field在final TypeLayout前参与正常layout/GC map/hash，但declaration transform不能修改已有field或body；需要代理时只能新增显式member。
- `Task<T>`、`Job<T>`、`Iterator<T>`和`AsyncIterator<T>`分别建立独立reference ledger项；它们的状态、cold/hot、single-use、cleanup和allocation contract不能互相借用一个笼统“协程参考”。
- async invocation采用显式`Task<T>`返回类型；指定执行位置、延迟执行和thread work统一用`init Job<T>(...)`与Scheduler表达。
- compiler-generated `Iterator<T>`是single-use；Span/PoolRef等带直接引用的高性能遍历使用手写ref struct Enumerator，不允许direct borrow跨yield。
- test parameter只由重复`#zr.testing.case(...)#`提供compile constants；普通production artifact不携带test body或manifest。

## 6. 源码现状锚点

当前基础并非空白，但需要重新划分职责：

- `zr_vm_parser/include/zr_vm_parser/ast.h` 同时保存 passing mode、ownership qualifier、property 分裂节点和 receiver qualifier。
- 同一 AST 中的 `SZrConstructExpression.isNew/isUsing` 仍混合 `$`、`new` 与 ownership construct；目标设计要求拆成 `ValueConstruct`、`GcNew`、`OwnConstruct` 和普通 `Call`。
- `zr_vm_core/include/zr_vm_core/function.h` 中的 `SZrFunctionTypedTypeRef` 仍是扁平类型字段；现有 `SZrSemIrInstruction` 是执行指令 sidecar。
- `zr_vm_parser/include/zr_vm_parser/cfg.h` 的 block 仍以单个 AST statement 和最多两个 successor 为中心。
- `zr_vm_parser/include/zr_vm_parser/semantic_facts.h` 已有 TypeId、SymbolId、LifetimeRegionId 和数据流 facts，可作为查询层基础。
- `zr_vm_core/include/zr_vm_core/type_layout.h` 已有 byte size/alignment、field offsets、GC/ownership offsets 和 copy/drop kind。
- `zr_vm_core/include/zr_vm_core/metadata_token.h` 已有 TypeDef、TypeRef、TypeSpec、Signature token 与 layout hash。
- `zr_vm_core/include/zr_vm_core/ownership.h` 已有 unique/shared/weak/borrow/loan/detach/release runtime 入口，但需要按新模型删减并分层。
- `zr_vm_language_server` 已经消费 semantic facts；迁移目标是扩充统一查询，而不是在 LSP 内重做类型和借用判断。
- `zr_vm_parser/compiler/compile_time_executor.c` 目前混合typed value、runtime object projection和decorator patch；目标拆为BuildPredicate、typed evaluator、declaration expansion和late check。
- `zr_vm_core/include/zr_vm_core/global.h`目前由每个`SZrGlobalState`直接拥有collector，`zr_vm_core/src/zr_vm_core/gc/gc.c`只有单state safepoint/step入口；目标拆出可挂接多个state的GcDomain与domain-local safepoint handshake。
- `zr_vm_lib_task`与`zr_vm_lib_thread`目前各自持有dynamic scheduler/object-field状态，thread worker目前新建独立`SZrGlobalState`；`runtime_transport.c`还通过dynamic `Transfer.taken`/object field先消费源再尝试目标decode。目标共用Task/Await/Scheduler ABI，让host在AttachedDomain与IsolatedDomain provider policy间选择，并以Canonical TransferEnvelope替换该失败窗口。
- AST仍有`ZR_AST_GENERATOR_EXPRESSION`和`SZrTestDeclaration`特殊block形态；目标分别替换为Iterator effect/yield CFG与普通function上的TestEntry contract。

## 7. 晋级规则

每份文档都提供自己的 promotion gate。共同最低门槛为：

- 枚举全部公开语法和规范 IR 形态。
- 覆盖成功、边界、失败和诊断范围。
- 覆盖 VM 与 AOT 的等价行为。
- 覆盖 `.zrs/.zri/.zro` 中受影响的产物。
- 覆盖 LSP 或明确说明该阶段为何只提供 query facts。
- 不存在 concrete type-name dispatch 或临时表层拼写特判。
- 未覆盖项必须保持里程碑 open，不能以“后续补测试”晋级。

## 8. 文档优先级

设计解释发生冲突时按以下顺序处理：

1. 已确认的子设计文档。
2. 总设计文档。
3. `docs/zr_language_specification.md` 当前版本。
4. `.codex/plans/` 中被本设计替代的历史计划。

在 06B 完成前，正式语言规范仍描述当前可执行语法；子设计是目标规范，不应被误写成当前版本已经支持的能力。
