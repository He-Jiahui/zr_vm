# ZR-VM Tests CMakeLists.txt
# 所有测试模块都在各自的子目录下

# GC 测试
add_subdirectory(gc)

# Parser 测试（如果构建了 parser 模块）
if(TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static)
    add_subdirectory(parser)
endif()

# Meta 测试（如果构建了 core 模块）
if(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    add_subdirectory(meta)
endif()

# Instructions 测试（如果构建了 core 模块）
if(TARGET zr_vm_core_shared OR TARGET zr_vm_core_static)
    add_subdirectory(instructions)
endif()

# Scripts 综合测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(scripts)
endif()

# Exceptions 测试（如果构建了 parser 和 core 模块）
if((TARGET zr_vm_parser_shared OR TARGET zr_vm_parser_static) AND 
   (TARGET zr_vm_core_shared OR TARGET zr_vm_core_static))
    add_subdirectory(exceptions)
endif()
