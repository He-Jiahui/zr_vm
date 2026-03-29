include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

set(zr_c_json_src ${CMAKE_CURRENT_SOURCE_DIR}/cJSON/cJSON.c)

zr_register_owned_third_party(zr_vm_library zr_c_json ${zr_c_json_src})
