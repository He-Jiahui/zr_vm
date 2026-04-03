//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_CORE_CONSTANT_REFERENCE_H
#define ZR_VM_CORE_CONSTANT_REFERENCE_H

#include "zr_vm_core/conf.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/function.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_common/zr_type_conf.h"

struct SZrState;
struct SZrFunction;
struct SZrObjectModule;

// 编译时和运行时共享的prototype序列化结构定义
// 这些结构用于将prototype信息序列化为紧凑二进制格式存储到常量池
// 布局：SZrCompiledPrototypeInfo + [inheritsCount * 4字节] + [membersCount * SZrCompiledMemberInfo]

// 编译时prototype信息结构（序列化格式头部）
// 这是磁盘/内存共享的二进制协议，必须显式禁止编译器填充。
#pragma pack(push, 1)
typedef struct SZrCompiledPrototypeInfo {
    TZrUInt32 nameStringIndex;              // 类型名称字符串在常量池中的索引
    TZrUInt32 type;                         // EZrObjectPrototypeType
    TZrUInt32 accessModifier;               // EZrAccessModifier
    TZrUInt32 inheritsCount;                // 继承类型数量
    TZrUInt32 membersCount;                 // 成员数量
    TZrUInt64 protocolMask;                 // 稳定 protocol bit mask
    // 注意：inheritStringIndices数组紧跟在结构体后面（不是指针）
    // 运行时通过 inheritsCount 和固定偏移量访问：offsetof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledPrototypeInfo)
    // 成员数据紧跟在继承数组后面：offsetof(SZrCompiledPrototypeInfo) + sizeof(SZrCompiledPrototypeInfo) + inheritsCount * sizeof(TZrUInt32)
    // 布局：SZrCompiledPrototypeInfo + [inheritsCount * 4字节] + [membersCount * SZrCompiledMemberInfo]
} SZrCompiledPrototypeInfo;

// 编译时成员信息结构（序列化格式）
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
    TZrUInt32 isUsingManaged;               // TZrBool (0或1)
    TZrUInt32 ownershipQualifier;           // EZrOwnershipQualifier（跨模块时按数值传递）
    TZrUInt32 callsClose;                   // TZrBool (0或1)
    TZrUInt32 callsDestructor;              // TZrBool (0或1)
    TZrUInt32 declarationOrder;             // 成员声明顺序
    TZrUInt32 contractRole;                 // EZrMemberContractRole
} SZrCompiledMemberInfo;
#pragma pack(pop)

// 常量引用路径结构（从parser模块引用）
typedef struct SZrConstantReferencePath {
    TZrUInt32 depth;              // 路径深度（总步骤数）
    TZrUInt32 *steps;             // 路径步骤数组（depth个元素）
    EZrValueType type;          // 常量类型记录
} SZrConstantReferencePath;

// 解析常量引用路径，返回目标对象
// 从startFunction开始，按照path中的步骤解析引用
// module: 模块上下文（可选，用于prototype延迟实例化和模块引用），如果为ZR_NULL则尝试从全局状态查找
// 返回：解析后的值（存储在result中），成功返回ZR_TRUE，失败返回ZR_FALSE
ZR_CORE_API TZrBool ZrCore_Constant_ResolveReference(
    struct SZrState *state,
    struct SZrFunction *startFunction,
    const SZrConstantReferencePath *path,
    struct SZrObjectModule *module,
    SZrTypeValue *result);

// 创建常量引用路径（分配内存）
// 返回：新创建的路径对象，失败返回ZR_NULL
ZR_CORE_API SZrConstantReferencePath *ZrCore_ConstantReferencePath_Create(
    struct SZrState *state,
    TZrUInt32 depth);

// 释放常量引用路径（释放内存）
ZR_CORE_API void ZrCore_ConstantReferencePath_Free(
    struct SZrState *state,
    SZrConstantReferencePath *path);

// 从常量池中的引用常量解析路径
// constant必须是引用类型的常量（type为ZR_VALUE_TYPE_OBJECT且internalType为特定标记，或特殊处理）
// 返回：解析后的路径对象，失败返回ZR_NULL
ZR_CORE_API SZrConstantReferencePath *ZrCore_ConstantReferencePath_FromConstant(
    struct SZrState *state,
    const SZrTypeValue *constant);

#endif // ZR_VM_CORE_CONSTANT_REFERENCE_H
