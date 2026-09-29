#ifndef ZR_VM_CORE_OBJECT_KNOWN_NATIVE_DISPATCH_H
#define ZR_VM_CORE_OBJECT_KNOWN_NATIVE_DISPATCH_H

#include "zr_vm_core/conf.h"

struct SZrObjectPrototype;
struct SZrState;
struct SZrTypeValue;
typedef struct ZrLibCallContext ZrLibCallContext;

/**
 * @brief 原生闭包上缓存的库绑定核心调用入口。
 *
 * 对象调用路径从缓存描述符构造 ZrLibCallContext，按需保护接收者和参数，
 * 进入描述符指定的 GC 安全点模式后再调用此回调。只有固定参数个数且带接收者、
 * 并且描述符允许栈根上下文的绑定才进入此直连路径；其他绑定仍由常规分派器处理。
 * @pre context 和 result 在回调执行期间有效。
 */
typedef TZrBool (*FZrObjectKnownNativeDirectCallback)(ZrLibCallContext *context, struct SZrTypeValue *result);

/**
 * @brief 对象和索引访问快速路径使用的只读内联读取入口。
 *
 * 回调接收 VM 状态、借用的接收者和键，并写入查找结果。只有缓存绑定声明了对应的
 * 只读内联契约，且操作数通过该路径的栈位置和生命周期检查时，调用方才选择它；
 * 它不能替代一般的绑定回调。
 * @pre 不得修改接收者，也不得在回调返回后继续持有借用值。
 */
typedef TZrBool (*FZrObjectKnownNativeReadonlyInlineGetFastCallback)(struct SZrState *state,
                                                                     const struct SZrTypeValue *selfValue,
                                                                     const struct SZrTypeValue *argument0,
                                                                     struct SZrTypeValue *result);

/**
 * @brief 结果可省略的只读内联设值入口。
 *
 * 索引契约路径只有在检查缓存参数个数和内联标志后，才传入借用的接收者、键和值。
 * 此回调没有结果槽；仅当绑定契约允许省略结果，且禁止修改或留存借用操作数时使用。
 * @pre 不得修改接收者，也不得在回调返回后继续持有借用值。
 */
typedef TZrBool (*FZrObjectKnownNativeReadonlyInlineSetNoResultFastCallback)(struct SZrState *state,
                                                                             const struct SZrTypeValue *selfValue,
                                                                             const struct SZrTypeValue *argument0,
                                                                             const struct SZrTypeValue *argument1);

/**
 * @brief 单个固定参数原生绑定的直连调用缓存。
 *
 * 原生闭包刷新绑定缓存时从函数、方法或元方法描述符填充此记录；对象调用和索引契约
 * 访问器读取它，以免重复解析描述符。描述符指针标识绑定种类；ownerPrototype、接收者
 * 和参数个数用于重建调用上下文。两个专用回调是可选的，只能启用各自通过契约检查的
 * 内联快速路径。
 * TODO: 对外部构造记录前，明确并强制描述符指针是否必须且只能设置一个，以及元数据的所有者和生命周期。
 * TODO: reserved0 和 reserved1 分别承载契约位和派生快速路径位；需与标志枚举及缓存刷新逻辑保持一致。
 */
typedef struct SZrObjectKnownNativeDirectDispatch {
    FZrObjectKnownNativeDirectCallback callback;
    const void *moduleDescriptor;
    const void *typeDescriptor;
    const void *functionDescriptor;
    const void *methodDescriptor;
    const void *metaMethodDescriptor;
    FZrObjectKnownNativeReadonlyInlineGetFastCallback readonlyInlineGetFastCallback;
    FZrObjectKnownNativeReadonlyInlineSetNoResultFastCallback readonlyInlineSetNoResultFastCallback;
    struct SZrObjectPrototype *ownerPrototype;
    TZrUInt32 rawArgumentCount;
    TZrBool usesReceiver;
    TZrUInt8 reserved0;
    TZrUInt16 reserved1;
} SZrObjectKnownNativeDirectDispatch;

/**
 * @brief 进入通用内联调用路径前检查的绑定保证。
 *
 * 闭包刷新缓存时从库描述符复制这些标志。它们限定可否修改接收者、结果是否总被写入，
 * 以及回调能否接收借用的内联值；这些不是调用方可随意设置的提示，必须符合回调的实际契约。
 */
typedef enum EZrObjectKnownNativeDirectDispatchFlag {
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NONE = 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_NO_SELF_REBIND = 1u << 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_INLINE_VALUE_CONTEXT = 1u << 1,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN = 1u << 2,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT = 1u << 3,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_FLAG_RESULT_OPTIONAL = 1u << 4
} EZrObjectKnownNativeDirectDispatchFlag;

/**
 * @brief 两种专用只读回调的派生就绪标志。
 *
 * Core 根据回调是否存在、接收者和参数个数形状以及所需契约位重新计算这些标志。
 * 使用方可依据就绪位跳过重复的形状检查；底层绑定记录变化时，刷新缓存必须重新计算。
 */
typedef enum EZrObjectKnownNativeDirectDispatchHotFlag {
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_NONE = 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_GET_FAST_READY = 1u << 0,
    ZR_OBJECT_KNOWN_NATIVE_DIRECT_DISPATCH_HOT_FLAG_READONLY_INLINE_SET_NO_RESULT_FAST_READY = 1u << 1
} EZrObjectKnownNativeDirectDispatchHotFlag;

#endif
