# AOT 09：内存管理与布局驱动优化

## 分配类别

| 类别 | 主要位置 | 必要contract |
|---|---|---|
| scalar/短期struct | register/stack | escape、size/alignment、Drop |
| GC class/array | GC heap | root、safepoint、pointer map、barrier |
| resource/Unique | ownership heap/arena | move、Drop、bridge |
| closure/environment | GC或明确owner storage | capture layout、generation |
| pooled fixed-layout T | `zr.pooling` slab | StableSlot、generation、GcScanKind |

## 规则

- stack promotion只在escape facts证明对象、ref和destructor语义均安全时发生。
- scalar replacement需要保留property/ref observable behavior与Drop顺序。
- write barrier由field/layout/heap relation决定；不能按“pool/old object”全局跳过。
- NoScan只对closed `GcFree` TypeLayout成立；GcMapped/GcBarriered slab继续参与GC contract。
- PoolHandle可长期流转且不保持实体强引用；PoolRef单次验证后hot access不重复generation check，但active guard阻止slot reuse。
- own class/resource分配进入ownership heap/arena，不称为“栈碎片”；性能评估区分heap fragmentation、cache locality与GC scan。
- pin只用于明确native/interop window，并具有scope cleanup；不能长期默认pin所有连续容器。

## 优化与测量

测量allocation count/bytes、peak live bytes、GC root/scan bytes、barrier/card、Drop、pool reuse、fragmentation、cache miss与pause。任何“更快”结论必须指明输入、backend、target和未改变的语义检查。

## 完成记录

[2026-06-25 GC/AOT memory baseline](./09-memory/2026-06-25-gc-aot-memory-baseline.md) 记录已有GC map/bridge基础；pooling与新TypeLayout分类仍按syntax 03/04/09实施。

## 分阶段计划

输出/交付包括allocation classification annotation、root/barrier maps、pool slab/guard contract、优化proof记录与分项性能报告。

1. **A9.1 allocation classification**：根据escape、TypeLayout、DropKind把value映射到register/stack/GC heap/ownership heap/pool；分类结果写入ExecIR annotation并可审计。
2. **A9.2 root/barrier maps**：生成frame/global/inline aggregate roots和field barriers，跨module layout hash不一致时拒绝加载。
3. **A9.3 stack promotion/SROA**：仅在no-escape、no-observable-identity、Drop顺序等proof齐全时启用；debug模式可关闭并比较行为。
4. **A9.4 pooling**：实现StableSlot、generation、reader/writer guard、retire/deferred reuse以及GcFree/GcMapped/GcBarriered slab。
5. **A9.5 measurement**：统一采集allocation/bytes、root/scan、barrier/card、pause、fragmentation、pool validate/reuse和cache locality。

证据从`tests/gc/gc_tests.c`、`tests/core/test_type_layout_metadata_contracts.c`、`tests/acceptance/2026-06-24-aot-09-s1a-gc-descriptor-offsets.md`、`tests/acceptance/2026-06-25-aot-09-s2a-gc-root-map-descriptor.md`和GC fragment benchmark开始。新增pool测试必须证明stale/ABA永久失效、active guard阻止reuse且hot field access无重复generation check。

退出条件：优化开关不改变语义；NoScan只由closed GcFree layout产生；compacting GC、native pin和resource Drop压力通过；性能报告按类别归因而非只报告总耗时。
