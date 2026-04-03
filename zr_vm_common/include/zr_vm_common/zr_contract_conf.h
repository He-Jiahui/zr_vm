//
// Stable contract-role identifiers shared by compiler, runtime and native metadata.
//

#ifndef ZR_VM_COMMON_ZR_CONTRACT_CONF_H
#define ZR_VM_COMMON_ZR_CONTRACT_CONF_H

#include "zr_vm_common/zr_common_conf.h"

typedef enum EZrMemberContractRole {
    ZR_MEMBER_CONTRACT_ROLE_NONE = 0,
    ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT = 1,
    ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT = 2,
    ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_METHOD = 3,
    ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD = 4,
    ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH = 5
} EZrMemberContractRole;

#endif // ZR_VM_COMMON_ZR_CONTRACT_CONF_H
