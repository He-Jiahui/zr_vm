#ifndef ZR_VM_PARSER_TYPE_INFERENCE_NOMINAL_MEMBER_H
#define ZR_VM_PARSER_TYPE_INFERENCE_NOMINAL_MEMBER_H

#include "zr_vm_parser/compiler.h"

SZrTypePrototypeInfo *type_inference_find_member_relation_prototype_root(
        SZrCompilerState *cs, SZrString *typeName);
TZrBool type_inference_member_prototype_is_registered(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype);
TZrBool type_inference_member_prototypes_have_same_identity(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *left,
        const SZrTypePrototypeInfo *right);
SZrTypeMemberInfo *type_inference_find_member_on_prototype(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype,
        SZrString *memberName);
TZrBool type_inference_find_member_call_on_prototype(
        SZrCompilerState *cs, const SZrTypePrototypeInfo *prototype,
        SZrString *memberName, SZrFunctionCall *call, SZrFileRange location,
        SZrTypeMemberInfo **outMember);
SZrTypeMemberInfo *type_inference_find_receiver_member(
        SZrCompilerState *cs, const SZrInferredType *receiverType,
        SZrString *memberName);
TZrBool type_inference_find_receiver_member_call(
        SZrCompilerState *cs, const SZrInferredType *receiverType,
        SZrString *memberName, SZrFunctionCall *call, SZrFileRange location,
        SZrTypeMemberInfo **outMember);

#endif
