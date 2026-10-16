#ifndef ZR_VM_TESTS_UNITY_CONFIG_H
#define ZR_VM_TESTS_UNITY_CONFIG_H

/* Parse CRT before Unity's stdnoreturn.h defines noreturn. */
#include <stdlib.h>

/* 测试夹具的生命周期仍由各目标的 setUp/tearDown 提供。 */

#endif
