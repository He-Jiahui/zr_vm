# Usage: gdb -q -batch -x tests/parser/gdb_ssa_state_map_liveness.gdb
#        --args build/ssa-gcc-debug/bin/zr_vm_ssa_state_map_liveness_test
set pagination off
set confirm off
break UnityFail
commands
silent
print 'ssa_state_map_fixture.h'::diagnostic
bt 5
quit 1
end
run

# @unit gdb_ssa_state_map_liveness.gdb::configure_and_run
# Liveness 诊断会话与运行协议
#
# ## Summary
# 为 state-map liveness 测试启动人工诊断会话。关闭分页和确认后设置 UnityFail 断点，解释完整 commands 列表，再由 run 启动程序，避免在失败处理配置完成前执行测试。
#
# ## Parameters
# 没有函数参数。调用者通过 --args 传入与当前源码相符、具有 GDB 可用调试符号的 zr_vm_ssa_state_map_liveness_test 及原有实参；通过 -x 选择本脚本。开头 Usage 的 build/ 路径是已有示例，实际路径由调用者提供。
#
# ## Returns
# 没有函数返回值。run 运行原程序；命中 UnityFail 时由下一诊断块显式 quit 1。脚本在 run 之后没有统一读取 inferior 退出码的分支，因此不能仅凭本脚本无诊断输出推导九case成功。
#
# ## Constraints and lifecycle
# 应在独立批处理 GDB 会话中使用并由调用者监督进程及超时。脚本没有隔离用户初始化、设置断点 pending 策略或配置线程调度。断点和静态变量的调试名字必须与所选程序匹配；当前 CTest 直接运行测试可执行文件，不自动运行此 GDB 脚本。程序、GDB 和产品状态的生命周期仍由其各自启动及退出协议管理。
#
# ## References
# tests/parser/gdb_ssa_state_map_liveness.gdb:1 记录人工入口；tests/parser/gdb_ssa_state_map_liveness.gdb:5 设失败断点；tests/parser/gdb_ssa_state_map_liveness.gdb:11 结束回调注册；tests/parser/gdb_ssa_state_map_liveness.gdb:12 启动程序。
# tests/cmake/ssa-tests.cmake:277 定义配套目标；tests/cmake/ssa-tests.cmake:302 注册直接运行程序的 CTest。
# tests/parser/test_ssa_state_map_liveness.c:181 和 tests/parser/test_ssa_state_map_liveness.c:192 是原程序 main 与 UNITY_END 结果。

# @unit gdb_ssa_state_map_liveness.gdb::unity_failure_callback
# UnityFail 停点诊断回调
#
# ## Summary
# 在命名符号 UnityFail 的入口停点静默打印 'ssa_state_map_fixture.h'::diagnostic 和最多五层回溯，随后 quit 1，把该停点转为人工诊断输出及非零结束状态。
#
# ## Parameters
# 没有 C 回调参数。输入是当前停住的 inferior 栈和按文件作用域解析的 TU-static diagnostic；可执行文件必须保留相应符号和可读状态。诊断由产品 API 写入，夹具 setUp 不显式清零它，故当前值可能来自先前调用，不能自动视为本次失败断言的原因。
#
# ## Returns
# 没有函数返回值。成功执行各诊断命令后，输出变量快照和回溯并以 quit 1 结束 GDB；若调试表达式或回溯本身出错，应读取 GDB 错误，不能保证后面的自定义 quit 已执行。
#
# ## Constraints and lifecycle
# 这只处理经过 UnityFail 的停点。当前布尔断言通过 UNITY_TEST_FAIL 调用它，但 UINT32 相等断言调用 UnityAssertEqualUintNumber 并可直接 UNITY_FAIL_AND_BAIL，不能宣称所有 Unity 失败均被本断点捕获。打印只借用当前 inferior 状态，不延长 function、map 或诊断相关对象的生命周期，也不保存可复用地址。quit 会提前结束调试，不能承诺继续执行原测试的 tearDown 或产品清理；本脚本不执行内存泄漏或 GC 正确性验证。
#
# ## References
# tests/parser/gdb_ssa_state_map_liveness.gdb:8 按夹具文件作用域读诊断；tests/parser/gdb_ssa_state_map_liveness.gdb:9 限制回溯层数；tests/parser/gdb_ssa_state_map_liveness.gdb:10 提前退出。
# tests/parser/ssa_state_map_fixture.h:15 定义静态诊断；tests/parser/ssa_state_map_fixture.h:17 的 setUp 不重置该值；tests/parser/ssa_state_map_fixture.h:25 定义原测试的 tearDown。
# tests/third_party/zr_unity/Unity/src/unity_internals.h:976 把显式失败派到 UnityFail；tests/third_party/zr_unity/Unity/src/unity_internals.h:996 走无符号整数断言入口；tests/third_party/zr_unity/Unity/src/unity.c:801 可直接跳出。
# tests/third_party/zr_unity/Unity/src/unity.c:2239 是被设置的命名函数断点。
