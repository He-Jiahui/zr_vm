# 04 resource class、Unique/Shared/Weak、Drop 与 GC bridge

> 状态：已按可配置 GC domain 混合模型补齐并发边界，等待按里程碑实施。
>
> 硬依赖：[Canonical TypeRef/Place/CFG](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)、[TypeLayout/ref struct](./2026-07-18-03-struct-ref-struct-span-layout-design.md)

## 1. 目标结果

建立一个日常可用、静态安全且性能成本可预测的 ownership world：

- `resource class` 明确表示确定性生命周期类型。
- `Unique<T>` 表示唯一 owner，move 后源静态失效。
- `Shared<T>` 表示单 mutator/线程局部的非原子共享 owner；跨 mutator 共享不是它的隐含能力。
- `Weak<T>` 观察 Shared 生命周期，不延长对象存活。
- shared/mutable borrow 分别由 `ref readonly T`/`ref T` 表示，不再创建运行时 Borrow/Loan wrapper。
- Drop glue 统一正常退出、早返回、异常和部分构造清理。
- GC 与 ownership world 通过显式 bridge 交互，不使用隐藏 ignore registry 猜测生命周期。
- GC 的 collection/pause scope 是宿主可配置的 `GcDomain`，不是语言强制的“全进程”或“每游戏实例”策略。
- 普通路径不需要运行时 use-after-move、double-drop 或 loan 检查。

## 2. 生命周期世界

### 2.1 GC world

```zr
class Document { ... }
let document = new Document(...);
```

- 普通 class 由精确 GC 管理。
- class 可以形成强环，GC 负责回收不可达图。
- class handle 可以复制。
- 每个 GC object 永久属于一个 `GcDomain`；同一 domain 可以挂接一个或多个 VM state/mutator。
- GC compact/移动由 runtime handle 和精确根图处理。
- finalization 不作为普通业务资源的确定性释放基础。

### 2.2 Ownership world

```zr
resource class FileHandle { ... }
let file: Unique<FileHandle> = own FileHandle(...);
```

- resource class 不能通过普通 `new` 创建。
- resource class 实例必须处于 Unique/Shared/GcBox 等明确生命周期载体中。
- 纯 ownership graph 不参与 GC reachability tracing。
- owner 结束时按 Drop contract 立即释放资源。

### 2.3 类型而非分配点决定世界

同一个 class 不再允许有时 `new` 进入 GC、有时 `%unique new` 脱离 GC。类型声明确定生命周期类别：

- `class`：GC type。
- `resource class`：ownership type。

这避免同一字段声明、析构规则和 alias contract 因运行时分配模式不同而改变。

## 3. 源语法

```zr
resource class Texture {
    const fn width(): int { ... }
    fn upload(data: in ReadOnlySpan<byte>): void { ... }
}

let texture: Unique<Texture> = own Texture(...);
let shared: Shared<Texture> = texture.share();
let weak: Weak<Texture> = shared.weak();

if let Some(value) = weak.upgrade() {
    render(value);
}

drop(shared);
```

表层只有：

- 类型：`resource class`、`Unique<T>`、`Shared<T>`、`Weak<T>`。
- 构造：`own T(...)`。
- 转换：`.share()`、`.weak()`、`.upgrade()`、`.intoGc()`。
- 提前释放：`drop(value)`。

删除 `%borrow/%loan/%borrowed/%loaned/%release/%detach` 运行时入口语法。

## 4. resource class

### 4.1 字段规则

resource class 可以直接包含：

- value/struct。
- Unique/Shared/Weak owner field。
- 其他 resource class 的 owner handle。
- 显式 `Gc<T>` bridge handle。

resource class 不能直接包含：

- 普通 GC class handle。
- 未受约束的 dynamic/object GC value。
- 裸 native pointer，除非字段具有明确 native ownership/pin contract。
- 非法逃逸 ref/ref struct。

直接禁止 GC handle 的原因不是 GC 无法扫描，而是让“该 ownership graph 是否需要 tracing”成为类型可见事实。需要跨世界时使用 `Gc<T>`。

### 4.2 构造

- `own T(...)` 分配 resource storage 并建立 Unique owner。
- 构造期间按字段维护 initialization bitmap/facts。
- 构造成功后 value 进入 fully initialized/available。
- 构造抛错时，只 drop 已初始化字段，逆序执行，不调用完整对象的 custom drop body。
- `this` 在构造完成前不能逃逸、share、intoGc 或被 closure 捕获。
- 构造器调用 helper 的限制由借用/逃逸/effect contract 决定，不通过“resource constructor 一律不能调用函数”的硬编码规则实现。

### 4.3 继承与 interface

推荐第一版：

- resource class 不继承普通 GC class，普通 class 也不继承 resource class。
- resource class 可以实现不要求 GC boxing 的 interface。
- resource class 之间是否允许具体继承沿用 class 单继承规则，但 base 必须同为 resource class，且 Drop/layout chain 完整。
- interface 转换不能产生普通 GC box；interface handle 必须保留 owner/borrow capability。

## 5. Unique

### 5.1 语义

- `Unique<T>` 非 nullable；可选 owner 使用 `Option<Unique<T>>`。
- Unique 不可复制。
- 按值赋值、参数和返回执行 move。
- move 后源 Place 为 moved，除重新初始化外不可读取、借用、drop。
- scope exit 自动 drop。
- `drop(unique)` 提前消耗 owner。
- `unique.share()` 消耗 Unique 并返回 Shared。
- 只有 Unique 可以提供 `ref T` mutable borrow。
- active borrow 期间 Unique 不能 move、share、drop 或 intoGc。

### 5.2 目标表示

性能目标是 Unique handle 的普通表示只需一个 resource pointer/handle，不需要每次访问引用计数控制块。

resource header 至少能解析 TypeDef/Drop/Layout；是否将 header 与对象数据紧邻属于 runtime 布局实现，不影响语言 ABI。Unique 到 Shared 转换允许一次性创建 control block。

## 6. Shared 与 Weak

### 6.1 Shared 语义

- Shared 可复制；复制 strong count 加一，drop 减一。
- Shared 默认非原子且绑定 thread/actor isolation domain。
- Shared 只能提供 readonly borrow/`const fn` receiver。
- 需要共享可变状态时使用显式 Cell/Mutex/actor protocol，不能从 Shared 强化出 `ref T`。
- final strong drop 立即执行对象 Drop。
- Shared 本身非 nullable；可选使用 Option。

### 6.2 Weak 语义

- Weak 只能从 Shared 创建。
- Weak 不增加 strong count。
- Weak 可复制，维护 weak count 或等价控制块存活契约。
- `upgrade()` 在对象仍存活时增加 strong count并返回 `Some(Shared<T>)`，否则返回 `None`。
- Weak 不允许直接调用 target 成员；必须先 upgrade。这避免每次成员调用隐式做 nullable/liveness 检查。

### 6.3 Control block

概念结构：

```text
SharedControl<T>
  object
  strongCount
  weakCount
  type/drop descriptor
  isolationDomainId
```

规则：

1. strong > 0 时 control 持有一个 implicit weak。
2. final strong drop：先使 upgrade 不再成功，再执行 object drop，最后释放 object storage。
3. object 被释放后 control.object 为空，但 weak handle 仍可安全查询失败。
4. weak count 归零后释放 control block。
5. Drop 期间不能通过 Weak 重新 upgrade 当前已经开始销毁的对象。

当前 `SZrOwnershipControl` 使用 linked weak slots 和 `isDetachedFromGc`。目标实现应改为稳定 Weak handle/control lifetime，不再追踪并主动把所有 Weak 变量 slot 写 null；Weak 的失效由 `upgrade()` 查询 control state 表达。

### 6.4 AtomicShared

第一版 `Shared<T>` 不跨线程。后续 `AtomicShared<T>`：

- strong/weak 使用原子操作。
- target 满足`MutatorMoveSafe`/`MutatorShareSafe`；第12章可通过Send/Sync public projection查询同一事实。
- Weak 对应 AtomicWeak 或 owner kind 关联的 Weak。
- memory ordering 是 runtime contract，不由普通 Shared 路径动态选择。

禁止根据“对象后来是否跨线程”把同一个 Shared control 临时切换为原子模式。

## 7. Shared 强环

- Shared 不实现 cycle collector。
- 长期 parent/child/back-reference 图必须使用 Weak 打断反向边。
- 编译器诊断当前函数内可以证明的新强回边。
- 对 self field、双向 Shared field、容器 retain 等高风险模式给出 lint。
- 跨模块、dynamic container 和运行时条件形成的环不能被过程内分析完全消灭；文档必须明确可能泄漏。
- 需要任意强环的业务对象应使用普通 GC class，而不是 Shared。

QuickJS 同时具有引用计数和 remove-cycles GC 阶段，说明“引用计数自动解决循环”并不成立。ZR 选择保持 ownership world 无 cycle collector，以获得确定且较低的普通路径成本。

## 8. Borrow 与 owner receiver

owner type 通过通用 capability 提供：

```text
Unique<T>: DerefReadonly<T> + DerefWritable<T>
Shared<T>: DerefReadonly<T>
Weak<T>: no Deref; must upgrade
```

- `inspect(owner)` 在参数为 `in T` 时创建 shared reborrow，不消费 owner。
- writable method 在 Unique 上创建受限 mutable receiver loan。
- readonly method 在 Unique/Shared 上创建 shared receiver loan。
- 方法返回 ref 时，ref region 不得长于 owner borrow。
- owner auto-deref 必须由 capability/TypeDef contract 驱动，不能比较 wrapper 名字。

## 9. Drop

### 9.1 Drop contract

每个 move-only/resource type具有编译器可查询的 Drop contract：

```text
DropContract
  customDropFunction (optional)
  fieldDropPlan
  canThrow = false
  canSuspend = false
```

语言层可暂时继续使用现有析构声明语法承载 custom drop body；最终拼写不在本文重设计。语义必须统一为 Drop contract。

### 9.2 顺序

完整对象：

1. 标记对象进入 dropping，禁止新 upgrade/borrow。
2. 调用 custom drop body。
3. 按声明逆序 drop fields。
4. 释放 storage/control 的对应部分。

部分构造：

1. 不调用要求完整对象 invariant 的 custom drop body。
2. 按已初始化 bitmap 逆序 drop fields。
3. 释放 storage。

struct/resource inheritance存在时，derived fields/custom body 完成后再进入 base drop chain。

### 9.3 Drop 不得失败

- Drop 不能 async/await。
- Drop 不能把异常传播给调用者。
- 编译器拒绝声明为 throwing 的 Drop。
- native drop 若违反 no-throw contract，在 debug/host 边界产生 fatal structured error，不能在已有异常 unwind 时再次抛出。
- 需要报告关闭失败的资源提供显式 `close(): Result`，Drop 只做不可失败兜底清理。

### 9.4 Cleanup plan

normal return、early return、break、continue、throw 和 construction failure 都通过 CFG cleanup blocks 执行 Drop。runtime 不再维护一套与 compiler cleanup 顺序不同的隐式 owner stack。

`using` 的 Close 和 owner Drop 分开：Close 可以返回错误；Drop 不可失败。owner 本身不需要 using。

## 10. GC bridge

### 10.1 Ownership graph 持有 GC 对象：`Gc<T>`

```zr
resource class Request {
    let document: Gc<Document>;
}
```

`Gc<T>` 是显式 GC root handle：

- 只能指向普通 GC class。
- copy/drop 更新 root handle 生命周期。
- resource graph 不需要被 GC 递归扫描；GC 直接扫描 handle table 中的 target。
- GC compact 更新 handle target，不依赖 resource object 地址稳定。
- 写入/替换 Gc field 通过 root/barrier API。
- `Gc<T>` 不是 T 的 owner；T 的回收仍由 GC 决定。

### 10.2 把 resource 生命周期交给 GC：`GcBox<T>`

```zr
let resource: Unique<FileHandle> = own FileHandle(...);
let boxed: GcBox<FileHandle> = resource.intoGc();
```

规范语义：

- `.intoGc()` 第一版只允许 Unique。
- 调用消耗 Unique，active borrow 时非法。
- 返回 GC 管理的 `GcBox<T>`；box 最终不可达时执行 T 的 Drop。
- 这是显式放弃确定性释放，通常只用于兼容 GC API、缓存或宿主边界。
- 不承诺零拷贝、地址不变或原 resource storage 直接进入 moving GC。
- Shared 不提供 `.intoGc()`；多个 strong owner 无法无运行时争用地收敛为唯一 box。

### 10.3 GC 对象持有 resource

普通 class 不能直接声明 Unique/Shared/Weak field。需要时使用显式 `GcBox<T>` 或其他 bridge：

- 生命周期由 GC finalization 决定，成本和非确定性明确。
- GC metadata 标记该 field 的 finalization/drop contract。
- bridge 不允许形成“GC finalizer 等 Shared 环释放”的不可解释顺序。

### 10.4 Bridge 与 barrier

- `Gc<T>` handle create/update/drop 进入 GC root/barrier contract。
- GcBox finalization 与 GC sweep/remark 分阶段协调，不能在不安全阶段直接递归 GC。
- Drop body 中创建新 GC root 必须受正常 barrier 约束。
- native/host 保存 owner 或 GC target 必须使用对应 owner handle/Gc handle/pin handle，禁止裸指针跨 collection。

### 10.5 `zr.pooling` 作为唯一 owner

resource/entity pool 可以持有实体本身，只向调用方交付 `PoolHandle<T>` weak identity：

- pool/slab 是唯一 owner，handle copy 不增加实体 strong count。
- `tryBorrow/tryRead` 取得带 guard 的 `PoolRef<T>`/`PoolReadRef<T>`，不转移 owner。
- recycle 先使 handle generation 失效并进入 Retired；active guards 归零后才执行 T 的 Drop 和 slot reuse。
- pool drop 必须停止发布、retire 全部 live slot、等待/拒绝新guard，并保证每个initialized entity恰好Drop一次。
- individual `own class` allocation影响ownership heap/arena fragmentation，不是栈碎片；fixed-size slab/arena用于收敛该成本。

pool长期存活不自动允许GC跳过其内容。只有closed T的TypeLayout为`GcFree`时slab可NoScan；`GcMapped/GcBarriered`仍需要precise pointer map、barrier/card和remembered set。

## 11. GC domain 与多 mutator

### 11.1 Collection scope 由 domain 定义

`GcDomain` 是 runtime/host abstraction，不是第一版源码关键字、普通 class 或可反射构造的 public TypeDef。它至少拥有：

```text
GcDomainId + generation
collector/regions/allocation budget
mutator registry + precise root registry
remembered sets + write-barrier state
safepoint epoch + pause state
pause/memory/transfer telemetry
```

- 每个 GC object、GC handle、coroutine frame 和 movable root 都带可验证的 domain identity；object 从创建到回收不改变 domain。
- 一个 VM state/mutator 同一时刻只挂接一个 domain；一个 domain 可以挂接一个或多个 state/mutator。
- 宿主在创建 state/scheduler 时选择映射：全进程单 domain、每游戏实例一 domain、若干实例一组，或一个多线程实例独占一 domain 都合法。
- 语言规范不把“游戏实例”等同于 GC 隔离单元，也不强制每个 OS thread 拥有独立 heap。
- `collect/fullCollect` 只针对调用者当前 domain；不存在默认停止全部 runtime domain 的语言级全局 GC。

因此，同一可执行程序可以按部署成本选择 C# 风格的共享 heap，也可以选择 Lua/QuickJS 风格的隔离 runtime，还可以在多个 domain 内分别使用多 mutator GC。选择改变共享与传输成本，不改变类型安全规则。

### 11.2 Domain-local STW

第一版正确性基线是 **domain-local STW**，不是 process-wide STW：

1. collector 增加当前 domain 的 safepoint epoch 并阻止该 domain 新 mutator 进入运行态；
2. 该 domain 内每个 mutator在 allocation slow path、call/backedge、await/scheduler边界和native entry/exit轮询；
3. mutator发布精确stack/frame/handle roots、barrier buffer和当前执行状态后park；
4. collector只等待本domain的registered mutators，完成collection/relocation后更新epoch并唤醒；
5. 其他domain继续运行；它们只可能受共享OS资源、allocator或host lock竞争影响，不能被本domain safepoint协议直接暂停。

native/FFI descriptor必须声明 safepoint mode：可轮询的`GcAware`、发布roots后离开mutator集合的`BlockingDetached`，或有严格时限且禁止GC/裸指针逃逸的`NoSafepointCritical`。移动阶段若仍存在未pin的native interior pointer，collection必须等待或失败为structured host error，不能猜测更新裸地址。

收集阶段按成本分层：

- minor evacuation：domain-local STW；
- major mark：先实现incremental，再允许后台concurrent mark；
- remark/reference processing：短domain-local STW；
- compact：按region执行domain-local STW，宿主可按pause budget推迟；
- emergency/full collection：仍只停止当前domain，不能升级为隐式process-wide pause。

### 11.3 同域共享与跨域传输

同 domain 的普通 GC reference 可以零拷贝跨 state 使用，但跨 mutator move/share 分别还要满足本章底层的`MutatorMoveSafe`/`MutatorShareSafe` capability role；“指针同属一个 heap”不等于数据竞争安全。04只定义这两个runtime/artifact role，不新增public TypeDef；第12章再单向依赖本章，把它们投影为`zr.thread.Send`/`zr.thread.Sync`。

跨 domain 永远禁止普通 GC edge、`Gc<T>` target、async frame pointer、borrow/ref、`Span`、`PoolRef`和未声明的native pointer。跨域边界只接受由 Canonical TypeLayout/descriptor 计算的 `DomainTransferKind`：

| kind | 语义与成本 |
|---|---|
| `ValueCopy` | `GcFree`/无owner裸指针的closed value按布局复制 |
| `StructuredClone` | 显式可克隆GC graph按字段schema复制，保留payload内部alias/cycle，拒绝指向payload外部domain object的edge |
| `ImmutableHandle` | 指向domain外只读arena、mmap/blob或host asset；handle可复制，payload不可写且不进入moving heap |
| `ResourceMove` | 消耗`Unique<T>`并由注册的transfer/drop contract在目标domain重建owner；失败必须回滚或保持唯一owner |
| `Forbidden` | 默认；不生成transport，绑定或schedule时报定向diagnostic |

`DomainTransferKind` 是 artifact/runtime capability metadata，不新增一个用户可随意实现的marker。`StructuredClone`必须有稳定schema、对象/字节/depth quota和失败原子性；它不是隐式参数传值。跨域发送完成后两个heap之间仍不存在GC pointer，transport table也不能成为隐藏全局root registry。

大型共享只读资源优先放在moving GC之外，通过`ImmutableHandle`或线程安全resource handle访问。这样可避免每实例复制，又不把所有实例绑到同一次GC暂停。可变共享资源必须由显式并发native/resource contract保护，不能借`GcDomain`绕过`MutatorShareSafe`。

### 11.4 Barrier、handle 与失败语义

- allocation根据当前mutator的attached domain选择region；无attached domain时禁止分配GC object。
- GC field write barrier先验证source/target domain一致，再执行generational/concurrent barrier；跨域写入在debug和host boundary都必须失败，release构建不能静默接受。
- `Gc<T>`、pin、weak、Task completion和native handle保存`GcDomainId + generation`，避免domain销毁/复用后的ABA。
- domain shutdown先停止接收mutator和transport，drain/fault pending Task，再做本domain终结与回收；不能等待另一个domain中的GC object finalizer释放本domain资源。
- collector内部可并行使用worker thread，但collector worker不因此成为语言mutator，也不改变domain pause scope。

### 11.5 成本与可观测性

宿主选择domain拓扑时必须能读取每domain的heap bytes、allocation rate、minor/major/remark/compact pause、safepoint wait、active mutator、cross-domain bytes/object count和transport failure。验收必须覆盖三种部署：单共享domain、多隔离domain、每domain多mutator；不能只用单线程microbenchmark证明并发方案成立。

## 12. Runtime 操作

Semantic IR 至少需要：

```text
OwnConstruct
OwnerMove
OwnerShare
OwnerWeak
WeakUpgrade
OwnerDrop
GcHandleCreate/Copy/Drop
OwnerIntoGcBox
```

不再需要运行时：

```text
BorrowValue
LoanValue
ReturnLoanValue
source-slot-null-as-move-proof
hidden ownership-to-GC detach guessing
```

VM/AOT 可以对 Unique move/drop、known final Shared drop 做优化，但必须保留同一可观察 Drop 顺序。

## 13. Artifact 与 ABI

保存：

- TypeDef `resourceClass/hasDrop` flags。
- Owner TypeRef kind。
- Drop contract/function token。
- field ownership/GC-bridge map。
- `GcDomainId`/generation handle ABI、safepoint ABI 与 collector capability version。
- `MutatorMoveSafe`、`MutatorShareSafe`和`DomainTransferKind`的规范capability id；它们由closed TypeLayout/descriptor计算，不按类型名字符串识别。第12章的Send/Sync只绑定前两项，不反向拥有本章schema。
- structured clone schema、resource transfer token和immutable handle provider identity。
- callable move/borrow/escape contract。
- GcBox/Gc handle bridge kind。

跨模块绑定必须比较 owner kind、Drop contract hash、layout hash 和 bridge capability。不得只比较 target TypeDef 名字。

native ABI：

- Unique transfer、borrowed ref、Shared clone、Weak handle 必须分别声明。
- native callable必须声明`GcAware`、`BlockingDetached`或`NoSafepointCritical`，并给出pin/root/transfer contract。
- native 未声明 ownership 时默认不接管 owner，也不得保存 ref。
- native 返回 owner 时必须提供对应 destroy/drop callback/token。

## 14. 里程碑

### M1 resource/Unique + Drop

覆盖构造、move、scope drop、explicit drop、field drop、partial construction、异常 cleanup。

晋级门：无 borrow/shared/GC bridge 参与时，Unique 普通路径无隐藏 refcount；VM/AOT Drop 顺序一致。

### M2 Shared/Weak

覆盖 share、clone、drop、weak、upgrade、control lifetime 和强环 lint。

晋级门：last strong、many weak、drop-time upgrade、nested owner fields、异常路径完整；默认路径无原子操作。

### M3 owner borrow/receiver

覆盖 Unique mutable/shared borrow、Shared readonly borrow、ref return 和 two-phase receiver。

晋级门：全部冲突由 compile-time facts 拒绝；runtime Borrow/Loan 不再是正确性依赖。

### M4 domain identity 与单 mutator bridge

覆盖`GcDomain`创建/销毁、state attach/detach、object/handle domain identity、`Gc<T>`/`GcBox<T>`双向bridge、单mutator precise roots和跨domain write拒绝。

晋级门：GC压力和Drop期间collection下无漏标、悬空、double-drop；domain generation可检测stale handle；任何普通GC edge都不能跨domain；无隐藏ignore registry。

### M5 domain-local STW 与多 mutator

覆盖mutator registry、precise root publish、safepoint epoch/handshake、native safepoint mode、minor/remark/compact pause和同domain多mutator barrier。

晋级门：collector只等待当前domain mutator；其他domain持续推进；超时报告阻塞mutator/native frame；VM/AOT在相同root/barrier stress下结果一致。第12章的same-domain ThreadScheduler不得早于本gate晋级。

### M6 跨 domain transport

覆盖`ValueCopy`、`StructuredClone`、`ImmutableHandle`、`ResourceMove`和`Forbidden`，包含alias/cycle保持、quota、失败回滚及domain shutdown。

晋级门：所有跨domain payload都可由artifact schema复现；传输前后heap间无GC edge；resource move恰好一个owner；第12章的isolated-domain ThreadScheduler不得早于本gate晋级。

### M7 concurrent major + artifact/AOT/LSP

覆盖incremental/concurrent major mark、短remark、按budget compact、跨模块owner/domain/transfer contract、AOT helper、telemetry、hover/diagnostic。

晋级门：source/binary/VM/AOT行为一致；并发mark下barrier无漏标；pause与transfer指标可按domain归因；旧detach/borrow/loan opcode不再由新语法发出。

## 15. 测试矩阵

### Unique/Drop

- move assignment/parameter/return、use-after-move。
- normal/early/throw/break/continue cleanup。
- nested owner fields、reverse drop order。
- partial construction、custom drop、native drop failure。
- Option<Unique></unique> 和非 nullable invariant。

### Shared/Weak

- 1/N strong、0/N weak、repeated upgrade。
- final strong 与 Weak 观察。
- drop body 内 Weak upgrade 必须失败。
- process-local strong cycle 诊断/lint。
- Shared 跨线程拒绝；AtomicShared 后独立测试。

### Borrow

- Unique mutable/shared conflict。
- Shared writable receiver 拒绝。
- owner active Span/ref 时 move/share/drop 拒绝。
- ref return 不超出 owner。

### GC bridge

- Gc root create/copy/replace/drop。
- minor/major/compact 期间 target 保活和更新。
- GcBox 正常回收、显式仍可达、Drop 中分配。
- ownership/GC 双向图、native pin/handle。
- heap pressure、重复 collect、深层 bridge graph。

### GC domain/concurrency

- 单domain单mutator、单domain多mutator、多domain并行分配与回收。
- 一个domain执行minor/remark/compact/full GC时，其他domain的progress counter持续增长。
- safepoint命中allocation/call/backedge/await/native entry/exit，阻塞native可定位且不会静默跳过root。
- same-domain GC reference零拷贝但仍执行MutatorMoveSafe/MutatorShareSafe检查；跨domain普通pointer/ref/Gc handle全部拒绝；第12章另测Send/Sync public projection。
- StructuredClone保留payload内alias/cycle，拒绝外部edge并遵守object/byte/depth quota。
- ImmutableHandle并发读取、ResourceMove成功/失败回滚、domain shutdown与stale generation。
- concurrent mark期间mutator写屏障、weak/finalizer/GcBox/Drop组合压力。

### Performance

- Unique 构造/move/drop 与普通 malloc/free/RAII 对比。
- Shared clone/drop 非原子热循环。
- Weak upgrade 热路径。
- 大量 Gc handle 对 root scan 的成本。
- GcFree pool slab/owner图不进入普通 tracing 的收益，以及GcMapped/GcBarriered scan bytes和barrier成本。
- 单全局domain、每实例domain和分组domain在相同游戏负载下的p50/p95/p99 pause、throughput与内存冗余。
- domain内1/N mutator的safepoint wait、barrier/TLAB成本；跨domain clone bytes/objects和immutable handle收益。

## 16. 参考依据

- Rust Box/Rc/Weak/Arc：`lua/rust/library/alloc/src/boxed.rs`、`lua/rust/library/alloc/src/rc.rs`、`lua/rust/library/alloc/src/sync.rs`。
- Rust move/drop/borrow tests：`lua/rust/tests/ui/moves`、`lua/rust/tests/ui/drop`、`lua/rust/tests/ui/borrowck`。
- QuickJS refcount、Weak 和 remove-cycles：`lua/QuickJS-master/quickjs.c`。
- QuickJS runtime级heap/GC边界：`lua/QuickJS-master/quickjs.c`中的`JSRuntime`、`gc_obj_list`、`JS_RunGC`。
- Lua state/global state与incremental/generational barrier：`lua/src/lstate.h`、`lua/src/lgc.c`。
- .NET共享heap、后台GC与STW协调：`lua/runtime/src/coreclr/gc/gc.cpp`、`lua/runtime/src/coreclr/gc/gcpriv.h`。
- JDK G1 concurrent mark、safepoint与region collection：`lua/jdk/src/hotspot/share/gc/g1/g1CollectedHeap.cpp`、`lua/jdk/src/hotspot/share/gc/g1/g1ConcurrentMark.cpp`。
- CPython weakref/GC 对比：`lua/cpython/Objects/weakrefobject.c`、`lua/cpython/Modules/gcmodule.c`。
- ZR 当前 runtime：`zr_vm_core/include/zr_vm_core/global.h`、`zr_vm_core/include/zr_vm_core/ownership.h`、`zr_vm_core/src/zr_vm_core/ownership.c`、`zr_vm_core/include/zr_vm_core/type_layout.h`、`zr_vm_core/src/zr_vm_core/gc/gc.c`。

ZR 的刻意差异是：普通业务对象继续使用GC；resource class才进入Rust-like ownership。Shared第一版为单mutator非原子计数，不引入QuickJS式ownership cycle collector；跨世界通过Gc/GcBox明示成本。`GcDomain`把heap隔离范围和pause算法拆开：宿主可选共享或隔离domain，每个domain内部仍可使用local-STW与concurrent major的混合收集。
