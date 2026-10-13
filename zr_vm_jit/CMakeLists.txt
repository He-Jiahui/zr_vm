# zr_vm_jit 配置入口
# Summary: 顶层 ZR_VM_ENABLE_HOST_JIT 开启时才进入本目录；这里构建独立 C++17 facade，
#          core 仍保持 C11，且本 target 当前只提供契约校验和记录生命周期管理。
# Inputs: ZR_VM_JIT_USE_LLVM 可选地探测 LLVM package；它不等价于可运行的 ORC provider。
# Outputs: zr_vm_jit 静态库、公开 include 路径及安装规则；目标另链接 core。
# Constraints: 当前实现 machineCodeAvailable 恒为 false，缺少 provider 时 Compile/回调
#              只报告不可用或 fallback。相关验证入口见 tests/cmake/ssa-tests.cmake。

# 仅探测可选 LLVM 配置包。此选项不改变顶层是否加入本目录，也不会单独启用机器码执行。
option(ZR_VM_JIT_USE_LLVM
        "Probe for LLVM ORC/JITLink for the optional host-JIT provider"
        OFF)

# TODO: ZR_VM_JIT_HAS_ORC 目前只进入编译定义，两个源码均未读取该宏，且本 target 未链接
# ORC/JITLink 库。接入 provider 时应从真实实现、依赖链接及能力报告一起核查此门禁；
# LLVM_FOUND 只能证明 package 可发现，不能单独宣称机器码能力。
set(ZR_VM_JIT_HAS_ORC OFF)
if (ZR_VM_JIT_USE_LLVM)
    # 不在契约 adapter 中固定 LLVM API 版本；package 缺失时继续构建 contract-only target，
    # 由运行时状态显式报告 BACKEND_UNAVAILABLE，而不是让配置阶段误报机器码支持。
    find_package(LLVM CONFIG QUIET)
    if (LLVM_FOUND)
        set(ZR_VM_JIT_HAS_ORC ON)
        message(STATUS "LLVM found for zr_vm_jit (${LLVM_PACKAGE_VERSION}); provider hooks remain contract-gated")
    else ()
        message(STATUS "LLVM not found: zr_vm_jit will report BACKEND_UNAVAILABLE")
    endif ()
endif ()

# 将 facade 与状态图声明校验编译成单独静态库。两个实现文件共享公开 C ABI，
# 但用 C++17 实现内部 singleton、mutex 与记录证明；此 target 不改变 core 的语言标准。
add_library(zr_vm_jit STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/orc_backend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/jit_state_maps.cpp)
add_library(zr_vm_jit::zr_vm_jit ALIAS zr_vm_jit)

# 消费者通过 C ABI 头文件调用；私有编译定义记录 package 探测结果，不等同于 provider 注册成功。
target_compile_features(zr_vm_jit PUBLIC cxx_std_17)
target_compile_definitions(zr_vm_jit PRIVATE
        ZR_VM_JIT_HAS_ORC=$<BOOL:${ZR_VM_JIT_HAS_ORC}>)
set_target_properties(zr_vm_jit PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME zr_vm_jit)

target_include_directories(zr_vm_jit
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/zr_vm_core/include
        ${CMAKE_SOURCE_DIR}/zr_vm_common/include)

# 部分 core C 头在 C++ 中保留 C designated initializer 和可变字符串别名。
# 非 MSVC 编译时抑制这两类既有兼容性告警，保留其余 adapter 告警检查；
# MSVC 使用 /W4 并启用标准 C++ 异常处理。
if (MSVC)
    target_compile_options(zr_vm_jit PRIVATE /W4 /EHsc)
else ()
    target_compile_options(zr_vm_jit PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-pedantic>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>)
endif ()

# core 拥有 publication 记录和 lease 计数；本库借用其 API，不复制 manager 实现。
# parser AOT IR 目标若已存在则可选链接；否则本 target 不因此拉入 parser。
if (TARGET zr_vm_parser_aot_ir)
    target_link_libraries(zr_vm_jit PRIVATE zr_vm_parser_aot_ir)
endif ()
if (TARGET zr_vm_core_static)
    target_link_libraries(zr_vm_jit PRIVATE zr_vm_core_static)
elseif (TARGET zr_vm_core_shared)
    target_link_libraries(zr_vm_jit PRIVATE zr_vm_core_shared)
else ()
    message(FATAL_ERROR "ZR_VM_ENABLE_HOST_JIT requires a core library target")
endif ()

install(TARGETS zr_vm_jit
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)
