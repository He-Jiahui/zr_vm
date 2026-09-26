---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/aot_typed_call_binding.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
implementation_files:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/aot_typed_call_binding.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_call_plan.c
  - zr_vm_core/include/zr_vm_core/native_call_contract.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/ffi/test_ssa_native_abi.c
  - tests/ffi/test_ffi_module.c
  - tests/library/test_call_binding_native_registry.c
  - tests/core/test_native_inline_span_dispatch.c
doc_type: milestone-detail
status: planned
---

# 04.03 Native、FFI 与跨后端桥接

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 统一 native/FFI 与 VM/AOT/JIT 的 typed call ABI，消除重复 marshalling 和名称查找。

**Architecture：** 签名生成 marshal plan 与 root/pin/callback lifetime contract；直接 cached callback 仅在 ABI 完全匹配时使用，其他情况显式 bridge。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2/M4。
- 前置：[04.02 调用返回、结果转发与尾调用复用](../04-frame-native/02-call-return-tail.md)。
- 交付：NativeCallPlan、native callback 生命周期、线程 attach 与跨后端 bridge。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

已有 native_binding、aot_typed_call_binding 和 cached lanes；native_binding.h 的 capability flags 是类型/FFI 功能元数据，不等于 08.02 的权限白名单。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_library/include/zr_vm_library/native_binding.h` | 复用 signature/layout 注册 |
| 现有，修改/复用 | `zr_vm_library/src/zr_vm_library/aot_typed_call_binding.c` | 复用 AOT typed call bridge |
| 现有，修改/复用 | `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_invoke.c` | 复用实际 FFI 调用与 marshalling 入口，连接统一 NativeCallPlan |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 共享 frame/native ABI |
| 计划新增 | `zr_vm_library/src/zr_vm_library/native_binding/native_call_plan.c` | 一次建立 marshal/pin/cleanup 计划 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/native_call_contract.h` | 回调保留、异常策略、thread attach 描述 |
| 计划新增测试 | `tests/ffi/test_ssa_native_abi.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 列 ABI 分类** 标量、inline struct、contiguous view、typed array、ref/out、变参、callback 分别定义 byte layout、对齐、ownership 与保留规则；无法直接匹配则 copy/marshal。

- [ ] **2. 建立加载期计划** token/signature/layout hash 验证后生成 plan，native lane 使用 callback ID 解析出的运行期地址，跨进程重载必须重新解析。

- [ ] **3. 规范 GC 与异常** native 可见数据 pin 或放 non-moving 区；native 调用期间允许回调 GC 时注册精确 roots，C++/外部异常须在 ABI 边界转换，不能跨 C frame 任意 unwind。

- [ ] **4. 规范线程与生命周期** 外部线程先 attach domain；callback unregister 等待 in-flight 结束，retained 参数需明确可延长生命周期，未知保留行为按 escape 处理。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
NativeCallPlan = {signatureHash, layoutHash, marshalOps, pinOps,
                  rootMap, exceptionPolicy, threadPolicy, callbackLifetime}
validateContract -> attachIfRequired -> publishVmState -> pinAndRoot
invokeResolvedCallback -> translateException -> unpinAndCleanup -> reloadVmState
ABICompatibleSpan -> directPointerForCallDuration
otherwise -> explicitTemporaryCopyWithDefinedWriteback
```

typed signature 并不自动允许指针跨 GC；直接 span 传递必须保证调用期地址稳定。权限检查位于同一 bridge，不能因 cached lane 而跳过。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 匹配/不匹配 struct ABI | 分别直传/显式 marshal |
| native 回调中 GC 和嵌套 VM call | 无悬空引用 |
| callback 注销与并发进入 | 已进入调用完成，之后拒绝 |
| 异常、ref/out 写回、未 attach 线程 | 遵守统一错误及清理协议 |

复用回归入口：`tests/ffi/test_ffi_module.c`、`tests/library/test_call_binding_native_registry.c`、`tests/core/test_native_inline_span_dispatch.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_native_abi` 和可执行目标 `zr_vm_ssa_native_abi_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_native_abi_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_native_abi$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** VM/C/LLVM native 调用共用描述；零额外名字查找，pin/root/异常/线程行为齐备。

**失败恢复：** 不能直传时走安全 marshal，不把 ABI 不匹配伪装成可直接调用。

**文档交付：** 新增 docs/library-and-builtins/native-ffi-contract.md（该文档当前不存在），并与既有 zr-pooling-and-pinned-ffi-views.md 交叉链接；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrNativeCallPlan SZrNativeCallPlan;
typedef struct SZrNativeCallRequest SZrNativeCallRequest;
TZrBool ZrLibrary_NativeCall_Prepare(const SZrNativeCallRequest *request,
    SZrNativeCallPlan *plan, SZrNativeCallDiagnostic *diagnostic);
TZrBool ZrLibrary_NativeCall_Invoke(const SZrNativeCallPlan *plan,
    struct SZrState *state, SZrNativeCallDiagnostic *diagnostic);
/* SZrNativeCallDiagnostic 为运行期诊断类型，由本任务定义；不引用 parser 侧 SZrExecIrDiagnostic */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| signature/marshal plan | native registry 加载期 | fast lane 不重复分析 ABI |
| pins/roots/thread attach | 统一 native boundary | 进入前登记、所有退出路径对称释放 |
| callback in-flight leases | callback registry | unregister 等待已进入调用，不释放被执行 context |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 列出现有 native/FFI/AOT bridge 的参数分类和差异，先统一描述再减少 marshalling。

- [ ] **批次 2：** 接 ABI-compatible direct span 与不匹配 copy path，测试 alignment/struct return/ref-out。

- [ ] **批次 3：** 做重入、跨线程 attach、GC、异常和 unregister race；记录每条清理路径资源平衡。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange native calls back into VM and forces moving GC
assert callback context rooted and pinned input unchanged in address
arrange marshal failure on argument N
assert previous temporary buffers and pins released exactly once
arrange unregister races with in-flight callback
assert new entry rejected and existing call completes safely
```

### 迁移结束检查

native fast lane 不得跳过 capability 或 thread policy。跨平台 ABI helper 不复制进 C 和 LLVM emitter 两处，统一 native contract 是唯一真相。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
