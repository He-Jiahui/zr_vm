include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

set(zr_unity_source_root ${CMAKE_CURRENT_SOURCE_DIR}/Unity/src)
if (NOT EXISTS "${zr_unity_source_root}/unity.c")
    message(FATAL_ERROR "Missing Unity submodule at ${CMAKE_CURRENT_SOURCE_DIR}/Unity. Run git submodule update --init --recursive.")
endif ()

set(zr_unity_src ${zr_unity_source_root}/unity.c)

zr_register_owned_third_party(tests zr_unity ${zr_unity_src})
target_include_directories(zr_third_party_zr_unity_static PUBLIC ${zr_unity_source_root})
target_compile_definitions(zr_third_party_zr_unity_static PUBLIC
        UNITY_INCLUDE_DOUBLE
        UNITY_DOUBLE_PRECISION=1e-6
)
