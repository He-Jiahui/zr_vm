//
// Created by HeJiahui on 2025/6/4.
//

#ifndef ZR_VERSION_INFO_H
#define ZR_VERSION_INFO_H


#define ZR_VM_COPYRIGHT "Copyright (c) 2025 HeJiahui"
#define ZR_VM_AUTHOR "HeJiahui"
#define ZR_VM_DESCRIPTION "A simple virtual machine written in C"
// SETUP VERSION
#define ZR_VM_MAJOR_VERSION 0
#define ZR_VM_MINOR_VERSION 0
#define ZR_VM_PATCH_VERSION 1
// SETUP COMPILER INFO
#ifndef ZR_VM_COMPILER_VERSION
#define ZR_VM_COMPILER_VERSION "unknown"
#endif
// SETUP DEBUG INFO
#ifdef ZR_DEBUG
#define ZR_VM_PUBLISH_VERSION "debug"
#elif ZR_RELEASE
#define ZR_VM_PUBLISH_VERSION "release"
#else
#define ZR_VM_PUBLISH_VERSION "unknown"
#endif
// SETUP PLATFORM INFO
#ifdef ZR_PLATFORM_WIN
#define ZR_VM_PLATFORM_INFO "win"
#define ZR_VM_PLATFORM_IS_WIN
#define ZR_SEPARATOR '\\'
#elif ZR_PLATFORM_DARWIN
#define ZR_VM_PLATFORM_INFO "darwin"
#define ZR_VM_PLATFORM_IS_DARWIN
#define ZR_SEPARATOR '/'
#elif ZR_PLATFORM_UNIX
#define ZR_VM_PLATFORM_INFO "unix"
#define ZR_VM_PLATFORM_IS_UNIX
#define ZR_SEPARATOR '/'
#else
#define ZR_VM_PLATFORM_INFO "unknown"
#define ZR_SEPARATOR '/'
#endif

// common definitions
#define ZR_EXPAND(x) x

#define ZR_AS_STR(x) #x
#define ZR_TO_STR(x) ZR_AS_STR(x)

// version string
#define ZR_VM_VERSION                                                                                                  \
    (ZR_TO_STR(ZR_VM_MAJOR_VERSION) "." ZR_TO_STR(ZR_VM_MINOR_VERSION) "." ZR_TO_STR(ZR_VM_PATCH_VERSION))
#define ZR_VM_VERSION_FULL                                                                                             \
    (ZR_TO_STR(ZR_VM_MAJOR_VERSION) "." ZR_TO_STR(ZR_VM_MINOR_VERSION) "." ZR_TO_STR(                                  \
            ZR_VM_PATCH_VERSION) "-" ZR_VM_PLATFORM_INFO "-" ZR_VM_PUBLISH_VERSION "-" ZR_VM_COMPILER_VERSION)


#endif // ZR_VERSION_INFO_H
