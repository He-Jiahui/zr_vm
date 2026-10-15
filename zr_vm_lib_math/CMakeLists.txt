include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_lib_math")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

if (NOT WIN32)
    find_library(ZR_VM_SYSTEM_MATH_LIBRARY m)
    if (ZR_VM_SYSTEM_MATH_LIBRARY)
        zr_link_internal_for_module(${zr_curr_module_name} ${ZR_VM_SYSTEM_MATH_LIBRARY})
    endif ()
endif ()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})

# ## Summary
# 声明 Math 产品库的静态/共享构建、首方依赖、可选系统数学库及安装规则；本文件不定义测试或运行时行为。
#
# ## Configuration inputs and outputs
# 顶层先加入 zr_vm_common、再通过 add_subdirectory 引入本目录。zr_declare_module 的 ON 把 common 源清单纳入
# Math 变体，并按 BUILD_STATIC_LIB / BUILD_SHARED_LIB 创建目标；后续 helper 对同一组已存在目标追加依赖与安装规则。
#
# ## Constraints and lifecycle
# 非 Windows 平台只尝试查找系统库名 m；只有 find_library 成功时才把解析出的库链接到各启用变体，未找到时跳过。
# 公共 include 目录复制到安装前缀；模块目标是否安装仍由静态/共享构建开关控制。
#
# ## References
# helper 的目标、源码 glob、链接与 install 语义见 zr_vm_common/CommonMacros.cmake 的
# zr_declare_module、zr_link_library_for_module、zr_link_internal_for_module、zr_install_module。
# tests/cmake/compiler-project-targets.cmake 通过共享的链接 helper 将 Math 变体纳入模块集成测试。
