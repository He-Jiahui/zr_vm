#ifndef ZR_VM_PARSER_COMPILER_PROPERTY_H
#define ZR_VM_PARSER_COMPILER_PROPERTY_H

#include "zr_vm_parser/compiler.h"

typedef enum EZrCompilerPropertyContainerKind {
    ZR_COMPILER_PROPERTY_CONTAINER_CLASS = 0,
    ZR_COMPILER_PROPERTY_CONTAINER_STRUCT,
    ZR_COMPILER_PROPERTY_CONTAINER_INTERFACE,
} EZrCompilerPropertyContainerKind;

TZrBool compiler_property_bind(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *propertyNode,
        SZrString *ownerTypeName,
        SZrString *superTypeName,
        EZrCompilerPropertyContainerKind containerKind,
        TZrUInt32 declarationOrder);

#endif
