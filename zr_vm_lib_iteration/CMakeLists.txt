include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

# 独立构建并安装迭代 provider：容器内建路径调用 Register，共享变体向插件加载器提供统一描述符入口。
set(zr_curr_module_name "zr_vm_lib_iteration")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})

# ## Summary
# provider 只发布协议身份；容器 provider 提供实际同步迭代回调。
# ## Constraints and lifecycle
# 目标种类由 BUILD_STATIC_LIB/BUILD_SHARED_LIB 控制，ON 将 common 源并入模块目标。
# ## References
# zr_vm_lib_iteration/include/zr_vm_lib_iteration/module.h; zr_vm_lib_container/src/zr_vm_lib_container/module.c
