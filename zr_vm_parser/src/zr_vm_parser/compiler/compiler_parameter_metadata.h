#ifndef ZR_VM_PARSER_COMPILER_PARAMETER_METADATA_H
#define ZR_VM_PARSER_COMPILER_PARAMETER_METADATA_H

#include "compiler_internal.h"

TZrBool compiler_parameter_metadata_write_descriptor(
        SZrCompilerState *cs,
        SZrObject *descriptor,
        const SZrFunctionMetadataParameter *parameter,
        TZrUInt32 position);

TZrBool compiler_parameter_metadata_attach_member_array(
        SZrCompilerState *cs,
        SZrTypeMemberInfo *memberInfo,
        SZrAstNodeArray *params,
        SZrAstNode *functionNode,
        const TZrChar *fieldName);

#endif
