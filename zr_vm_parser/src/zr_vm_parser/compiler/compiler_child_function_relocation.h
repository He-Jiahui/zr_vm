#ifndef ZR_VM_PARSER_COMPILER_CHILD_FUNCTION_RELOCATION_H
#define ZR_VM_PARSER_COMPILER_CHILD_FUNCTION_RELOCATION_H

#include "zr_vm_core/function.h"

/* Rebind owned raw-function slots before source children are detached. */
void compiler_relocate_copied_child_constants(
        SZrFunction *parent,
        SZrFunction *const *originalChildren);

#endif
