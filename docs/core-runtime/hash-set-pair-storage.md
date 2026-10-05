---
related_code:
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/hash_set.c
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - tests/core/test_hash_set_dense_paths.c
  - tests/CMakeLists.txt
  - tests/harness/runtime_support.c
  - zr_vm_core/src/zr_vm_core/memory.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - tests/third_party/zr_unity/Unity/src/unity.c
  - tests/third_party/zr_unity/Unity/src/unity_internals.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/hash_set.h
  - zr_vm_core/src/zr_vm_core/hash_set.c
plan_sources:
  - user: 2026-10-04 caller-first comment review of the dense hash-set fixture
  - user: 2026-09-05 repair the pooled HashSet removal prerequisite exposed by compiler integration
  - docs/plans/ssa/05-data-layout/03-maps-strings.md
tests:
  - tests/core/test_hash_set_dense_paths.c
  - tests/parser/test_compiler_regressions.c
  - tests/acceptance/2026-09-05-hash-set-pooled-removal.md
doc_type: module-detail
---

# Hash Set Pair Storage

## Purpose

`SZrHashSet` supports individually allocated key/value pairs and pairs carved
from `SZrHashPairPoolBlock` allocations. Native container arrays use pooled
pairs for dense append paths, so both allocation forms can occur in one set.
Removal and teardown must distinguish the two forms.

## Allocation Ownership

`ZrCore_HashSet_Add` allocates an independent pair. Pool reservation allocates
a block containing its header and pair array; `TakeReservedPair` returns an
interior pointer into that array. Only the block base is a valid allocator
deallocation target for pooled storage.

The private `zr_hash_pair_pool_contains` helper classifies a pair against the
set's pool block ranges. Both inline removal and final deconstruction use this
same check. It adds no public function or serialized metadata contract.

## Removal and Teardown

Removal unlinks the matching pair, decrements `elementCount`, and returns its
key as a shallow copy of the node's key slot. Standalone pairs are freed
immediately. Pooled pairs are not individually freed; their storage stays
owned by the set until deconstruction releases the complete pool blocks.

For a reference-counted owned key, the returned value carries the reference
already held by the pair. Removal does not retain or release that reference;
the caller receives it and must call `ZrCore_Ownership_ReleaseValue` once when
finished. This transfer contract does not define ownership teardown for
surviving entries: current deconstruction releases pair storage but does not
release key/value contents. Ownership cleanup for a nonempty set remains a
separate unresolved contract and is outside this Remove behavior.

The pool's `used` counters are allocation cursors, not live-element counts.
Removing a pooled pair does not decrement these cursors or introduce cell
reuse.

Deconstruction walks remaining bucket chains, frees their standalone pairs,
then frees every pool block once and releases the bucket array. Removed
pooled cells are therefore reclaimed even when no bucket still references
their block.

## Validation

`test_hash_set_dense_paths.c` covers pooled-only, standalone-only, and mixed
collision chains. A wrapper around the real runtime allocator records exact
hash-pair allocation bases and sizes. It rejects interior, duplicate, or
size-mismatched frees, checks that standalone removals free immediately, and
checks balanced allocation/free counts after deconstruction. Repeated removal
of the same key must remain a miss without changing the count. The owned-key
case initializes an ordinary managed object as unique, checks that Add stores
a retained shared key, then verifies Remove transfers that reference without
changing the count; releasing the original owner leaves the returned key
alive until the caller releases it.

### 当前 fixture 入口、寿命与场景范围

`tests/CMakeLists.txt:412` 将本文件编入 Unity target；`:414` 注册
`hash_set_dense_paths`，`:415` 指定该 executable，`:416` 标记 `core`。
当前 runner 的五个 `RUN_TEST` 分别检查空集合的容量/阈值、全池节点、全独立节点、
混合来源碰撞链和 owned-key 转移。注册存在及源码断言不能作为本次候选已执行的证明。

观察 allocator 在 VM 创建完成后、集合构造前安装：fixture 的
`tests/core/test_hash_set_dense_paths.c:112` 创建 VM，`:120` 替换
`global->allocator`，`:121` Construct，`:122` Init。原 allocator 的
`userData`、字节数与类别透传；只观察 `HASH_PAIR`，不模拟 OOM。三键
0/8/16 在八桶内碰撞，按中间、尾、首节点移除，分别区分独立节点即时释放和
池块延迟整块释放。八条记录足以覆盖这三个键产生的至多三个块。

`tearDown` 先恢复 allocator，再清理仍在 owned-key 集合桶链上的 key/value，
最后回收节点/桶存储与两个已初始化的静态值槽并销毁 VM。`Remove` 已摘除节点
转交的引用由返回槽负责，不能再从 surviving 节点清理中释放。静态状态和发布
标志使断言跳转后的清理能取得对应资源；它们没有延长函数内局部 set 的寿命。

稠密增长场景只有空集合 `Init(log2=3)` 的八桶变为 `Grow(16)` 的十六桶，
断言容量和 `resizeThreshold` 都是16。没有插入 dense key，因此该场景不证明
旧元素保留、真实 append、重排或 GC 行为。owned-key 场景只验证普通 class 对象
unique→节点 shared 的 retain 与 Remove 的既有引用转移，以及分别释放后的槽清空；
不证明非空集合析构会释放值，也不证明最终 GC 已发生。

### 失败清理：有限静态证据

空集合增长用例保留 `BUG:`：合法 VM/Init 成功后，宿主 `realloc` 可合法返回
NULL 并保留旧桶；`hash_set.c:254` 返回 false，fixture
`tests/core/test_hash_set_dense_paths.c:283` 的 Unity 断言失败。
当前 Unity config 没有覆盖 `TEST_ABORT`，`unity_internals.h:891` 的 longjmp
跳过用例尾部清理，runner 随后调用 `tearDown`。此用例的 state/set 都在局部，
未进入可供 teardown 取得的静态状态，故已取得的旧桶及 VM 资源缺少清理入口。
这是合法可达的静态证明，没有执行 OOM 故障注入；VM 创建失败前无已取得 VM，
不能据此同样宣称泄漏。

removal helper 保留具体 `TODO:`：局部 set 不由 `tearDown` 持有，但节点/池块
最终 OOM 经过 `GcMalloc` 的回收重试与 `Exception_Throw`，可能进入 VM panic/abort。
当前三个合法场景没有证明该失败返回 Unity 断言。下一步需沿合法失败注入和异常
入口核实清理路径，不能把空集合 realloc 返回 false 的路径移植成节点 OOM 的 BUG。
本次注释整合未运行 native/build/test，不更改历史验证结果。

### 历史上层回放记录

The compiler integration fragmentation benchmark is the upper-layer replay:
native array removal reaches this hash-set contract after pooled string
appends. The defect already existed in baseline `c95e5387`; it is independent
of pending Shared/Weak control cleanup.
