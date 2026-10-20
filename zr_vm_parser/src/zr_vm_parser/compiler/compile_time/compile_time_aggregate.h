#ifndef ZR_VM_PARSER_COMPILE_TIME_AGGREGATE_H
#define ZR_VM_PARSER_COMPILE_TIME_AGGREGATE_H

#include "zr_vm_parser/compiler.h"

/* Successful allocation reservations remain cumulative; rejected quotas charge neither resource. */
TZrBool ct_reserve_aggregate_allocation(
        SZrCompilerState *cs,
        SZrFileRange location);

SZrObject *ct_new_object(SZrCompilerState *cs);
SZrObject *ct_new_array(SZrCompilerState *cs);

#endif
