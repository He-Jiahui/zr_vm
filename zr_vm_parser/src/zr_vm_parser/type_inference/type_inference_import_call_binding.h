#ifndef ZR_VM_PARSER_IMPORT_CALL_BINDING_H
#define ZR_VM_PARSER_IMPORT_CALL_BINDING_H

#include "zr_vm_parser/compiler.h"

void import_call_binding_export(SZrTypeMemberInfo *member, TZrUInt64 moduleHash);
void import_call_binding_method(SZrTypeMemberInfo *member, const SZrFunction *provider);

#endif
