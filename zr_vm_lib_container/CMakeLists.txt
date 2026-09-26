include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
# 容器回调实现 zr.iteration 的协议成员，模块链接和 CLI 注册链都依赖规范迭代 provider。
set(zr_curr_module_name "zr_vm_lib_container")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_lib_iteration")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
