---
title: Pair 同类型整数比较回归
related_code:
  - tests/container/test_container_pair_numeric.c
implementation_files:
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/container/test_container_pair_numeric.c
  - tests/cmake/container-pair-numeric-tests.cmake
---

Pair.compareTo 使用 first、second 的字典序。当前数值分支把整数统一按
nativeInt64 读取后转为 double：UInt64 高位被解释成负数，超过 double
精确整数范围的相邻值会丢失顺序。两个方向都可能返回正数。

新增夹具导入真实 zr.container 模块，取得 Pair 原型和 __constructor，
通过公开 ZrLib_CallValue 构造三个独立实例，再通过真实 compareTo 方法
派发比较。构造后检查分量类型和完整 64 位载荷，避免把输入转换错误算作比较错误。
每例检查正向、反向、独立相等实例和自身比较；只比较结果符号。

17 个案例覆盖 Int64/UInt64 在 2^53 的相邻值、Int64 最小值和最大值附近、
UInt64 跨 INT64_MAX 的边界、UInt64 最大值附近、完整类型范围、second
分量的相邻值，以及普通小整数和相等控制。四例混合整数覆盖负数与无符号零、
INT64_MAX 两侧、2^53 相邻值及相等值；按精确整数值判断顺序。
另用 Float(0) 与 UInt64 最大值检查无符号载荷误读，并用两个小 Float 保留
浮点正常控制。没有定义 NaN、无穷或超出 double 精度的整数与浮点排序。

夹具关闭自动 GC，将范围限定为数值语义，避免把局部 Pair 的 GC 保护、
OOM 和 owner 清理问题混入。真实 Core 分配器记录地址、旧大小和内存类别，
全局状态销毁后要求零存活记录。没有文件夹具、替代比较函数或生产源码副本。

原 11 例已由主会话实际运行：7 例失败、4 例通过，源码哈希保持冻结。
当前增加 6 例后的夹具尚未构建或运行；扩展 RED/GREEN 由主会话统一执行。
拟修复整数比较的精确顺序及浮点转换时 UInt64 的符号。整数与浮点在 2^53
附近的精度损失仍须保留精确限制，不能把本批通过解释为完整数值全序保证。
