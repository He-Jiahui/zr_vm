file(READ "${SOURCE_FILE}" source)
execute_process(COMMAND "${CLI_EXE}" -e "${source}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30)
string(STRIP "${output}" output)
if(NOT result EQUAL 0 OR NOT output STREQUAL "7")
    message(FATAL_ERROR "Constructor prototype growth failed (${result}): ${output}\n${error}")
endif()

# @unit configuration::run_constructor_prototype_growth
# ## Summary
# 把指定构造器回归夹具读入内存，作为一个 -e 参数启动 CLI，检查退出状态与输出值，使 CTest 能观察编译及执行链的退化。
# ## Parameters
# CLI_EXE [in]：可启动的 CLI 可执行路径；注册方用 zr_vm_cli_executable 的 TARGET_FILE 传入，runner 不负责构建或探测其版本。
# SOURCE_FILE [in]：可读的夹具路径；file(READ) 在启动子进程前读取全文，源文件仍归调用方所有，runner 不改写它。
# ## Returns
# 子进程结果等于 0 且 STRIP 后 stdout 精确为 7 时脚本正常结束；否则用 FATAL_ERROR 交付失败并附带捕获的 stdout 与 stderr。
# 读文件失败可在启动 CLI 前中止；子进程启动失败或超时也不能当作退出 0。成功分支不要求 stderr 为空，不按日志内容证明更多内部路径。
# ## Constraints and lifecycle
# execute_process 的 30 秒约束仅覆盖 CLI 子进程；配套 CTest 的 40 秒覆盖整个 runner，两者不能互换。
# source、result、output、error 是本次 CMake 脚本作用域内的值。CLI 收到完整源码的单个实参，未经过命令 shell；runner 未指定 WORKING_DIRECTORY 或环境覆盖，继承启动环境。
# 此脚本不创建 workspace、日志或缓存目录，也没有失败回滚/清理协议；不能由此推断 CLI 及其所有原生模块均无宿主副作用。
# 只检查这一手写场景的外部结果，未直接断言 prototype 扩容、分配失败恢复、全部构造器种类或线程交错。
# ## References
# tests/cmake/runtime-compiler-boundary-tests.cmake:58 — CLI target 条件边界。
# tests/cmake/runtime-compiler-boundary-tests.cmake:68 — 注册名、两个输入变量及 -P 入口。
# tests/cmake/runtime-compiler-boundary-tests.cmake:73 — CTest TIMEOUT 40。
# zr_vm_cli/src/zr_vm_cli/command/command.c:714 — -e 的下一实参成为 inlineCode。
# zr_vm_cli/src/zr_vm_cli/runtime/runtime.c:1288 — inline 源码编译入口。
