# ## Summary
# 为手工构造的 checked signed multiplication 夹具注册独立 Unity 可执行文件和 CTest 入口。
# 此片段由 tests 目录配置阶段 include；目标存在性守卫让无 Core 的配置跳过整个入口。
#
# ## Parameters / input settings
# - 顶层 BUILD_TESTS 决定是否进入 tests；CMAKE_SOURCE_DIR 必须指向仓库根。
# - 依赖父级 zr_vm_add_unity_test_target 与 zr_vm_link_core；Unity third-party 目标须先注册。
# - BUILD_SHARED_LIB 必须与实际已创建的 Core 变体一致，由链接助手选择 shared 或 static。
#
# ## Returns / configuration effects
# 无函数返回值；添加 checked multiply target 和同名语义的 execution_checked_multiply CTest。
# Unity 助手复用公共 runtime、path、crash guard、reference harness 及编译设置。
# 本入口另外提供仓库根、Core public/private include，供夹具直接使用 checked integer 私有助手。
#
# ## Constraints and lifecycle
# 任一种 Core 目标存在时才注册；链接所选变体及已注册 Unity，缺失 Unity 会在配置期失败。
# 夹具直接构造 Core 函数和指令，未通过 parser、AOT 或 SSA 代码生成；配置完成也不表示已执行。
# main 的八个场景通过 UNITY_END 返回失败状态，只有实际运行该进程才产生测试结论。
#
# ## References
# - CMakeLists.txt:185；tests/CMakeLists.txt:284（父级调用）
# - tests/CMakeLists.txt:63；tests/CMakeLists.txt:180（目标与链接助手）
# - zr_vm_common/ThirdPartyMacros.cmake:49（已注册依赖的链接入口）
# - tests/core/test_execution_checked_multiply.c:13；tests/core/test_execution_checked_multiply.c:477（私有 include 与 main）
# - tests/third_party/zr_unity/Unity/src/unity.c:2333（UnityEnd 状态汇总）
if (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    zr_vm_add_unity_test_target(zr_vm_execution_checked_multiply_test
        ${CMAKE_SOURCE_DIR}/tests/core/test_execution_checked_multiply.c)
    target_include_directories(zr_vm_execution_checked_multiply_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    zr_vm_link_core(zr_vm_execution_checked_multiply_test)
    add_test(NAME execution_checked_multiply
        COMMAND zr_vm_execution_checked_multiply_test)
    # ssa 供 CTest 标签筛选；该名称不证明走过 SSA 生成或执行链。
    # 120 秒限制此命令的运行时长，不承诺运行成功、性能界限或完整覆盖。
    set_tests_properties(execution_checked_multiply PROPERTIES
        LABELS "ssa"
        TIMEOUT 120)
endif()
