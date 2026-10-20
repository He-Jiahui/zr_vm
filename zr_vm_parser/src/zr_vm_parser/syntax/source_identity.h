#ifndef ZR_VM_PARSER_SOURCE_IDENTITY_H
#define ZR_VM_PARSER_SOURCE_IDENTITY_H

#include "zr_vm_parser/conf.h"

/* Source records and AST owners share this non-reusing native namespace. */
TZrBool ZrParser_SourceIdentity_Issue(TZrUInt64 *outKey);

#endif
