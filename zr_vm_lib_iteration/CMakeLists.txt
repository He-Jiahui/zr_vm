include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

# 作为独立 provider 构建：CLI 经 zr.container 注册它，共享库还可由原生插件加载器读取统一入口。
set(zr_curr_module_name "zr_vm_lib_iteration")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
