# 04 resource class、Unique/Shared/Weak、Drop 与 GC bridge

> 状态：细化草案，等待人工确认。
>
> 硬依赖：[Canonical TypeRef/Place/CFG](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[borrow checker](./2026-07-18-02-reference-syntax-borrow-checker-design.md)、[TypeLayout/ref struct](./2026-07-18-03-struct-ref-struct-span-layout-design.md)

## 1. 目标结果

建立一个日常可用、静态安全且性能成本可预测的 ownership world：

- `resource class` 明确表示确定性生命周期类型。
- `Unique<T>` 表示唯一 owner，move 后源静态失效。
- `Shared<T>` 表示线程域内非原子共享 owner。
- `Weak<T>` 观察 Shared 生命周期，不延长对象存活。
- shared/mutable borrow 分别由 `ref readonly T`/`ref T` 表示，不再创建运行时 Borrow/Loan wrapper。
- Drop glue 统一正常退出、早返回、异常和部分构造清理。
- GC 与 ownership world 通过显式 bridge 交互，不使用隐藏 ignore registry 猜测生命周期。
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
- target 满足 Send/Sync 或等价 capability。
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

## 11. Runtime 操作

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

## 12. Artifact 与 ABI

保存：

- TypeDef `resourceClass/hasDrop` flags。
- Owner TypeRef kind。
- Drop contract/function token。
- field ownership/GC-bridge map。
- Send/Sync/isolation capability。
- callable move/borrow/escape contract。
- GcBox/Gc handle bridge kind。

跨模块绑定必须比较 owner kind、Drop contract hash、layout hash 和 bridge capability。不得只比较 target TypeDef 名字。

native ABI：

- Unique transfer、borrowed ref、Shared clone、Weak handle 必须分别声明。
- native 未声明 ownership 时默认不接管 owner，也不得保存 ref。
- native 返回 owner 时必须提供对应 destroy/drop callback/token。

## 13. 里程碑

### M1 resource/Unique + Drop

覆盖构造、move、scope drop、explicit drop、field drop、partial construction、异常 cleanup。

晋级门：无 borrow/shared/GC bridge 参与时，Unique 普通路径无隐藏 refcount；VM/AOT Drop 顺序一致。

### M2 Shared/Weak

覆盖 share、clone、drop、weak、upgrade、control lifetime 和强环 lint。

晋级门：last strong、many weak、drop-time upgrade、nested owner fields、异常路径完整；默认路径无原子操作。

### M3 owner borrow/receiver

覆盖 Unique mutable/shared borrow、Shared readonly borrow、ref return 和 two-phase receiver。

晋级门：全部冲突由 compile-time facts 拒绝；runtime Borrow/Loan 不再是正确性依赖。

### M4 `Gc<T>`/`GcBox<T>`

覆盖双向 bridge、GC compact、barrier、finalization、native handles。

晋级门：GC 压力和 Drop 期间 collection 下无漏标、悬空、double-drop；无隐藏 ignore registry。

### M5 artifact/AOT/LSP

覆盖跨模块 owner/Drop/bridge contract、AOT helper、hover/diagnostic。

晋级门：source/binary/VM/AOT 行为一致，旧 detach/borrow/loan opcode 不再由新语法发出。

## 14. 测试矩阵

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

### Performance

- Unique 构造/move/drop 与普通 malloc/free/RAII 对比。
- Shared clone/drop 非原子热循环。
- Weak upgrade 热路径。
- 大量 Gc handle 对 root scan 的成本。
- GcFree pool slab/owner图不进入普通 tracing 的收益，以及GcMapped/GcBarriered scan bytes和barrier成本。

## 15. 参考依据

- Rust Box/Rc/Weak/Arc：`lua/rust/library/alloc/src/boxed.rs`、`lua/rust/library/alloc/src/rc.rs`、`lua/rust/library/alloc/src/sync.rs`。
- Rust move/drop/borrow tests：`lua/rust/tests/ui/moves`、`lua/rust/tests/ui/drop`、`lua/rust/tests/ui/borrowck`。
- QuickJS refcount、Weak 和 remove-cycles：`lua/QuickJS-master/quickjs.c`。
- Lua incremental/generational barrier：`lua/src/lgc.c`。
- CPython weakref/GC 对比：`lua/cpython/Objects/weakrefobject.c`、`lua/cpython/Modules/gcmodule.c`。
- ZR 当前 runtime：`zr_vm_core/include/zr_vm_core/ownership.h`、`zr_vm_core/src/zr_vm_core/ownership.c`、`zr_vm_core/include/zr_vm_core/type_layout.h`。

ZR 的刻意差异是：普通业务对象继续使用 GC；resource class 才进入 Rust-like ownership。Shared 第一版为线程域内非原子计数，不引入 QuickJS 式 ownership cycle collector；跨世界通过 Gc/GcBox 明示成本。
