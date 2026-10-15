---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_array_lowering.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
  - zr_vm_core/include/zr_vm_core/contiguous_view.h
  - zr_vm_core/src/zr_vm_core/object/contiguous_view.c
  - tests/core/test_ssa_arrays_slices.c
  - tests/cmake/ssa-tests.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_array_lowering.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c
  - tests/core/test_ssa_arrays_slices.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
  - docs/code-review/comment-standard.md
tests:
  - tests/core/test_ssa_arrays_slices.c
  - tests/cmake/ssa-tests.cmake
doc_type: code-review-batch
---

# Parser ExecIR 数组下标契约审查 — 2026-10-15

## 范围与源码身份

本批覆盖公开数组下标 facade、单行转发实现及定向 SSA 边界测试。实际源码注释只改公开头文件和测试；Core 连续视图的 25 个既有单元由单独批次维护，不在本批计数。API、断言、程序 token、CMake 表达式和行为均不变，不新增测试或源码副本。负责人仅更新 inventory 中这三个已完整审查文件的哈希、状态与说明，保留其他工作树差异。

租约固定的编辑前 SHA-256 如下：

| 路径 | 编辑前 SHA-256 | 编辑后 SHA-256 |
| --- | --- | --- |
| `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_array_lowering.c` | `9b9abf061e008973b3d35f3ca542c1c9257bdf838b13f0b60ac134fe60c2d3cc` | 未变 |
| `zr_vm_parser/include/zr_vm_parser/exec_ir_array_lowering.h` | `392c4e93d9fc7e1b8bd802f4b324d35e186da6cf97967748dbeafca21d4fc441` | `68c92c98ca28b39539ea1379c2ff14bf30e0f7bc28719eb8208b8c5d04908a6a` |
| `tests/core/test_ssa_arrays_slices.c` | `ae6f4acc9f1dfc0ea4ed286c380d20e001a0876b2e5f9c6f53ec765a65b25585` | `6e2051a1d97a8d2a5718ddcbc18240a9d0c71aec89481db3cbdab34b50aef9e2` |

台账包含 11 个叶单元：转发 C 函数、公开头文件接口、七个测试函数、`assert_view_equal` 和 `main`；没有复制 Core 单元。7 个单元附详细注释（公开头文件及六个测试函数）；转发 C 函数、局部相等 helper、直接测试 Core 空输出的函数和 `main` 标记为 `no-comment`。本 Parser 批次没有新增 BUG/TODO；相关 Core 单元保留已有 TODO（包括 `IndexOffset` 未接收真实 generation），不重复登记。

## 契约证据

`ZrParser_ExecIr_LowerArrayIndex` 只调用 `ZrCore_View_IndexOffset`；成功结果是相对 owner 的字节偏移，不是元素指针，也不执行访问。头文件在 `#endif` 后以 EOF Doxygen 记录 `view` 与可选 `diagnostic` 在同步调用期间借用、`offset` 仅成功时有效、调用方须保证非空输出存储不与 view 或另一个输出重叠，并说明该接口不负责 owner rooting、pin、当前代数获取、backing 范围校验或访问权限。调用方若把成功偏移用于真实存储访问，仍须维持 owner 及所需 borrow/pin，并独立核对 generation、范围和权限；失败时 offset 不可用。原头文件 1–11 行程序坐标保持不变。

定向测试以 `(uintptr_t)1u` 作为非空元数据标记。Core Create、Slice、Validate 和 Parser facade 在这些路径中不解引用该值，因此测试只证明描述符算术和诊断。七个测试函数覆盖 `SIZE_MAX` 处有效的空尾切片、乘法与加法溢出、等于长度的下标、空 facade 输出、空 Core Slice 输出，以及普通边界和溢出的组合情形。测试不运行生产 ExecIR/AOT 发射或真实数组存储，也不提供 GC root、pin 或长期借用证据。六个 EOF `@fn` 块记录这些范围；两条原英文算术说明原位改为中文；第 124 行同时纠正旧误述：当前输入在 byteOffset 加末元素跨度时已溢出，未走到 elementSize 相加。原程序物理行不变。

`zr_vm_parser.h` 包含该公开 facade。`tests/cmake/ssa-tests.cmake:1108-1119` 注册 `zr_vm_ssa_arrays_slices_test` 并添加 CTest `ssa_arrays_slices`。现有 Core coverage 已负责 `ZrCore_View_IndexOffset`、诊断分支和连续视图生命周期契约；本台账引用这些路径但不把它们算作 Parser 单元。

## 基线与验证边界

注释前的授权原生基线构建成功（exit 0，8.765 秒），CTest `ssa_arrays_slices` 通过 1/1（exit 0，2.469 秒）。固定证据为 `parser-execir-array-index-preedit-native-r1-results.json`（SHA-256 `d7de1f969bf9530ae62e88f0c8e086588e757f89ed6c971ab9724b9768bcbb75`）及 `parser-execir-array-index-preedit-native-r1-summary.json`（SHA-256 `99d13079634a1bf55b950c502d31305dae652d80c2276029dd8abf2bba356368`）。基线测试以 Debug 编译，未定义 `NDEBUG`。编辑后使用同一构建目录、工具链和配置实际重编译三个目标源文件，build exit 0（3.187 秒），CTest `ssa_arrays_slices` 仍为 1/1、exit 0（0.250 秒）。这是一个含七个 assert 函数调用的测试进程，不是七项独立 CTest 或 Unity case；本轮不借用历史接受文档的 Linux 运行信用。

严格 Clang stdin raw lexer 核对头文件和测试的 1,233 个非注释 token、StartOfLine/LeadingSpace 标志，全部输出已消费，原 token 行号映射完全一致；公开签名、断言和两个整行注释之外的原程序字节不变。证据 `parser-execir-array-index-comment-equivalence-r1.json`（SHA-256 `c0df33316f2a14fbd086f812fbe4b6d2fdb29fdadbd7c6343734e5f77c4a38a2`）。全部可检索 facade 调用仅为测试 :69、:82、:108、:110、:120；umbrella include 是导出入口，不是运行调用。定义/调用及完整函数边界收据为 `array-contiguous-view-caller-unit-audit-r1.json`（SHA-256 `fbf8b9b17be0f0da190f6b787151ba167df2077f76fd9a7db9b321900876e702`）。

修改前后 3,216 个保守输入仅四个已获严格注释等价证明的文件存在原始哈希变化，其余 3,212 项相同；修改后验证期间 3,216 输入和 10 冻结文件均稳定，所有子进程已等待退出。最终运行 `array-contiguous-view-final-native-r2-results.json`（SHA-256 `8dc7de69796cd91fb422762f2a3413153f82c1858da0ca67cc8f1a22c3e84943`），逐项对比 `array-contiguous-view-final-native-r2-comparison.json`（SHA-256 `4559485be5b04be89a699e498bd64a173dadfd57acc8df03755d8f5dfc5aea99`）。这只提供描述符元数据和偏移计算信用；代数不匹配、flags/PINNED 拒绝、参数别名、真实 backing、GC/pin 和生产 SSA/AOT 访问仍未执行。

产物复用 E:/cargo-targets/zr_vm 的当前任务构建与缓存，保留必要报告和既有失败证据；本批释放 0 字节。先前清理被自动审批以 blocked by policy 拒绝，保留项未重试清理，全任务产物清单保持未关闭。
