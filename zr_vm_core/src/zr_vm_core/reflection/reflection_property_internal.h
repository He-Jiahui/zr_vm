#ifndef ZR_VM_CORE_REFLECTION_PROPERTY_INTERNAL_H
#define ZR_VM_CORE_REFLECTION_PROPERTY_INTERNAL_H

#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"

/**
 * @brief 为脚本属性反射提供由 reflection.c 实现的对象创建、元数据和字段写入回调。
 * @pre 每个回调都必须有效；传入运行时对象须属于同一 SZrState 的 GC 域。
 * @note 当前由 reflection_populate_script_members 在一次同步构造中借用；属性构造器不缓存 host 指针。
 *       void 回调不返回逐字段成功状态，底层分配或插入失败时可能留下不完整的反射信息。
 */
typedef struct SZrReflectionPropertyHost {
    /** @brief 创建并初始化属性或访问器的通用成员反射对象；对象分配失败时返回 NULL。 */
    SZrObject *(*buildMemberInfo)(SZrState *state,
                                 const TZrChar *name,
                                 const TZrChar *qualifiedName,
                                 const TZrChar *kind,
                                 TZrUInt64 hash);
    /** @brief 将成员反射对象连接到所属类型及可选模块；引用通过反射字段保留。 */
    void (*assignOwnerLinks)(SZrState *state,
                             SZrObject *memberReflection,
                             SZrObject *ownerReflection,
                             SZrObject *moduleReflection);
    /** @brief 从编译成员记录填充修饰符、继承槽位和属性身份等反射元数据。 */
    void (*populateCompiledMetadata)(SZrState *state,
                                     SZrObject *memberReflection,
                                     SZrFunction *entryFunction,
                                     const SZrCompiledMemberInfo *member);
    /** @brief 从编译成员及 entryFunction 常量填充装饰器名称和 metadata。 */
    void (*populateDecoratorMetadata)(SZrState *state,
                                      SZrObject *memberReflection,
                                      SZrFunction *entryFunction,
                                      const SZrCompiledMemberInfo *member);
    /** @brief 由函数常量索引解析可调用函数；索引无效或常量不可解析时返回 NULL。 */
    SZrFunction *(*extractFunction)(SZrState *state,
                                    SZrFunction *entryFunction,
                                    TZrUInt32 constantIndex);
    /** @brief 按调用方给出的可见参数数目填充访问器的参数反射信息。 */
    void (*populateParameters)(SZrState *state,
                               SZrObject *callableReflection,
                               SZrFunction *function,
                               TZrUInt32 visibleParameterCount);
    /** @brief 将函数级源码和可调用元数据填入访问器反射对象。 */
    void (*populateFunctionMetadata)(SZrState *state,
                                     SZrObject *reflectionObject,
                                     SZrFunction *function);
    /** @brief 读取 entryFunction 的字符串常量；无法读取时返回 fallback，成功结果为借用指针。 */
    const TZrChar *(*stringFromConstant)(SZrState *state,
                                         SZrFunction *entryFunction,
                                         TZrUInt32 constantIndex,
                                         const TZrChar *fallback);
    /** @brief 在反射对象中写入布尔字段。 */
    void (*setFieldBool)(SZrState *state,
                         SZrObject *object,
                         const TZrChar *fieldName,
                         TZrBool value);
    /** @brief 在反射对象中写入整数数值字段。 */
    void (*setFieldInt)(SZrState *state,
                        SZrObject *object,
                        const TZrChar *fieldName,
                        TZrInt64 value);
    /**
     * @brief 将非空原生字符串复制为反射字符串字段；空字符串写为 null，空指针不操作。
     * BUG: propertyReflection/accessorReflection 在发布前仅由 C 局部变量持有；值字符串分配的 OOM 回收可清除接收对象，后续字段写入使用失效指针。
     *      两条窗口：reflection_property.c:270/296→416 和 :119/140→398；reflection.c:375 先分配值字符串，:332 才 pin 接收对象。
     *      证据链：string.c:914-917/725/733/743、gc_cycle.c:3137-3139、gc_internal.h:51-60、gc_mark.c:725-765、gc_sweep.c:27-35。
     */
    void (*setFieldString)(SZrState *state,
                           SZrObject *object,
                           const TZrChar *fieldName,
                           const TZrChar *value);
    /** @brief 以匹配对象实际类型的 valueType 写入对象引用并建立反射对象图边。 */
    void (*setFieldObject)(SZrState *state,
                           SZrObject *object,
                           const TZrChar *fieldName,
                           SZrObject *fieldObject,
                           EZrValueType valueType);
    /** @brief 按成员名将反射对象追加到 members 集合对应的名称桶。 */
    void (*addNamedEntry)(SZrState *state,
                          SZrObject *membersObject,
                          const TZrChar *memberName,
                          SZrObject *entryObject);
} SZrReflectionPropertyHost;

/**
 * @brief 判断编译成员是否为属性的规范承载项。
 * @return 仅当成员非空、声明类型为属性、没有访问器角色且具有有效属性身份时返回 true。
 */
TZrBool ZrCore_ReflectionProperty_IsCanonicalCarrier(
        const SZrCompiledMemberInfo *member);
/**
 * @brief 判断通用成员反射循环是否应略过规范属性或其已由规范属性承载的访问器。
 * @return 规范承载项总是略过；只有在成员表中找到同身份承载项的访问器才略过。
 */
TZrBool ZrCore_ReflectionProperty_ShouldSkipCanonicalMember(
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        const SZrCompiledMemberInfo *member);
/**
 * @brief 将编译成员表中的规范属性及其访问器写入类型的 members 反射集合。
 * @pre state、membersObject、typeReflection、prototype、qualifiedTypeName、entryFunction、members 和 host 必须有效；
 *      host 的全部回调必须非空且与这些对象属于同一运行时状态。moduleReflection 可以为空。
 *      调用方须保证运行时对象和编译成员记录在本次同步构造期间保持有效；GC 对象需保持根可达。
 * @note 参数和常量字符串均为借用数据，不会由本函数释放。无效顶层参数时直接返回；缺少属性名、
 *       不支持的访问器角色或成员对象分配失败时跳过相应项，void 回调的失败无法由本函数报告。
 */
void ZrCore_ReflectionProperty_PopulateCurrent(
        SZrState *state,
        SZrObject *membersObject,
        SZrObject *typeReflection,
        SZrObject *moduleReflection,
        SZrObjectPrototype *prototype,
        const TZrChar *qualifiedTypeName,
        SZrFunction *entryFunction,
        const SZrCompiledMemberInfo *members,
        TZrUInt32 memberCount,
        const SZrReflectionPropertyHost *host);

#endif
