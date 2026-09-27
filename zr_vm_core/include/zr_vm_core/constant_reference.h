//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_CORE_CONSTANT_REFERENCE_H
#define ZR_VM_CORE_CONSTANT_REFERENCE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/function.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_common/zr_type_conf.h"

struct SZrState;
struct SZrFunction;
struct SZrObjectModule;

/** 编译器写入、运行时物化和反射读取共用的 prototype blob 布局。
 * 函数的 prototypeData 先有数量头；每条记录随后依次包含本头部、
 * inheritsCount 个索引、decoratorsCount 个索引及 membersCount 个成员。
 * parser/compiler_internal.h 另有同布局镜像，字段顺序与打包方式须同步。 */

/** @brief 每条编译原型记录的固定头部；磁盘与内存共用无填充布局。 */
#pragma pack(push, 1)
typedef struct SZrCompiledPrototypeInfo {
    TZrUInt32 nameStringIndex;              // 类型名称字符串在常量池中的索引
    TZrUInt32 type;                         // EZrObjectPrototypeType
    TZrUInt32 accessModifier;               // EZrAccessModifier
    TZrUInt32 inheritsCount;                // 继承类型数量
    TZrUInt32 membersCount;                 // 成员数量
    TZrUInt64 protocolMask;                 // 稳定 protocol bit mask
    TZrUInt32 hasDecoratorMetadata;         // 是否存在 decorator metadata
    TZrUInt32 decoratorMetadataConstantIndex; // metadata 常量索引（hasDecoratorMetadata=1 时有效）
    TZrUInt32 decoratorsCount;              // decorator 名称记录数量
    TZrUInt32 modifierFlags;                // abstract/final 等类型修饰符
    TZrUInt32 nextVirtualSlotIndex;         // 当前类型累计 virtual slot 数
    TZrUInt32 nextPropertyIdentity;         // 当前类型累计 property identity 数
    TZrUInt32 layoutByteSize;               // inline struct/class 布局总大小（字节）
    TZrUInt32 layoutByteAlign;              // inline struct/class 布局最大对齐（字节）
    // 注意：inheritStringIndices数组紧跟在结构体后面（不是指针）
    // 运行时通过 inheritsCount 和固定偏移量访问：offsetof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledPrototypeInfo)
    // decorator 名称数组紧跟在继承数组后面：
    // offsetof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledPrototypeInfo) + inheritsCount * sizeof(TZrUInt32)
    // 成员数据紧跟在 decorator 数组后面。
} SZrCompiledPrototypeInfo;

/** @brief 原型的固定 31 字成员记录；字段类别决定复用槽位的解释。 */
typedef struct SZrCompiledMemberInfo {
    TZrUInt32 memberType;                   // EZrAstNodeType
    TZrUInt32 nameStringIndex;              // 成员名称字符串在常量池中的索引（如果为0表示无名）
    TZrUInt32 accessModifier;               // EZrAccessModifier
    TZrUInt32 isStatic;                     // TZrBool (0或1)
    TZrUInt32 isConst;                      // TZrBool (0或1)
    
    // 字段特定信息（仅当memberType为STRUCT_FIELD或CLASS_FIELD时有效）
    TZrUInt32 fieldTypeNameStringIndex;     // 字段类型名称字符串索引（如果为0表示无类型名）
    TZrUInt32 fieldOffset;                  // 字段偏移量
    TZrUInt32 fieldSize;                    // 字段大小
    
    // 方法特定信息（仅当memberType为METHOD或META_FUNCTION时有效）
    TZrUInt32 isMetaMethod;                 // TZrBool (0或1)
    TZrUInt32 metaType;                     // EZrMetaType
    TZrUInt32 functionConstantIndex;        // 函数在常量池中的索引
    TZrUInt32 parameterCount;               // 参数数量
    TZrUInt32 returnTypeNameStringIndex;    // 返回类型名称字符串索引（如果为0表示无返回类型名）
    TZrUInt32 reservedRemovedUsingManaged;  // reserved ABI slot; must remain zero
    TZrUInt32 ownershipQualifier;           // EZrOwnershipQualifier（跨模块时按数值传递）
    TZrUInt32 callsClose;                   // TZrBool (0或1)
    TZrUInt32 callsDestructor;              // TZrBool (0或1)
    TZrUInt32 declarationOrder;             // 成员声明顺序
    TZrUInt32 contractRole;                 // EZrMemberContractRole
    TZrUInt32 hasDecoratorMetadata;         // 是否存在成员级 compile-time decorator metadata
    TZrUInt32 decoratorMetadataConstantIndex; // metadata 常量索引（hasDecoratorMetadata=1 时有效）
    TZrUInt32 hasDecoratorNames;            // 是否存在成员级 decorator 名称数组
    TZrUInt32 decoratorNamesConstantIndex;  // decorator 名称数组常量索引（hasDecoratorNames=1 时有效）
    TZrUInt32 modifierFlags;                // abstract/virtual/override/final/shadow
    TZrUInt32 ownerTypeNameStringIndex;     // 当前成员所属类型
    TZrUInt32 baseDefinitionOwnerTypeNameStringIndex; // 覆写链根定义所属类型
    TZrUInt32 baseDefinitionNameStringIndex; // 覆写链根定义名称
    TZrUInt32 virtualSlotIndex;             // virtual slot；无效时为 UINT32_MAX
    TZrUInt32 interfaceContractSlot;        // interface contract slot；无效时为 UINT32_MAX
    TZrUInt32 propertyIdentity;             // property identity；无效时为 UINT32_MAX
    TZrUInt32 accessorRole;                 // 0 none, 1 getter, 2 setter, 3 initializer
} SZrCompiledMemberInfo;

#pragma pack(pop)

/* v34 成员 ABI 固定为 31 个 32 位字，禁止无版本扩展。 */
typedef char ZrCompiledMemberInfoV34LayoutMustRemainStable[
        sizeof(SZrCompiledMemberInfo) == 31U * sizeof(TZrUInt32) ? 1 : -1];

/*
 * A visible property carrier has no method payload of its own.  Current
 * executable metadata therefore uses the otherwise-empty method slots below
 * without changing the stable v34 31-word layout.  Accessor rows continue to
 * use the original method meanings.
 */
/* 可见属性 carrier 借用无方法载荷的旧槽位；方法与访问器仍按原语义读取。 */
#define ZR_COMPILED_PROPERTY_VALUE_TYPE_ID(MEMBER) ((MEMBER)->parameterCount)
#define ZR_COMPILED_PROPERTY_REFERENCE_ACCESS(MEMBER) ((MEMBER)->metaType)
#define ZR_COMPILED_PROPERTY_EXPORTS_WRITABLE_REF(MEMBER) ((MEMBER)->isMetaMethod)

/* 与 parser/compiler.h 的同名声明共用 guard，保持字段布局一致。 */
#ifndef ZR_CONSTANT_REFERENCE_PATH_DECLARED
#define ZR_CONSTANT_REFERENCE_PATH_DECLARED
/** @brief 带符号步骤以 uint32 存储的常量引用路径。
 * @note steps 由创建者分配；Create/FromConstant 的结果须用 Free 释放。 */
typedef struct SZrConstantReferencePath {
    TZrUInt32 depth;              // 路径深度（总步骤数）
    TZrUInt32 *steps;             // 路径步骤数组（depth个元素）
    EZrValueType type;          // 常量类型记录
} SZrConstantReferencePath;
#endif

/** @brief 从函数与可选模块上下文沿路径解析目标值。
 * @pre path->steps 含 depth 个已初始化步骤；需要模块查找时 state->global 有效。
 * @return 成功写入 result 并返回 true；失败返回 false。
 * @note TODO: 仓内没有外部调用；非函数末端的结果语义仍须对照未来消费方核实。 */
ZR_CORE_API TZrBool ZrCore_Constant_ResolveReference(
    struct SZrState *state,
    struct SZrFunction *startFunction,
    const SZrConstantReferencePath *path,
    struct SZrObjectModule *module,
    SZrTypeValue *result);

/** @brief 分配路径结构和未初始化的步骤数组。
 * @pre state 及 state->global 有效，depth 大于零。
 * @return 成功后由调用方持有，并用相同全局分配器对应的 Free 释放；失败返回 null。 */
ZR_CORE_API SZrConstantReferencePath *ZrCore_ConstantReferencePath_Create(
    struct SZrState *state,
    TZrUInt32 depth);

/** @brief 释放路径和步骤数组；state 须对应创建时的全局分配器。 */
ZR_CORE_API void ZrCore_ConstantReferencePath_Free(
    struct SZrState *state,
    SZrConstantReferencePath *path);

/** @brief 从二进制字符串常量解析 [depth, steps...] 路径。
 * @return 成功返回需用 Free 释放的原生路径；类型、长度或分配失败返回 null。
 * @note TODO: 当前编译器路径生成函数及此解码入口都没有实际调用者，
 * 须从未来消费方确认二进制字符串的来源与边界。 */
ZR_CORE_API SZrConstantReferencePath *ZrCore_ConstantReferencePath_FromConstant(
    struct SZrState *state,
    const SZrTypeValue *constant);

#endif // ZR_VM_CORE_CONSTANT_REFERENCE_H
