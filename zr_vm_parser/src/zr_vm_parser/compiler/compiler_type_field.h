#ifndef ZR_VM_PARSER_COMPILER_TYPE_FIELD_H
#define ZR_VM_PARSER_COMPILER_TYPE_FIELD_H

#include "zr_vm_parser/compiler.h"

void compiler_type_field_move_inferred_type(
        SZrCompilerState *cs, SZrTypeMemberInfo *memberInfo,
        SZrInferredType *inferredType);
TZrBool compiler_type_field_capture_annotation(
        SZrCompilerState *cs, SZrTypeMemberInfo *memberInfo,
        const SZrType *fieldType);

#endif
