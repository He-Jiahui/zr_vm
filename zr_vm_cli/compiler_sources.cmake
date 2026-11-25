include_guard(GLOBAL)

# The CLI discovers these files in its own source tree. Embedders that build
# the compiler without the CLI entry point must use the same complete set.
function(zr_vm_target_add_cli_compiler target)
    set(compiler_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/zr_vm_cli/compiler")
    target_sources(${target} PRIVATE
            "${compiler_dir}/compiler.c"
            "${compiler_dir}/compiler_aot.c"
            "${compiler_dir}/compiler_aot_exports.c"
            "${compiler_dir}/strict_records.c"
            "${compiler_dir}/strict_scalar.c"
            "${compiler_dir}/strict_semantic_flow.c")
    target_link_libraries(${target} PRIVATE zr_vm_strict_runtime)
endfunction()
