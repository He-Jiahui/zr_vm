add_library(zr_vm_strict_runtime STATIC ownership.c program.c program_c.c)
target_include_directories(zr_vm_strict_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
install(TARGETS zr_vm_strict_runtime ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
