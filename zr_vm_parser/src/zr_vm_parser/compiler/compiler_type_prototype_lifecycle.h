#ifndef ZR_VM_PARSER_COMPILER_TYPE_PROTOTYPE_LIFECYCLE_H
#define ZR_VM_PARSER_COMPILER_TYPE_PROTOTYPE_LIFECYCLE_H

#include "zr_vm_parser/compiler.h"

void compiler_type_member_free_owned_metadata(
        SZrState *state, SZrTypeMemberInfo *memberInfo);
void compiler_type_prototype_free_owned_metadata(
        SZrState *state, SZrTypePrototypeInfo *info);

#endif
