#ifndef ZR_VM_TESTS_CONTAINER_TEST_COMMON_H
#define ZR_VM_TESTS_CONTAINER_TEST_COMMON_H

#include "zr_vm_core/module.h"
#include "zr_vm_parser/compiler.h"

/** @brief 创建已注册容器及其依赖原生模块的独立测试 VM。
 *  @return 测试持有的主线程状态；创建失败时为 ZR_NULL。
 *  @note 返回状态由 ZrContainerTests_DestroyState 释放，不能与其他测试共享。
 */
SZrState *ZrContainerTests_CreateState(void);

/** @brief 释放 CreateState 创建的全局状态及其主线程。 */
void ZrContainerTests_DestroyState(SZrState *state);

/** @brief 在测试 VM 内创建编译器状态；调用方先销毁编译器再销毁 VM。 */
SZrCompilerState *ZrContainerTests_CreateCompilerState(SZrState *state);

/** @brief 清理 CreateCompilerState 的编译器状态和外层分配。 */
void ZrContainerTests_DestroyCompilerState(SZrCompilerState *cs);

/** @brief 按声明种类调用编译器入口，供测试逐条检查类型注册和错误状态。 */
void ZrContainerTests_CompileTopLevelStatement(SZrCompilerState *cs, SZrAstNode *node);

/** @brief 在当前编译器的类型原型表中查找闭合类型名。
 *  @note 返回指针借用自 cs；编译器销毁后失效。
 */
const SZrTypePrototypeInfo *ZrContainerTests_FindTypePrototype(const SZrCompilerState *cs, const char *typeName);

/** @brief 检查同名闭合类型是否被重复注册。 */
TZrSize ZrContainerTests_CountTypePrototypes(const SZrCompilerState *cs, const char *typeName);

/** @brief 按公开成员名查找原型元数据；结果借用自 prototype。 */
const SZrTypeMemberInfo *ZrContainerTests_FindTypeMemberByName(const SZrTypePrototypeInfo *prototype,
                                                               const char *memberName);

/** @brief 按元方法角色查找原型元数据；结果借用自 prototype。 */
const SZrTypeMemberInfo *ZrContainerTests_FindMetaMember(const SZrTypePrototypeInfo *prototype,
                                                         EZrMetaType metaType);

/** @brief 走 VM 模块导入路径读取原生模块。 */
SZrObjectModule *ZrContainerTests_ImportNativeModule(SZrState *state, const TZrChar *moduleName);

/** @brief 读取已导入模块的公开导出值；结果借用自模块。 */
const SZrTypeValue *ZrContainerTests_GetModuleExportValue(SZrState *state,
                                                          SZrObjectModule *module,
                                                          const TZrChar *exportName);

/** @brief 用字符串键读取对象字段；结果借用自对象。 */
const SZrTypeValue *ZrContainerTests_GetObjectFieldValue(SZrState *state,
                                                         SZrObject *object,
                                                         const TZrChar *fieldName);

/** @brief 读取底层 super array 长度；非数组输入返回 0。 */
TZrSize ZrContainerTests_GetArrayLength(SZrObject *array);

/** @brief 读取数组中的对象条目；非对象条目或无效输入返回 ZR_NULL。 */
SZrObject *ZrContainerTests_GetArrayEntryObject(SZrState *state, SZrObject *array, TZrSize index);

/** @brief 读取数组条目值；结果借用自数组。 */
const SZrTypeValue *ZrContainerTests_GetArrayEntryValue(SZrState *state, SZrObject *array, TZrSize index);

/** @brief 在模块信息数组中按指定字符串字段定位条目对象。 */
SZrObject *ZrContainerTests_FindNamedEntryInArray(SZrState *state,
                                                  SZrObject *array,
                                                  const TZrChar *fieldName,
                                                  const TZrChar *expectedValue);

/** @brief 比较 VM 字符串与测试侧 C 字符串。 */
TZrBool ZrContainerTests_StringEqualsCString(SZrString *value, const TZrChar *expected);

#endif
