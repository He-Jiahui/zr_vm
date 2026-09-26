include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)

set(zr_curr_module_name "zr_vm_lib_testing")

# 测试提供者由 CLI 在独立测试状态中注册；其描述符依赖解析器的属性契约，
# 断言回调依赖核心运行时和原生库注册表。
zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
