cmake_minimum_required(VERSION 3.12)
project(ZR_VM
        VERSION 0.0.1
        LANGUAGES C CXX      # 支持 C 和 C++（C++ 用于 WASM 绑定）
        DESCRIPTION "A ZR VM"
)

# 是否构建静态库（默认 ON）
option(BUILD_STATIC_LIB "Build static library" OFF)

# 是否构建动态库（默认 ON）
option(BUILD_SHARED_LIB "Build shared library" ON)

# ================ 编译器选项和标准设置 ================
set(CMAKE_C_STANDARD 11)           # 默认 C 11
set(CMAKE_C_STANDARD_REQUIRED ON)  # 必须支持指定标准
set(CMAKE_C_EXTENSIONS OFF)        # 禁用编译器扩展（如 GNU g++ 的扩展）

# 设置默认构建类型为 Debug
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()


if (MSVC)
    add_compile_options(/W4 /wd4819 /wd4103)  # Windows: 开启所有警告并视警告为错误 /WX
    add_compile_options(/utf-8)  # 设置源文件和执行字符集为UTF-8，解决中文注释和标识符编码问题
    add_compile_options(-DZR_PLATFORM_WIN_USE_MSVC)
    add_compile_options(-D_CRT_SECURE_NO_WARNINGS)  # 禁用 MSVC 安全函数警告
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)  # Linux/macOS: 严格警告 #-Werror
    add_compile_options(-Wstrict-prototypes -Wmissing-prototypes)
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(USE_SYSTEM_PACKAGES "Use system package manager for dependencies" ON)

find_package(Threads REQUIRED)  # 必选：线程库

# 为 Debug 构建添加 -DDEBUG 宏定义
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-DZR_DEBUG)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-DZR_RELEASE)
endif ()

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
    # 强制为第三方库启用 -fPIC
    add_compile_options(-fPIC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif ()

message(STATUS "C Compiler ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "CXX Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_C_COMPILER_VERSION}")
add_compile_options(-DZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")

add_subdirectory("third_party")

add_subdirectory("zr_vm_common")

add_subdirectory("zr_vm_core")

add_subdirectory("zr_vm_parser")

add_subdirectory("zr_vm_library")

add_subdirectory("zr_vm_language_server")

add_subdirectory("zr_vm_cli")

# 是否构建测试（默认 OFF）
option(BUILD_TESTS "Build test executables" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# 是否构建 Language Server Extension（默认 OFF）
option(BUILD_LANGUAGE_SERVER_EXTENSION "Build VS Code Language Server Extension" OFF)
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
        
        # 添加自定义目标：构建 WASM
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