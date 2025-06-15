include(${CMAKE_SOURCE_DIR}/third_party/ThirdPartyMacros.cmake)

set(zr_xx_hash_src ${CMAKE_CURRENT_SOURCE_DIR}/xxHash/xxhash.c)

zr_declare_third_party(zr_xx_hash ${zr_xx_hash_src})