# Keep the scalar pass dependency closure together for hand-built SSA tests.
include_guard(DIRECTORY)
get_filename_component(_zr_pass_source_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(ZR_VM_EXEC_IR_SCALAR_PASS_SOURCES
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp_owner.c
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_constant_materialize.c
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_copy_aliases.c
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_alias_availability.c
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_phi_identity.c
        ${_zr_pass_source_root}/zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
)
unset(_zr_pass_source_root)
