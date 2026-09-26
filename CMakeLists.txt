cmake_minimum_required(VERSION 3.12)
project(ZR_VM
        VERSION 0.0.1
        LANGUAGES C          # 默认只启用 C；WASM 分支按需启用 C++
        DESCRIPTION "A ZR VM"
)

# 测试子目录按 BUILD_TESTS 选择注册用例；提前启用 CTest，供各模块创建测试目标。
enable_testing()

# 模块宏用这两个开关决定目标和链接优先级；当前默认只构建 shared 版本。
option(BUILD_STATIC_LIB "Build static library" OFF)

option(BUILD_SHARED_LIB "Build shared library" ON)

# ================ 编译器选项和标准设置 ================
set(CMAKE_C_STANDARD 11)           # 默认 C 11
set(CMAKE_C_STANDARD_REQUIRED ON)  # 必须支持指定标准
set(CMAKE_C_EXTENSIONS OFF)        # 以标准 C11 模式调用编译器，不主动启用 GNU C 扩展。

# 仅对单配置生成器设置默认构建类型；多配置生成器（如 VS/MSVC）由 --config 决定。
if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif ()


if (MSVC)
    add_compile_options(
            "$<$<COMPILE_LANGUAGE:C>:/W4>"
            "$<$<COMPILE_LANGUAGE:C>:/wd4819>"
            "$<$<COMPILE_LANGUAGE:C>:/wd4103>"
    )  # 使用 /W4 报告警告；此处没有启用 /WX。
    # MSVC's C11 stdatomic.h implementation is gated behind this switch
    # (VS 2019 16.8 / MSVC 19.28 and newer).  Keep it C-only so the optional
    # C++ JIT target continues to use the native C++ atomic headers.
    if (MSVC_VERSION GREATER_EQUAL 1928)
        add_compile_options(
                "$<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>")
    endif ()
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:/utf-8>")  # 设置源文件和执行字符集为UTF-8，解决中文注释和标识符编码问题
    add_compile_options(-DZR_PLATFORM_WIN_USE_MSVC)
    add_compile_options(-D_CRT_SECURE_NO_WARNINGS)  # 禁用 MSVC 安全函数警告
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)  # 非 MSVC 编译器报告常见和标准兼容性警告；未全局启用 -Werror。
    add_compile_options(-Wstrict-prototypes -Wmissing-prototypes)
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# TODO: 当前构建图仅在此声明 USE_SYSTEM_PACKAGES，没有下游读取；确认是否接入依赖选择或移除此无效开关。
option(USE_SYSTEM_PACKAGES "Use system package manager for dependencies" ON)
option(BUILD_NETWORK_LIB "Build zr_vm_lib_network module" ON)
option(BUILD_DEBUG_LIB "Build zr_vm_lib_debug module" ON)
option(BUILD_THREAD_LIB "Build zr_vm_lib_thread module" ON)
option(BUILD_LANGUAGE_SERVER "Build zr_vm_language_server module" ON)
option(BUILD_RUST_BINDING "Build zr_vm_rust_binding module" ON)
option(BUILD_CLI "Build zr_vm_cli executable" ON)

# Host x86-64/AArch64 JIT is an isolated optional C++ adapter.  Keeping this
# OFF by default means ordinary C11/AOT/ExecBC builds never require a C++
# compiler or LLVM/ORC installation.
option(ZR_VM_ENABLE_HOST_JIT
        "Build the optional host baseline JIT adapter"
        OFF)

# host JIT 在顶层启用 C++17；语言服务器 WASM 分支会在自己的子目录按需另行启用 C++。
if (ZR_VM_ENABLE_HOST_JIT)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif ()

add_compile_definitions(
        $<$<CONFIG:Debug>:ZR_DEBUG>
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:ZR_RELEASE>
)

if (WIN32)
    add_compile_options(-DZR_PLATFORM_WIN)
    message(STATUS "Current platform is WIN")
elseif (APPLE)
    add_compile_options(-DZR_PLATFORM_DARWIN)
    message(STATUS "Current platform is DARWIN")
elseif (UNIX)
    add_compile_options(-DZR_PLATFORM_UNIX)
    message(STATUS "Current platform is UNIX")
endif ()

if (UNIX AND NOT WIN32)
    # 共享库会链接首方与第三方静态对象；为当前目录下所有 Unix C 目标统一启用 PIC。
    add_compile_options(-fPIC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif ()

message(STATUS "C Compiler ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_C_COMPILER_VERSION}")
if (CMAKE_CXX_COMPILER_LOADED)
    message(STATUS "CXX Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
endif ()
add_compile_options(-DZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")

# common 的构建宏先声明共享目标命名/链接规则，后续模块按依赖层次依次加入。
add_subdirectory("zr_vm_common")

add_subdirectory("zr_vm_core")

add_subdirectory("zr_vm_parser")

if (ZR_VM_ENABLE_HOST_JIT)
    add_subdirectory("zr_vm_jit")
endif ()

add_subdirectory("zr_vm_library")

if (BUILD_NETWORK_LIB)
    if (EXISTS "${CMAKE_SOURCE_DIR}/zr_vm_lib_network/CMakeLists.txt")
        add_subdirectory("zr_vm_lib_network")
    else ()
        message(STATUS "Skipping zr_vm_lib_network: directory is absent in this checkout")
    endif ()
else ()
    message(STATUS "Skipping zr_vm_lib_network: BUILD_NETWORK_LIB is OFF")
endif ()

# debug provider 的网络桥接在 CMake 目标层依赖 network；关闭 network 时不注册 debug。
if (BUILD_DEBUG_LIB AND (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static))
    if (EXISTS "${CMAKE_SOURCE_DIR}/zr_vm_lib_debug/CMakeLists.txt")
        add_subdirectory("zr_vm_lib_debug")
    else ()
        message(STATUS "Skipping zr_vm_lib_debug: directory is absent in this checkout")
    endif ()
elseif (BUILD_DEBUG_LIB)
    message(STATUS "Skipping zr_vm_lib_debug: zr_vm_lib_network target is unavailable")
else ()
    message(STATUS "Skipping zr_vm_lib_debug: BUILD_DEBUG_LIB is OFF")
endif ()

add_subdirectory("zr_vm_lib_math")

add_subdirectory("zr_vm_lib_system")

add_subdirectory("zr_vm_lib_iteration")

add_subdirectory("zr_vm_lib_container")

add_subdirectory("zr_vm_lib_ffi")

add_subdirectory("zr_vm_lib_testing")

if (BUILD_THREAD_LIB)
    if (EXISTS "${CMAKE_SOURCE_DIR}/zr_vm_lib_thread/CMakeLists.txt")
        add_subdirectory("zr_vm_lib_thread")
    else ()
        message(STATUS "Skipping zr_vm_lib_thread: directory is absent in this checkout")
    endif ()
else ()
    message(STATUS "Skipping zr_vm_lib_thread: BUILD_THREAD_LIB is OFF")
endif ()

if (BUILD_LANGUAGE_SERVER)
    add_subdirectory("zr_vm_language_server")
else ()
    message(STATUS "Skipping zr_vm_language_server: BUILD_LANGUAGE_SERVER is OFF")
endif ()

# Rust 绑定当前链接 network 目标；无该目标时跳过，以免暴露缺失依赖的构建入口。
if (BUILD_RUST_BINDING AND (TARGET zr_vm_lib_network_shared OR TARGET zr_vm_lib_network_static))
    add_subdirectory("zr_vm_rust_binding")
elseif (BUILD_RUST_BINDING)
    message(STATUS "Skipping zr_vm_rust_binding: zr_vm_lib_network target is unavailable")
else ()
    message(STATUS "Skipping zr_vm_rust_binding: BUILD_RUST_BINDING is OFF")
endif ()

if (BUILD_CLI)
    add_subdirectory("zr_vm_cli")
else ()
    message(STATUS "Skipping zr_vm_cli: BUILD_CLI is OFF")
endif ()

# 默认注册首方测试；大范围 CTest 的实际运行由调用者或 CI 决定。
option(BUILD_TESTS "Build test executables" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 扩展目标需要语言服务器和 Node/npm，普通 C 库构建不触发 npm 或 WASM 打包。
option(BUILD_LANGUAGE_SERVER_EXTENSION "Build VS Code Language Server Extension" OFF)
if (BUILD_LANGUAGE_SERVER_EXTENSION AND NOT BUILD_LANGUAGE_SERVER)
    message(FATAL_ERROR "BUILD_LANGUAGE_SERVER_EXTENSION requires BUILD_LANGUAGE_SERVER=ON")
endif ()
if(BUILD_LANGUAGE_SERVER_EXTENSION)
    # 检查 Node.js 是否可用
    find_program(NODE_EXECUTABLE node)
    find_program(NPM_EXECUTABLE npm)
    
    if(NODE_EXECUTABLE AND NPM_EXECUTABLE)
        message(STATUS "Node.js found: ${NODE_EXECUTABLE}")
        message(STATUS "npm found: ${NPM_EXECUTABLE}")
        
        # 设置 extension 目录
        set(EXTENSION_DIR ${CMAKE_SOURCE_DIR}/zr_vm_language_server_extension)
        set(EXTENSION_WASM_DIR ${EXTENSION_DIR}/wasm)
        
        # 确保 WASM 目录存在
        file(MAKE_DIRECTORY ${EXTENSION_WASM_DIR})
        
        # BUG: 普通 MSVC/GCC/Clang 配置中，此命令用原生工具链原地重配置 BUILD_WASM；
        # language-server 子目录只在 Emscripten 下创建 *_wasm 目标，下一命令遂找不到目标。
        # 扩展脚本已有 emcmake 独立构建目录；此目标应复用它，再复制 wasm 产物。
        add_custom_target(build_wasm_for_extension
            COMMAND ${CMAKE_COMMAND} -DBUILD_WASM=ON ${CMAKE_SOURCE_DIR}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target zr_vm_language_server_wasm
            COMMENT "Building WASM for VS Code Extension"
        )
        
        # 复制 WASM 文件到 extension 目录
        add_custom_command(TARGET build_wasm_for_extension POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_BINARY_DIR}/wasm/zr_vm_language_server.wasm
                ${EXTENSION_WASM_DIR}/zr_vm_language_server.wasm
            COMMENT "Copying WASM file to extension directory"
        )
        
        # 安装 npm 依赖
        add_custom_target(install_extension_dependencies
            COMMAND ${NPM_EXECUTABLE} install
            WORKING_DIRECTORY ${EXTENSION_DIR}
            COMMENT "Installing VS Code Extension dependencies"
        )
        
        # 编译 TypeScript
        add_custom_target(compile_extension
            COMMAND ${NPM_EXECUTABLE} run compile
            WORKING_DIRECTORY ${EXTENSION_DIR}
            DEPENDS install_extension_dependencies
            COMMENT "Compiling VS Code Extension TypeScript"
        )
        
        # 打包 extension
        add_custom_target(package_extension
            COMMAND ${NPM_EXECUTABLE} run package
            WORKING_DIRECTORY ${EXTENSION_DIR}
            DEPENDS compile_extension build_wasm_for_extension
            COMMENT "Packaging VS Code Extension"
        )
        
        message(STATUS "VS Code Extension build targets configured")
        message(STATUS "  - build_wasm_for_extension: Build WASM module")
        message(STATUS "  - install_extension_dependencies: Install npm dependencies")
        message(STATUS "  - compile_extension: Compile TypeScript")
        message(STATUS "  - package_extension: Package extension for distribution")
    else()
        message(WARNING "Node.js or npm not found. VS Code Extension build disabled.")
        message(WARNING "  Install Node.js and npm to enable extension build.")
    endif()
endif()
