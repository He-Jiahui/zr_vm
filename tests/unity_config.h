#ifndef ZR_VM_TESTS_UNITY_CONFIG_H
#define ZR_VM_TESTS_UNITY_CONFIG_H

#include "harness/unity_crash_guard.h"

#define UNITY_TEST_PROTECT() ZrTests_Unity_TestProtect()
#define UNITY_TEST_ABORT() ZrTests_Unity_TestAbort()

#endif
