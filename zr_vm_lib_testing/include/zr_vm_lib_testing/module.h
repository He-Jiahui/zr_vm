#ifndef ZR_VM_LIB_TESTING_MODULE_H
#define ZR_VM_LIB_TESTING_MODULE_H

#include "zr_vm_library.h"

#define ZR_VM_LIB_TESTING_API ZR_API
/* 失败记录供测试宿主在断言抛出后读取，定长字段让宿主取得独立副本。 */
#define ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY 256U
#define ZR_VM_LIB_TESTING_MESSAGE_CAPACITY 256U
#define ZR_VM_LIB_TESTING_TYPE_NAME_CAPACITY 128U
#define ZR_VM_LIB_TESTING_SOURCE_FILE_CAPACITY 256U

/** @brief 区分宿主收到的结构化断言失败种类，而非底层 VM 异常状态。 */
typedef enum EZrTestingAssertionKind {
    ZR_TESTING_ASSERTION_KIND_ASSERT = 1,
    ZR_TESTING_ASSERTION_KIND_EQUAL = 2,
    ZR_TESTING_ASSERTION_KIND_THROWS = 3
} EZrTestingAssertionKind;

/** @brief 保存失败时的有界值视图；formatterFaulted 标明调试文本格式化失败。 */
typedef struct SZrTestingValueSnapshot {
    TZrChar typeName[ZR_VM_LIB_TESTING_TYPE_NAME_CAPACITY + 1U];
    TZrChar text[ZR_VM_LIB_TESTING_SNAPSHOT_CAPACITY + 1U];
    TZrBool hasValue;
    TZrBool truncated;
    TZrBool formatterFaulted;
} SZrTestingValueSnapshot;

/** @brief 保存调用点的源码位置；缺少 VM 调试位置时字段保持零值。 */
typedef struct SZrTestingSourceSpan {
    TZrChar sourceFile[ZR_VM_LIB_TESTING_SOURCE_FILE_CAPACITY + 1U];
    TZrUInt32 startLine;
    TZrUInt32 startColumn;
    TZrUInt32 endLine;
    TZrUInt32 endColumn;
} SZrTestingSourceSpan;

/** @brief 宿主从当前线程取出的失败副本，同时映射为脚本可捕获的 AssertionFailure。 */
typedef struct SZrTestingAssertionFailure {
    EZrTestingAssertionKind assertionKind;
    SZrTestingSourceSpan sourceSpan;
    TZrChar message[ZR_VM_LIB_TESTING_MESSAGE_CAPACITY + 1U];
    SZrTestingValueSnapshot expected;
    SZrTestingValueSnapshot actual;
    SZrTestingValueSnapshot exception;
} SZrTestingAssertionFailure;

/** @brief 返回测试阶段提供者的静态描述符，供内建注册和测试宿主检查公开契约。 */
ZR_VM_LIB_TESTING_API const ZrLibModuleDescriptor *ZrVmLibTesting_GetModuleDescriptor(void);
/** @brief 在目标 VM 全局状态注册测试提供者及其任务依赖；访问导出前宿主须选择测试阶段。 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Register(SZrGlobalState *global);
/** @brief 清除当前线程的上一次断言记录；CLI 在用例执行前及失败报告读取后调用。 */
ZR_VM_LIB_TESTING_API void ZrVmLibTesting_ClearLastFailure(void);
/** @brief 将当前线程的失败记录复制给宿主；没有记录或输出指针为空时返回假。 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_GetLastFailure(SZrTestingAssertionFailure *outFailure);
/** @brief `zr.testing.assert` 的原生入口；条件失败时抛出结构化 AssertionFailure。 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Assert(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief `zr.testing.equal<T>` 的原生入口，使用 VM 规范相等语义并保存失败快照。 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Equal(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief `zr.testing.throws<E>` 的原生入口；公开签名要求同步无参动作和 Error 派生类型。 */
ZR_VM_LIB_TESTING_API TZrBool ZrVmLibTesting_Throws(ZrLibCallContext *context, SZrTypeValue *result);

#if defined(ZR_LIBRARY_TYPE_SHARED)
/** @brief 动态加载器约定的固定 v1 入口符号；描述符 ABI 版本另行校验。 */
ZR_VM_LIB_TESTING_API const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
#endif

#endif
