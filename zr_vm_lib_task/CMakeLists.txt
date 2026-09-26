# 此目录保留旧版 zr.task provider 的独立构建声明；顶层 CMake 当前不纳入它。
# 若重新接入产品图，需先核对与核心 ZrCore_TaskRuntime_RegisterBuiltins 的唯一 provider 约束。
include(${CMAKE_SOURCE_DIR}/zr_vm_common/CommonMacros.cmake)
set(zr_curr_module_name "zr_vm_task")

zr_declare_module(${zr_curr_module_name} ON)

zr_link_library_for_module(${zr_curr_module_name} "zr_vm_core")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_library")
zr_link_library_for_module(${zr_curr_module_name} "zr_vm_parser")

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION include)

zr_install_module(${zr_curr_module_name})
