#ifndef ZR_VM_PARSER_COMPILER_INTERFACE_CONTRACTS_H
#define ZR_VM_PARSER_COMPILER_INTERFACE_CONTRACTS_H

#include "compiler_internal.h"

TZrBool compiler_interface_contracts_member_signatures_match(
        const SZrTypeMemberInfo *requiredMember,
        const SZrTypeMemberInfo *implementation);
TZrBool compiler_interface_contracts_validate_value_type(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *valueTypeInfo,
        SZrFileRange errorLocation);

#endif // ZR_VM_PARSER_COMPILER_INTERFACE_CONTRACTS_H
