# Close-proxy cleanup is a core runtime contract shared by the VM and AOT.
# 由 tests/CMakeLists.txt 的末尾 include 进入；Unity helper 统一加入公共 harness 并建立各场景的可执行目标。
# TODO: 仓内只见该 include 入口；外部若复用本片段，需核实 helper、模块目标已存在且没有重复注册同名 target/CTest。
# core API 夹具检查代理只消费 source 一次、保留较老登记，并覆盖 GC、异常替换与 ownership 镜像清理。
zr_vm_add_unity_test_target(zr_vm_close_proxy_core_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_close_proxy.c)
# 此用例直接使用 core API 与仓根相对的 harness 头，不需要源码编译器或 library 的 AOT helper。
target_include_directories(zr_vm_close_proxy_core_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
zr_vm_link_core(zr_vm_close_proxy_core_test)
# CTest 解析目标文件路径后启动 Unity main；ssa/core 标签用于筛选，30 秒限制整个进程而非单个用例。
add_test(NAME close_proxy_core COMMAND $<TARGET_FILE:zr_vm_close_proxy_core_test>)
set_tests_properties(close_proxy_core PROPERTIES LABELS "ssa;core" TIMEOUT 30)

# 编译真实 using/@close 脚本，检查 original 的保留、replacement 的替换和复用 call-info 链的可达性。
zr_vm_add_unity_test_target(zr_vm_close_meta_exception_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_close_meta_exception.c)
# 脚本创建 VM 时登记 parser 与 system 原型；头路径及 library 链接组合支持该源码执行入口。
target_include_directories(zr_vm_close_meta_exception_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include
        ${CMAKE_SOURCE_DIR}/zr_vm_lib_system/include)
zr_vm_link_parser_core_plus_library(zr_vm_close_meta_exception_test)
# 该注册执行四个脚本场景；core 标签表示清理语义归属，不表示只链接 core 库或已经运行通过。
add_test(NAME close_meta_exception COMMAND $<TARGET_FILE:zr_vm_close_meta_exception_test>)
set_tests_properties(close_meta_exception PROPERTIES LABELS "ssa;core" TIMEOUT 30)

# 手写 ExecBC 验证解释器从 E/A1 登记代理并只回调一次，另检查高代理槽导致的帧槽计数。
zr_vm_add_unity_test_target(zr_vm_close_proxy_instruction_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_close_proxy_instruction.c)
# 指令用例手工建 core function/call-info，不经 parser 编译，因此只暴露仓根和 core 头。
target_include_directories(zr_vm_close_proxy_instruction_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include)
zr_vm_link_core(zr_vm_close_proxy_instruction_test)
# 单独保留解释器入口的 CTest 名，便于与 core API 夹具分别定位；30 秒覆盖 Unity main 的两个用例。
add_test(NAME close_proxy_instruction COMMAND $<TARGET_FILE:zr_vm_close_proxy_instruction_test>)
set_tests_properties(close_proxy_instruction PROPERTIES LABELS "ssa;core" TIMEOUT 30)

# 直接调用 AOT runtime helper，检查高物理代理清空 logical/physical source 后只关闭一次；不执行生成函数入口。
zr_vm_add_unity_test_target(zr_vm_close_proxy_aot_runtime_test
        ${CMAKE_SOURCE_DIR}/tests/library/test_close_proxy_aot_runtime.c)
# 手工 generated-frame 夹具需要 core 布局与 library 的 aot_runtime 接口；链接由现有公共组合提供。
target_include_directories(zr_vm_close_proxy_aot_runtime_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_library/include)
zr_vm_link_parser_core_plus_library(zr_vm_close_proxy_aot_runtime_test)
# aot 标签标识 helper 契约的筛选归属；此注册不创建编译器驱动，也不证明生成代码或强制 GC 路径。
add_test(NAME close_proxy_aot_runtime COMMAND $<TARGET_FILE:zr_vm_close_proxy_aot_runtime_test>)
set_tests_properties(close_proxy_aot_runtime PROPERTIES LABELS "ssa;aot" TIMEOUT 30)

# 用 writer 写出无新 opcode 的二进制，核对 patch 44 输出、patch 43 读取和未来 patch 45 的拒绝门禁。
zr_vm_add_unity_test_target(zr_vm_close_proxy_legacy_patch_test
        ${CMAKE_SOURCE_DIR}/tests/module/test_close_proxy_legacy_patch.c)
# 此兼容性夹具读 core IO、写 parser artifact，不需要 system/library 的脚本模块注册。
target_include_directories(zr_vm_close_proxy_legacy_patch_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_parser/include)
zr_vm_link_parser_core(zr_vm_close_proxy_legacy_patch_test)
# artifact 标签把格式门禁与运行时清理分开筛选；固定 opcode/patch 断言仅由该进程中的 Unity main 执行。
add_test(NAME close_proxy_legacy_patch COMMAND $<TARGET_FILE:zr_vm_close_proxy_legacy_patch_test>)
set_tests_properties(close_proxy_legacy_patch PROPERTIES LABELS "ssa;artifact" TIMEOUT 30)
