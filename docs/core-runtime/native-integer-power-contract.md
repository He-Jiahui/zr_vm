---
related_code:
  - zr_vm_core/include/zr_vm_core/math.h
  - tests/core/test_native_math_failures.c
  - tests/cmake/native-math-tests.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/math.h
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
  - docs/code-review/coverage/zr_vm_core_math_conversion.tsv
tests:
  - tests/core/test_native_math_failures.c
  - tests/cmake/native-math-direct-validation/CMakeLists.txt
  - tests/acceptance/native-math-failures.md
doc_type: module-detail
---

# 原生整数幂的数值契约

`ZrCore_Math_UIntPower` 与 `ZrCore_Math_IntPower` 是解释器和 AOT 支持层共用的纯数值函数。函数不分配内存、不抛异常。语言级定义域仍由调用者检查；辅助函数的零指数结果保持 1，负的有符号指数和结果溢出保持返回 0。合法零结果与溢出无法从现有返回类型区分。

无符号计算使用平方递推。正指数的零基数直接返回零，避免零除。累积结果乘基数之前检查 `result > UINT64_MAX / base`；仅在仍需处理指数时检查基数平方是否可表示。这样既能接受大数的一次幂，也不会让最后一次累积乘法回绕。

有符号计算以无符号减法取得负数幅值，避免对 INT64_MIN 做有符号取负。正结果最多为 INT64_MAX；负结果可包含幅值 2^63，并直接返回 INT64_MIN，不执行越界转换后取负。

回归覆盖零基数、指数零与一、可表示的大整数、平方和累积乘法的溢出、负底数奇偶性、INT64_MIN 本身与普通负底数达到最小值的情况。初始 MSVC 四组测试均失败，分别显示除零、合法大值误拒绝及最小负数问题；最终七组使用共享 CMake 注册供普通 CTest 和独立双编译器验证。该定向证据不等同于完整语言、AOT 或跨平台验收。
