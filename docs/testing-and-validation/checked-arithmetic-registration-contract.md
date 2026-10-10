---
related_code:
  - CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/cmake/checked-divide-tests.cmake
  - tests/cmake/checked-multiply-tests.cmake
  - tests/core/test_execution_checked_divide.c
  - tests/core/test_execution_checked_multiply.c
  - tests/third_party/zr_unity/CMakeLists.txt
  - zr_vm_common/ThirdPartyMacros.cmake
implementation_files:
  - tests/cmake/checked-divide-tests.cmake
  - tests/cmake/checked-multiply-tests.cmake
plan_sources:
  - "user: 全工作区调用方优先审查、宏观契约注释和分阶段提交"
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_execution_checked_divide.c
  - tests/core/test_execution_checked_multiply.c
doc_type: testing-guide
---

# Checked integer 独立测试入口

这两个 CMake 片段把手工构造 Core 函数、指令和 VM 状态的 Unity 夹具接入父工程。它们复用公共测试支持，并为测试提供 Core 私有 checked integer 头的可见性。算术契约与既有验证范围见 [checked integer 模块文档](../core-runtime/checked-integer-arithmetic.md)。

## 调用链与输入

顶层 `BUILD_TESTS` 为真时进入 `tests/`。父配置的 `tests/CMakeLists.txt:284` include multiply 片段，`:8807` include divide 片段；这两处是当前的实际配置调用入口。

| 片段 | 夹具 | CTest 名称 |
| --- | --- | --- |
| `checked-divide-tests.cmake` | `test_execution_checked_divide.c` | `execution_checked_divide` |
| `checked-multiply-tests.cmake` | `test_execution_checked_multiply.c` | `execution_checked_multiply` |

每个片段先检查 Core shared 或 static 目标是否存在。父级 `zr_vm_add_unity_test_target`（`:63`）加入对应 C source、runtime/path/crash guard/reference harness、Unity 依赖和公共编译设置。Unity third-party 目标必须已经注册；`zr_link_third_party_for_target` 在缺失注册时以配置错误结束。

`CMAKE_SOURCE_DIR` 必须是仓库根。私有 include 指向 `zr_vm_core/src/zr_vm_core`，因为两个夹具直接 include `execution/execution_checked_integer.h`。这条可见性只对测试目标生效。

`zr_vm_link_core`（`:180`）按 `BUILD_SHARED_LIB` 选择链接 shared 或 static；目标存在性检查与该选项必须保持一致。片段不负责创建 Core 目标，也不替父工程选择构建变体。

## 注册效果与执行边界

Core 目标存在时，各片段建立一个独立可执行目标和一个 CTest 入口；缺少两种 Core 目标时不注册。每个 `main` 注册八个现有 Unity 场景，使用 `UNITY_END()` 将失败数作为进程返回状态。实际运行入口之后才能产生测试结果，配置完成不代表夹具已编译、链接或通过。

`LABELS "ssa"` 用于 CTest 标签筛选。夹具直接操作 Core，未经过 parser、AOT 或 SSA 代码生成；标签本身不提供那些上层路径的覆盖证明。`TIMEOUT 120` 限定此测试命令的最长运行时间，不代表算术正确性、性能承诺或完整覆盖。

## 本轮注释核对范围

本轮登记两个配置入口契约和两个标签/超时语句块。原有配置的每条物理行、空白和换行均保留；两个 main 的旧台账引用按原 `add_test` 语句迁移行号。

配置比较使用当前父助手、当前 Unity 注册文件及这两个实际片段，分别观察 shared、static 和无 Core 的注册输出。私有 Core imported 目标只提供配置上下文，因此该比较只验证目标 source/include/define/link 属性、CTest 命令、标签和超时的前后一致性；不授予 Core 或 Unity 的完整构建、夹具运行或上层 SSA 信用。实际命令和终态结果分别保存在本批验证记录中，未运行的范围继续保留。
