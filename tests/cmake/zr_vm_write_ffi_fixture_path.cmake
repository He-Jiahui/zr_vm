# 顶层构建在 fixture 生成后调用本脚本写头文件；FFI 和 system_fs 测试共享该绝对路径。
# 将 Windows 反斜杠转成斜杠，避免生成的 C 字符串字面量把路径片段当转义序列。
if (NOT DEFINED IN_DLL OR NOT DEFINED OUT_FILE)
    message(FATAL_ERROR "zr_vm_write_ffi_fixture_path.cmake requires -DIN_DLL= and -DOUT_FILE=")
endif ()

file(TO_CMAKE_PATH "${IN_DLL}" _ffi_path_cmake_style)

# BUG: Linux 构建路径若含双引号，原样插入的路径会截断生成的 C 字符串，导致依赖此头文件的测试编译失败。
file(WRITE "${OUT_FILE}"
        "#ifndef ZR_VM_TESTS_FFI_FIXTURE_PATH_H\n"
        "#define ZR_VM_TESTS_FFI_FIXTURE_PATH_H\n"
        "#define ZR_VM_FFI_FIXTURE_PATH \"${_ffi_path_cmake_style}\"\n"
        "#endif\n")
