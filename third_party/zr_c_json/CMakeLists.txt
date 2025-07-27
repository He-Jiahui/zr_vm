include(${CMAKE_SOURCE_DIR}/third_party/ThirdPartyMacros.cmake)

set(zr_c_json_src ${CMAKE_CURRENT_SOURCE_DIR}/cJSON/cJSON.c)

zr_declare_third_party(zr_c_json ${zr_c_json_src})