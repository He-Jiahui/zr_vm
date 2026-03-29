include(${CMAKE_SOURCE_DIR}/zr_vm_common/ThirdPartyMacros.cmake)

set(zr_xx_hash_src ${CMAKE_CURRENT_SOURCE_DIR}/xxHash/xxhash.c)

zr_register_owned_third_party(zr_vm_core zr_xx_hash ${zr_xx_hash_src})
