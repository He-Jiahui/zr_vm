add_library(zr_vm_strict_runtime STATIC ownership.c)
target_include_directories(zr_vm_strict_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
