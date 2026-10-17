#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_UNALIGNED_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_UNALIGNED_H

#include "zr_vm_core/conf.h"

/* A scalar field borrowed from a packed ZRP row. Helpers accept void * so that
 * taking the field address never promises native UInt32 alignment. */
#pragma pack(push, 1)
typedef struct SZrAotCZrpUInt32Field {
    TZrUInt32 value;
} SZrAotCZrpUInt32Field;
#pragma pack(pop)

#endif
