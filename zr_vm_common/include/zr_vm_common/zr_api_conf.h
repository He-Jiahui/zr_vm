//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_API_CONF_H
#define ZR_API_CONF_H

/**
 * @brief 首方模块公开声明的符号导出约定。
 * @note 共享 Windows 目标由 zr_declare_module 定义 ZR_LIBRARY_TYPE_SHARED；其他配置保留普通 extern 声明。
 */
/* TODO: 依赖别的首方库的共享模块也定义 ZR_LIBRARY_TYPE_SHARED；
 * 用 MSVC 导出表核查依赖头中的 ZR_API 是否把非本模块符号误标为 dllexport。 */
#if ZR_PLATFORM_WIN && ZR_LIBRARY_TYPE_SHARED // && ZR_PLATFORM_WIN_USE_MSVC
#define ZR_API __declspec(dllexport)
/* TODO: 全仓未发现 ZR_API_IMPORT 使用；核查 Windows 客户端是否依赖隐式导入后再决定保留。 */
#define ZR_API_IMPORT __declspec(dllimport)
#else
#define ZR_API extern
#define ZR_API_IMPORT extern
#endif


/** @brief 未由模块构建目标提供名称时的诊断标识；产品模块通常在 CMake 中覆盖。 */
#ifndef ZR_CURRENT_MODULE
#define ZR_CURRENT_MODULE "zr_vm_common"
#endif


#endif // ZR_API_CONF_H
