//
// Created by HeJiahui on 2025/6/4.
//

#ifndef ZR_VM_CORE_VERSION_INFO_H
#define ZR_VM_CORE_VERSION_INFO_H


#define ZR_VM_COPYRIGHT "Copyright (c) 2025 HeJiahui"
#define ZR_VM_AUTHOR "HeJiahui"
#define ZR_VM_DESCRIPTION "A simple virtual machine written in C"

#define ZR_VM_MAJOR_VERSION 0
#define ZR_VM_MINOR_VERSION 0
#define ZR_VM_PATCH_VERSION 1


// common definitions
#define ZR_EXPAND(x) x

#define ZR_AS_STR(x) #x
#define ZR_TO_STR(x) ZR_AS_STR(x)

// version string
#define ZR_VM_VERSION (ZR_TO_STR(ZR_VM_MAJOR_VERSION) "." ZR_TO_STR(ZR_VM_MINOR_VERSION) "." ZR_TO_STR(ZR_VM_PATCH_VERSION))


#endif //ZR_VM_CORE_VERSION_INFO_H
