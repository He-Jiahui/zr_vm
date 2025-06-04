cmake_minimum_required(VERSION 3.12)
project(ZR_VM
        VERSION 0.0.1
        LANGUAGES C      # 支持 C++ 和 C
        DESCRIPTION "A ZR VM"
)

# ================ 编译器选项和标准设置 ================
set(CMAKE_C_STANDARD 17)           # 默认 C 17
set(CMAKE_C_STANDARD_REQUIRED ON)  # 必须支持指定标准
set(CMAKE_C_EXTENSIONS OFF)        # 禁用编译器扩展（如 GNU g++ 的扩展）

if(MSVC)
    add_compile_options(/W4 /WX)  # Windows: 开启所有警告并视警告为错误
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)  # Linux/macOS: 严格警告
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(USE_SYSTEM_PACKAGES "Use system package manager for dependencies" ON)

find_package(Threads REQUIRED)  # 必选：线程库

add_subdirectory("zr_vm_common")

add_subdirectory("zr_vm_core")
add_subdirectory("zr_vm_library")


add_subdirectory("zr_vm_cli")