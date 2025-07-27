//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_API_CONF_H
#define ZR_API_CONF_H

/**
 *
 */
#if ZR_PLATFORM_WIN && ZR_LIBRARY_TYPE_SHARED // && ZR_PLATFORM_WIN_USE_MSVC
#define ZR_API __declspec(dllexport)
#define ZR_API_IMPORT __declspec(dllimport)
#else
#define ZR_API extern
#define ZR_API_IMPORT extern
#endif


#ifndef ZR_CURRENT_MODULE
#define ZR_CURRENT_MODULE "zr_vm_common"
#endif


#endif // ZR_API_CONF_H
