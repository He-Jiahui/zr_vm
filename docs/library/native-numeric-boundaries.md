---
related_code:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - tests/library/test_native_numeric_boundaries.c
  - tests/cmake/native-numeric-boundary-tests.cmake
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/library/test_native_numeric_boundaries.c
  - tests/cmake/native-numeric-boundary-tests.cmake
doc_type: module-detail
---

# Native API 整数实参边界

## 入口与契约

`ZrLib_CallContext_ReadInt` 是 Library 公开参数读取入口。FFI Buffer/Pointer、文件系统、GC budget、任务延迟等调用方依赖其统一参数诊断。整数标签仍沿用原有读取策略，包括 uint64 到 int64 的既有实现相关转换；本组没有修改这些策略或调用方算法。

float/double 实参仍允许朝零截断，例如 3.9 得到 3，-3.9 得到 -3。转换前要求数值有限且位于 `[-2^63, 2^63)`。下界由 int64 最小值精确转为 double，上界取其相反数。使用有序区间的合取同时排除 NaN、正负无穷及所有越界值，无需在非法输入上执行 C 浮点到整数转换。double 中的 `INT64_MAX` 会舍入到 +2^63，因此该浮点值必须拒绝；整数标签的 INT64_MAX 仍合法。

范围检查在判断 `outValue` 是否为空之前完成。合法输入允许空输出；非法输入即使空输出也调用既有 `RaiseTypeError(context, index, "int")`，保留参数序号、函数名和 expected-int 诊断规则。错误发生在写输出之前，调用者的原输出值不变。缺失参数继续走既有 arity 诊断，非数字类型继续走既有 type 诊断。

`ReadFloat` 的数值扩展行为不变，NaN/Inf 继续有效。独立 AOT 转换、Math 支持层已修转换及 uint64 策略不属于本组。

## 实现边界

生产改动仅限 dispatch 文件中 ReadInt 的 float/double 分支，接口头只更新 ReadInt 契约说明。区间验证后才允许 C cast；未新增公开接口、分配或资源所有权。保留同文件其他分工的 Object 写入提取、SetString 修复及当前 CRLF 行尾。超长 dispatch 本次只修已有入口的局部判定，没有增加新职责。

## 回归与证据

独立 fixture 链接真实 Core/Library，构造公开调用上下文并调用 ReadInt，不替换转换实现，也不依赖 Math 私有 helper。八项独立进程由 fragment 注册，各有超时和 Windows 无对话框失败诊断。fragment 先检查全部七个实际静态链接目标，裁剪配置缺任一目标时跳过。

| 用例 | 覆盖 |
| --- | --- |
| int_controls | 整数标签与正常浮点分数朝零截断 |
| finite_range | 有限巨大正负浮点拒绝、输出不变 |
| nonfinite | NaN 与正负无穷拒绝、输出不变 |
| boundaries | -2^63 接受、+2^63 拒绝及邻近合法值 |
| null_output | 合法空输出成功，非法空输出仍同诊断 |
| type_diagnostic | 非数字类型的既有 int 参数诊断 |
| arity_diagnostic | 缺失参数的既有数量诊断 |
| float_controls | ReadFloat 的 NaN/Inf 及普通数值控制 |

非法输入检查 TryRun 状态、函数名、参数序号、expected-int、未正常成功及原输出；最终 GlobalState 释放检查真实宿主分配全部回收。root 的 `integrated-caller-numeric-red-r2` 已自然退出：configure/build 均 0，226 输入 `changed_inputs=[]`。numeric 四项 finite_range/nonfinite/boundaries/null_output 实际 FAIL，其余四项控制 PASS。更早编译错误的一轮不计行为 RED。

Root 的 support-batch-5-green 已实际重新构建，八项全部通过；整批 143 项 CTest、pool 14 项及 CLI 示例通过，1929 个输入哈希无变化。
