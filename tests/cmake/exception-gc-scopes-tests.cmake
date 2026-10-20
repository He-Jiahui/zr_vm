# 内部 GC-domain 记录和锁恢复夹具只在纯静态链接配置注册，避免把 src 内部接口当作 shared 导出契约。
# NOT TARGET 使片段在目标已存在时跳过整组创建与注册；不是单独对 CTest 名做去重。
# TODO: 仓内未见提前创建此 target 的入口；外部若预置目标，需核查 exception_gc_scopes 是否已另行注册。
if (BUILD_STATIC_LIB AND NOT BUILD_SHARED_LIB AND
        NOT TARGET zr_vm_exception_gc_scopes_test)
    zr_vm_add_unity_test_target(zr_vm_exception_gc_scopes_test
            ${CMAKE_SOURCE_DIR}/tests/core/test_exception_gc_scopes.c)
    # 私有 src 头用于读取真实 mutator 记录；公共 core 头与 Unity/harness 路径由此处和公共 helper 分担。
    target_include_directories(zr_vm_exception_gc_scopes_test PRIVATE
            ${CMAKE_SOURCE_DIR}/zr_vm_core/include
            ${CMAKE_SOURCE_DIR}/zr_vm_core/src/zr_vm_core)
    zr_vm_link_core(zr_vm_exception_gc_scopes_test)
    # Threads::Threads 来自父 tests/CMakeLists.txt 的 find_package，支撑 POSIX worker；Windows 源分支使用原生线程。
    target_link_libraries(zr_vm_exception_gc_scopes_test PRIVATE Threads::Threads)
    # 以 target 名交给 CMake 解析可执行路径；不传 --scope-depths-only，main 因而包含两个跨线程锁恢复用例。
    add_test(NAME exception_gc_scopes COMMAND zr_vm_exception_gc_scopes_test)
    # gc/exception 供 CTest 筛选；15 秒约束整个深度、并发 major 和锁等待进程，失败仍由 Unity/进程状态报告。
    set_tests_properties(exception_gc_scopes PROPERTIES LABELS "gc;exception" TIMEOUT 15)
endif ()
