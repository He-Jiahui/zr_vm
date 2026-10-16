---
related_code:
  - zr_vm_core/include/zr_vm_core/conversion.h
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/src/zr_vm_core/string/string.c
  - tests/core/test_string_native_layout.c
  - tests/cmake/string-native-layout-tests.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/conversion.h
  - zr_vm_core/src/zr_vm_core/string/string.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/core/test_string_native_layout.c
  - tests/cmake/string-native-layout-tests.cmake
doc_type: testing-guide
---

# 原生字符串长布局回归

短字符串的字节保存在对象内，长字符串则通过独立 payload 指针保存内容。原生格式化返回值和 API 字符串缓存需要按布局取得文本地址。只读取 `stringDataExtend` 的转换不能用于任意字符串。

`test_string_native_layout.c` 使用真实 `GlobalState_New` 和 Core 库，提供普通宿主分配器，不替换字符串实现、不注入 OOM。两项用例各自在独立进程中运行，销毁创建的全局状态；Windows 禁用故障对话框，CTest 超时为 30 秒。

`format` 先检查短文本及 `%s`、`%d`、`%%` 的正常组合，再构造超过短串上限的字母文本，通过合法 `%s` 占位符调用 `ZrCore_NativeString_Format`。先检查 VM 栈顶保存的是长字符串及完整文本，再检查返回地址是否等于该对象的 payload 地址。只有地址正确时才读取完整返回文本，避免旧实现指向指针槽时，测试自身通过无界字符串比较造成额外越界。

`cache` 先验证普通短串仍命中，再多次传入同一原生长文本地址，每次核对完整文本、终止符及返回对象身份。内容相等但重复创建长串仍判失败，因为该入口承诺先查 API 字符串缓存。循环覆盖超过一个缓存桶深度的查询次数。

现有 `test_string_layout.c` 的长串测试只检查 native 指针槽对齐，还包含 Function、Module 析构用例；本回归单独成组，以便分别记录格式化与缓存的 RED/GREEN。`support-oom-string-red-r2` 已记录两个真实失败：格式化返回地址不等于 payload；缓存每次创建新对象，但内容保持完整。生产修复将这两处文本访问改用已有的 `ZrCore_String_GetNativeString`，不改变仅供短串使用的转换宏，不引入头文件依赖或宏实参重复求值。全仓自有代码的该宏调用点仅此两处。

`string.c` 目前超过 1100 行；此次仅替换两处既有文本访问，没有增加职责或控制流，因此保留文件布局。格式化及缓存的 OOM 清理是独立缺陷，本回归不授予其验证信用。修复后的两项已在 `integrated-support-batch-green` 的真实 Core 构建中通过，含短串控制；同轮 53 项 CTest、pool 14 项与 CLI hello-world 均通过，收据 `changed_inputs=[]`。独立复核 ACCEPT。实际构建和运行收据保存在 `E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/`。
