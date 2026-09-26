---
related_code:
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/include/zr_vm_core/object.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/type_layout_initialization.c
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_layout_visibility.c
  - zr_vm_core/src/zr_vm_core/object/object_layout_map.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_objects_layout_maps.c
  - tests/core/test_object_shape_transition_cache.c
  - tests/core/test_type_layout_metadata_contracts.c
doc_type: milestone-detail
status: planned
---

# 05.01 对象 Shape 与内部布局映射

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 用固定 MemberId/descriptor index/layout offset 加速对象访问，并使内部物理布局与公开逻辑布局可追溯。

**Architecture：** TypeLayout 定义稳定外部 contract，内部 LayoutMap 表达 logical member 到 physical storage 的映射；shape guard 管理对象状态变化。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2/M3/M6。
- 前置：[04.01 Packed frame 与精确存储布局](../04-frame-native/01-frame-layout.md)；[03.02 成员链与调用 Binding facts 收敛](../03-interpreter-binding/02-static-binding-facts.md)。
- 交付：对象构造 shape 固定策略、内部 LayoutMap、getter/setter/反射 bridge。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

object shape transition 和 type_layout 已存在；不能重新按类名特判字段，也不能把 property 当无副作用的字段。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/type_layout.c` | 稳定布局与内部布局的查询边界 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/type_layout_initialization.c` | 构造时布局初始化 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/object.h` | shape/descriptor contract |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_layout_visibility.c` | 判断 reflection/FFI/地址逃逸可见性 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/object/object_layout_map.c` | 逻辑 MemberId 到内部位置映射 |
| 计划新增测试 | `tests/core/test_ssa_objects_layout_maps.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 审计可观察布局** 逐种记录 public ABI、反射枚举顺序、序列化字段、native offset、地址比较、union/显式布局和继承前缀；任何未证明封闭的对象不重排。

- [ ] **2. 固定构造阶段 shape** 批量分配字段并一次发布最终 shape；构造中外泄的 this 必须使用可观察的中间合法状态，不能先发布未初始化数据。

- [ ] **3. 接入 offset 快路** 静态字段经 TypeId/layoutHash/MemberId 得到 offset；动态对象继续 dictionary/prototype；property/meta 走 03.02 的 operation binding。

- [ ] **4. 设计布局 bridge** 内部字段重排需要 logical→physical 和反向 debug map；跨公开边界按稳定逻辑 layout materialize，bridge 成本计入收益。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
LayoutMapEntry = {memberToken, logicalOffset, physicalOffset, size,
                  alignment, gcFieldKind, ownershipKind}
canTransform = privateClosedWorld && !reflectionVisible && !ffiVisible
               && !addressEscapes && !serializationLayoutObserved
constructFields -> establishInitializedBits -> publishShape
access(member) -> validateLayout -> offsetLookup -> typedLoadOrStore
publicBoundary -> materializeLogicalLayoutUsingMap
```

constructor 可能抛异常，初始化 bitmap 决定清理范围；shape generation 变化使旧 offset proof 失效，不能只比较类型名称。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 同类型不同 shape、继承字段 | 正确 offset 或合法慢路 |
| 构造一半抛错且 this 可能外泄 | 仅初始化字段可读/可清理 |
| 反射/序列化/FFI 观察对象 | 稳定逻辑布局不变 |
| 新增动态字段 | dictionary 行为保留，旧 cache 被 guard 拦截 |

复用回归入口：`tests/core/test_object_shape_transition_cache.c`、`tests/core/test_type_layout_metadata_contracts.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_objects_layout_maps` 和可执行目标 `zr_vm_ssa_objects_layout_maps_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_objects_layout_maps_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_objects_layout_maps$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 对象/属性/反射的可见行为不变，布局变换每项有许可证明和 source/debug map。

**失败恢复：** 内部变换可放弃，公开布局不回写；未知动态 shape 使用已有合法动态对象路径。

**文档交付：** 新增 docs/core-runtime/object-layout-maps.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrObjectLayoutMap SZrObjectLayoutMap;
typedef struct SZrObjectMemberLocation SZrObjectMemberLocation;
TZrBool ZrCore_Object_ResolveLayoutMember(const SZrObjectLayoutMap *map,
    TZrMetadataToken member, SZrObjectMemberLocation *location,
    SZrObjectLayoutDiagnostic *diagnostic); /* core 自有诊断类型，由本任务定义；不引用 parser 侧 SZrExecIrDiagnostic */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| logical MemberId/layout | canonical metadata | public reflection/serialization 始终稳定 |
| physical LayoutMap | closed-world layout pass | static load/store 需匹配 hash/generation |
| initialized fields/shape | constructor runtime | 完整构造后发布，失败只清理已初始化字段 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现 visibility eligibility 与 logical/physical identity map，不立刻重排字段。

- [ ] **批次 2：** 优化构造发布和固定 offset 路径，测试 this 外泄及构造异常。

- [ ] **批次 3：** 加入内部重排和公开 bridge，检查反射、native、序列化观察一致。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange field reorder candidate visible to FFI
assert transformation blocked with ABI_VISIBLE
arrange constructor throws after initializing first two fields
assert only those owners dropped
arrange shape transition invalidates cached offset
assert guard rejects old offset before load
```

### 迁移结束检查

公开 layout hash 和内部 physical layout hash 分开；不能把内部优化导致的布局变化误当成允许修改公开 ABI 的热更。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

