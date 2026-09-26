---
related_code:
  - tests/CMakeLists.txt
  - tests/harness/runtime_support.c
  - tests/harness/reference_support.c
implementation_files:
  - tests/CMakeLists.txt
  - tests/harness/runtime_support.c
  - tests/harness/reference_support.c
  - tests/cmake/ssa-tests.cmake
  - tests/harness/ssa_differential_support.h
  - tests/harness/ssa_differential_support.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_differential_harness.c
doc_type: milestone-detail
status: planned
---

# 00.03 差分测试支架与里程碑门禁

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 搭建从低层 verifier 到多后端差分的共用验证入口，并给出可执行的阶段门禁。

**Architecture：** 新增独立 ssa CMake 测试清单；同一输入同时比较返回值、异常、可见副作用、drop、writeback 和调度事件，不只比较数值结果。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M0 验证基础；所有里程碑。
- 前置：[00.02 共享契约、语义边界与版本冻结](../00-measurement-contracts/02-contract-freeze.md)。
- 交付：统一 fixture、测试登记函数、命令和 baseline/implemented/accepted 状态定义。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

tests/CMakeLists.txt 已有 zr_vm_add_unity_test_target、共享 runtime harness 与 CTest 注册。原文件很大，应新增 ssa-tests.cmake 单点接入；已有模块 suite 继续负责回归。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `tests/CMakeLists.txt` | 只增加一个 include，避免重复登记 |
| 现有，修改/复用 | `tests/harness/runtime_support.c` | 复用 state 生命周期与 provider 初始化 |
| 现有，修改/复用 | `tests/harness/reference_support.c` | 复用语言输入与输出断言 |
| 计划新增 | `tests/cmake/ssa-tests.cmake` | 所有本目录计划新增测试的统一注册 |
| 计划新增 | `tests/harness/ssa_differential_support.h` | 定义返回、异常、效果与 drop 事件比较 |
| 计划新增 | `tests/harness/ssa_differential_support.c` | 生成相同输入并运行所选后端 |
| 计划新增测试 | `tests/core/test_ssa_differential_harness.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立 baseline 清单** 已有失败、环境缺失、未实现后端分别记录；当前工作区不干净，记录快照而不覆盖或吸收其他修改。

- [ ] **2. 加入可复用断言** 记录 result type/bit pattern、exception type/source、外部写入、析构顺序、inline writeback；浮点默认按语义规则精确比较，fast-math 用许可下的专属 oracle。

- [ ] **3. 登记各子计划测试** 每个叶子计划的新 test_ssa 文件对应一个精确 CTest 名；扩展用例在文件内登记，CTest 无匹配测试必须失败。

- [ ] **4. 定义渐进验收** M1 foundation 是 IR/verifier/oracle 可用；M1 全覆盖等待 07.02 的 C/LLVM parity。M0 runner 门禁等待 07.04。任何局部阶段通过不代替原索引完整门禁。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for fixture in corpus:
    baseline = run(ExecIR_oracle, fixture, optimization=off)
    for backend in enabledBackends:
        observed = run(backend, fixture, declaredProfile)
        assertEqual(baseline.semanticEvents, observed.semanticEvents)
        assertEqual(baseline.ownershipBalance, observed.ownershipBalance)
        assertSourceEquivalent(baseline.exception, observed.exception)
        recordActualModeAndFallback(observed)
    assert(fixture.requiredVariants == executedVariants)
```

状态统一 planned → implementing → focused-passed → accepted；accepted 需要本任务全范围和对应整层门禁。缺工具链是 unsupported/unavailable，不能用 stub 的成功替代平台通过。sanitizer 分开构建，不能混合 TSan/ASan。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 返回相同但 drop 顺序不同 | 差分失败 |
| 异常只在 AOT 被吞掉 | 差分失败并保存最小输入 |
| CTest regex 匹配 0 项 | 命令失败 |
| AOT fixture fallback 才成功 | 语义可通过，native coverage 不通过 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_differential_harness` 和可执行目标 `zr_vm_ssa_differential_harness_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_differential_harness_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_differential_harness$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 最小错误注入能被每一类断言捕获；精确注册名可列出；未经执行的计划测试不记为通过。

**失败恢复：** 新增 harness 不改变现有测试入口；新断言误报须先复查语言语义和 oracle，不能放宽为只比最终值。

**文档交付：** 新增 docs/testing-and-validation/ssa-differential-validation.md，并由 11.03 维护完整验收矩阵；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrSsaObservation SZrSsaObservation;
typedef struct SZrSsaFixture SZrSsaFixture;
TZrBool ZrTests_Ssa_RunFixture(const SZrSsaFixture *fixture,
    TZrUInt32 backend, SZrSsaObservation *observation);
TZrBool ZrTests_Ssa_Compare(const SZrSsaObservation *expected,
    const SZrSsaObservation *actual);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| fixture/expected events | 测试作者按语言规范编写 | 所有后端使用同一输入和外部初态 |
| semantic observation | instrumented runtime/oracle | 包含输出、错误、drop、写回和挂起事件 |
| coverage manifest | 各子计划维护 | expected variants 与 executed variants 必须一致 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先构建只含 oracle 与旧 ExecBC 的小 fixture，注入错误验证 harness 真能失败。

- [ ] **批次 2：** 将本目录 47 项测试注册集中化；CTest 名与源文件建立一对一清单，防重复/遗漏。

- [ ] **批次 3：** 逐步挂入 C/LLVM/JIT；不支持模式返回明确状态，不能制造空成功结果。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange expectedEvents=[get,write,drop], actualEvents=[get,drop,write]
assert differential comparison fails at index=1
arrange fixture requires four backends, executed only two
assert coverage incomplete even when two passed
arrange CTest selection matches zero tests
assert command exits nonzero
```

### 迁移结束检查

不要替换原有 suite 清单。最终 acceptance 同时列出新增 SSA suite 和既有回归；原有失败按快照记录，工具缺失不能混入 passed 总数。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。


## 构建和登记模板

以下为实施时增加的 CMake 模板，测试源由各叶子计划新增。函数体放 `tests/cmake/ssa-tests.cmake`，其 include 必须在既有 Unity helper 定义之后。本计划构建命令启用静态库，因此目标名称确定。

```cmake
function(zr_vm_add_ssa_test name source)
    set(target zr_vm_ssa_${name}_test)
    zr_vm_add_unity_test_target(${target} "${CMAKE_SOURCE_DIR}/${source}")
    target_link_libraries(${target} PRIVATE
        zr_vm_parser_static zr_vm_library_static zr_vm_core_static)
    add_test(NAME ssa_${name} COMMAND $<TARGET_FILE:${target}>)
    set_tests_properties(ssa_${name} PROPERTIES LABELS "ssa")
endfunction()
zr_vm_add_ssa_test(differential_harness tests/core/test_ssa_differential_harness.c)
```

WSL 内从仓库根初始化（这是未来实施命令，本次文档拆分不声称执行过）：

```bash
cmake -S . -B build/ssa-gcc-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF
cmake --build build/ssa-gcc-debug -j 4
ctest --test-dir build/ssa-gcc-debug -L ssa --output-on-failure --no-tests=error
cmake -S . -B build/ssa-clang-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF
cmake --build build/ssa-clang-debug -j 4
ctest --test-dir build/ssa-clang-debug -L ssa --output-on-failure --no-tests=error
```

另建 `build/ssa-clang-asan`，使用 `-DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"` 和 `-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"`；支持的平台启用 LSan。TSan 使用单独目录及 `-fsanitize=thread`。Valgrind/Helgrind 跑无 sanitizer 的 Debug 二进制，分别使用 `--error-exitcode=99` 和 `--tool=helgrind --error-exitcode=99`。

Windows 先在现有 Visual Studio 开发环境配置 `build/ssa-msvc-debug`，启用静态库，使用 `cmake --build build/ssa-msvc-debug --config Debug` 与 `ctest --test-dir build/ssa-msvc-debug -C Debug -L ssa --output-on-failure --no-tests=error`。多配置构建不要用 CMAKE_BUILD_TYPE 代替 --config。

性能使用独立 Release 目录，不能从 Debug/sanitizer 结果推断收益。所有原有 member/native/property/GC/ownership/AOT suites 在当前配置中用 `ctest -N` 列举后完整运行，实际测试总数写进 acceptance。
