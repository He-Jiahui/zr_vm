---
related_code:
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/object/object_inline_array.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_arrays_slices.c
  - tests/core/test_inline_struct_array_layout.c
  - tests/core/test_super_array_raw_int_canonical_storage.c
doc_type: milestone-detail
status: planned
---

# 05.02 Typed array、连续 Slice 与 Bounds

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 提供稳定 typed contiguous storage 与 slice contract，自动消除可证明冗余的索引开销。

**Architecture：** 统一 view 描述 owner/offset/length/stride/element layout 和 lifetime，dense array 内联/扩容优化通过 storage capability 暴露，编译器不识别具体容器类名。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2/M6；M7 数据前置。
- 前置：[05.01 对象 Shape 与内部布局映射](../05-data-layout/01-objects-layout-maps.md)；[02.02 别名分析、GVN 与范围证明](../02-automatic-optimization/02-gvn-range.md)。
- 交付：连续存储 descriptor、slice validity、小数组内联策略与 bounds lowering。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

object_super_array/object_inline_array 已有 raw storage；`zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c/.h` 已实现库层 contiguous view（loan liveness 语义见 docs/library-and-builtins/zr-container-contiguous-views.md），FFI 侧另有 pinned view（ffi_runtime_pointer_view.c，见 zr-pooling-and-pinned-ffi-views.md）。本任务把 view contract 下沉到 core 时必须以既有 lib_container 实现为迁移来源并收敛 FFI pinned view 语义，不并存两套 view 定义；优先适配现有布局而非新增互不兼容数组实现。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/object/object_super_array.c` | 复用 dense storage/扩容 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/object/object_inline_array.c` | 复用 inline struct 元素布局 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/type_layout.h` | 元素 stride/引用字段描述 |
| 现有，修改/复用 | `zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c` | 既有库层 view；迁移/适配到 core contract 后收敛为其消费者 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/contiguous_view.h` | 通用 view contract（以 lib_container 既有语义为迁移来源） |
| 计划新增 | `zr_vm_core/src/zr_vm_core/object/contiguous_view.c` | lifetime/shape/边界验证 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c` | 固定 stride 索引和 checked arithmetic |
| 计划新增测试 | `tests/core/test_ssa_arrays_slices.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义 view 生命周期** owner 受 GC 追踪；view 保存 offset 而非跨 safepoint 裸地址；可变借用、只读借用、跨 await、扩容失效分别编码，不能因为 readonly 就默认 storage 永远不变。

- [ ] **2. 实现 storage adapter** dense numeric、inline struct、小数组内联和 heap backing 统一返回 descriptor；容量复用必须清理失效元素及 remembered set，内联转 heap 更新 generation。

- [ ] **3. 实现索引投影** index*stride+base 先证明/检查整型溢出和范围；bounds hoist 依赖 length 在该 effect region 不变，变长数组跨未知 call 保留检查。

- [ ] **4. 连接 native/batch** ABI 匹配且 pin/lifetime 合法可直传；非连续或未知 alignment 走明确 copy/fallback，返回写回遵守 ref/out contract。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
View = {ownerRoot, byteOffset, length, stride, elementLayoutHash,
        mutability, storageGeneration, lifetimeRegion}
address = checkedAdd(base(ownerRoot), checkedMul(index, stride), byteOffset)
validate(index >= 0 && index < length)
validate(storageGeneration == owner.currentStorageGeneration)
acrossSafepoint: keep owner+offset, recompute raw address afterwards
```

slice 暴露顺序和 alias，不能偷偷转 SoA；连续性、alignment、non-overlap 是可证明的独立事实。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 空数组、负 index、最大 stride 乘法 | 正确边界/溢出错误 |
| view 活跃时数组扩容 | 拒绝非法别名或使 guard 失效 |
| inline struct 内部 GC 字段 | 扫描/写屏障完整 |
| native retained view 跨调用 | 按 lifetime contract 拒绝或复制 |

复用回归入口：`tests/core/test_inline_struct_array_layout.c`、`tests/core/test_super_array_raw_int_canonical_storage.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_arrays_slices` 和可执行目标 `zr_vm_ssa_arrays_slices_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_arrays_slices_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_arrays_slices$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** typed array/slice 在 GC、扩容、异常与 native 边界安全；消除 checks 需 proof 和失败反例。

**失败恢复：** 无证明时保留 bounds/shape 检查与通用 storage adapter，不变更语言索引规则。

**文档交付：** 新增 docs/library-and-builtins/contiguous-storage-contract.md，并更新既有 docs/library-and-builtins/zr-container-contiguous-views.md 与 zr-pooling-and-pinned-ffi-views.md 指向新 core contract，避免双份真相；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrContiguousView SZrContiguousView;
typedef struct SZrContiguousViewRequest SZrContiguousViewRequest;
TZrBool ZrCore_View_Create(const SZrContiguousViewRequest *request,
    SZrContiguousView *view, SZrViewDiagnostic *diagnostic);
TZrBool ZrCore_View_Validate(const SZrContiguousView *view,
    SZrViewDiagnostic *diagnostic);
/* SZrViewDiagnostic 为 core 自有诊断类型，由本任务定义；不引用 parser 侧 SZrExecIrDiagnostic */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| owner/offset/lifetime | view constructor 与 borrow facts | 保留 owner root，不长期保存裸 base |
| length/stride/storageGeneration | container storage adapter | bounds/offset/legalization 共同使用 |
| element GC/ownership map | TypeLayout | resize/store/scan 都覆盖 inline reference 字段 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 将现有 super/inline array 接到统一 descriptor，保持原存储实现。

- [ ] **批次 2：** 实现 view 的生命周期和 resize/pin 规则，测试跨 GC/await/native 使用。

- [ ] **批次 3：** 接 bounds/stride 优化、小数组内联策略和容量复用，计入 GC barrier 与搬迁成本。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange view over inline buffer, then resize moves storage
assert borrowed mutation forbidden or view invalidated as contract requires
arrange index*stride overflow
assert reject before address computation
arrange array shrink removes owned elements
assert drop and remembered-set cleanup exact
```

### 迁移结束检查

small-array inline 不是始终更快，门槛应可测可调。任何提取 raw pointer 的 helper 明确只在无移动区间或已 pin 状态使用。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

