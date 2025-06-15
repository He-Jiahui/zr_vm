cmake_minimum_required(VERSION 3.12)
project(ZR_VM
        VERSION 0.0.1
        LANGUAGES C      # 支持 C++ 和 C
        DESCRIPTION "A ZR VM"
)

# 是否构建静态库（默认 ON）
option(BUILD_STATIC_LIB "Build static library" OFF)

# 是否构建动态库（默认 ON）
option(BUILD_SHARED_LIB "Build shared library" ON)

# ================ 编译器选项和标准设置 ================
set(CMAKE_C_STANDARD 17)           # 默认 C 17
set(CMAKE_C_STANDARD_REQUIRED ON)  # 必须支持指定标准
set(CMAKE_C_EXTENSIONS OFF)        # 禁用编译器扩展（如 GNU g++ 的扩展）

# 设置默认构建类型为 Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 定义构建类型选项
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel"
        HELP "Choose the type of build."
        FORCE
)

if(MSVC)
    add_compile_options(/W4 /WX)  # Windows: 开启所有警告并视警告为错误
    add_compile_options(-DZR_PLATFORM_WIN_USE_MSVC)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)  # Linux/macOS: 严格警告
    add_compile_options(-Wstrict-prototypes -Wmissing-prototypes)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(USE_SYSTEM_PACKAGES "Use system package manager for dependencies" ON)

find_package(Threads REQUIRED)  # 必选：线程库

# 为 Debug 构建添加 -DDEBUG 宏定义
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-DZR_DEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-DZR_RELEASE)
endif()

if(WIN32)
    add_compile_options(-DZR_PLATFORM_WIN)
    message(STATUS "Current platform is WIN")
elseif(APPLE)
    add_compile_options(-DZR_PLATFORM_DARWIN)
    message(STATUS "Current platform is DARWIN")
elseif(UNIX)
    add_compile_options(-DZR_PLATFORM_UNIX)
    message(STATUS "Current platform is UNIX")
endif()

message(STATUS "Compiler ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_C_COMPILER_VERSION}")
add_compile_options(-DZR_VM_COMPILER_VERSION="${CMAKE_C_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}")

add_subdirectory("third_party")

add_subdirectory("zr_vm_common")

add_subdirectory("zr_vm_core")
add_subdirectory("zr_vm_library")


add_subdirectory("zr_vm_cli")