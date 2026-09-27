//
// Native binding descriptors and helpers for zr_vm_library.
//

#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_H

#include "zr_vm_library/conf.h"
#include "zr_vm_library/native_call_plan.h"
#include "zr_vm_library/zrm.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_common/zr_contract_conf.h"

struct ZrLibModuleDescriptor;
struct ZrLibTypeDescriptor;
struct ZrLibFunctionDescriptor;
struct ZrLibMethodDescriptor;
struct ZrLibMetaMethodDescriptor;
struct ZrLibEnumMemberDescriptor;
struct SZrCallInfo;

/** @brief 模块描述符声明运行时必须提供的能力；注册与调用计划共用此集合做兼容性检查。 */
typedef enum EZrLibModuleCapability {
    ZR_LIB_MODULE_CAPABILITY_NONE = 0,
    ZR_LIB_MODULE_CAPABILITY_TYPE_HINTS = 1u << 0,
    ZR_LIB_MODULE_CAPABILITY_TYPE_METADATA = 1u << 1,
    ZR_LIB_MODULE_CAPABILITY_ENUM_INTERFACE_METADATA = 1u << 2,
    ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS = 1u << 3,
    ZR_LIB_MODULE_CAPABILITY_FFI_RUNTIME = 1u << 4
} EZrLibModuleCapability;

/* 注册器与调用计划使用同一能力上界，避免编译期可见而执行期不可用的契约。 */
#define ZR_VM_NATIVE_RUNTIME_CAPABILITIES                                                                               \
    ((TZrUInt64)(ZR_LIB_MODULE_CAPABILITY_TYPE_HINTS | ZR_LIB_MODULE_CAPABILITY_TYPE_METADATA |                        \
                 ZR_LIB_MODULE_CAPABILITY_ENUM_INTERFACE_METADATA | ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS |       \
                 ZR_LIB_MODULE_CAPABILITY_FFI_RUNTIME))

/** @brief 常量描述符的序列化类别，供模块物化与编译期投影共享。 */
typedef enum EZrLibConstantKind {
    ZR_LIB_CONSTANT_KIND_NULL = 0,
    ZR_LIB_CONSTANT_KIND_BOOL = 1,
    ZR_LIB_CONSTANT_KIND_INT = 2,
    ZR_LIB_CONSTANT_KIND_FLOAT = 3,
    ZR_LIB_CONSTANT_KIND_STRING = 4,
    ZR_LIB_CONSTANT_KIND_ARRAY = 5
} EZrLibConstantKind;

/** @brief 暴露给类型推断和工具链的符号提示；文本由描述符提供者维持有效。 */
typedef struct ZrLibTypeHintDescriptor {
    const TZrChar *symbolName;
    const TZrChar *symbolKind;
    const TZrChar *signature;
    const TZrChar *documentation;
} ZrLibTypeHintDescriptor;

/** @brief 字段的公开契约与运行时可见性，物化器据此生成成员和引用属性。 */
typedef struct ZrLibFieldDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
    TZrUInt32 contractRole;
    TZrBool runtimeOnly;
    TZrBool isReadonly;
} ZrLibFieldDescriptor;

/** @brief 属性引用的读写许可；编译器与运行时须对同一属性使用相同模式。 */
typedef enum EZrLibReferenceAccess {
    ZR_LIB_REFERENCE_ACCESS_NONE = 0,
    ZR_LIB_REFERENCE_ACCESS_WRITABLE = 1,
    ZR_LIB_REFERENCE_ACCESS_READONLY = 2
} EZrLibReferenceAccess;

/** @brief 参数传递契约，区分值、只读输入及需写回的 out/ref 参数。 */
typedef enum EZrLibParameterPassingMode {
    ZR_LIB_PARAMETER_PASSING_MODE_VALUE = 0,
    ZR_LIB_PARAMETER_PASSING_MODE_IN = 1,
    ZR_LIB_PARAMETER_PASSING_MODE_OUT = 2,
    ZR_LIB_PARAMETER_PASSING_MODE_REF = 3
} EZrLibParameterPassingMode;

/** @brief 可调用项的参数元数据；注册时校验 passingMode，调用绑定据此建立借用及写回契约。 */
typedef struct ZrLibParameterDescriptor {
    const TZrChar *name;
    const TZrChar *typeName;
    const TZrChar *documentation;
    EZrLibParameterPassingMode passingMode;
} ZrLibParameterDescriptor;

/** @brief 泛型形参及约束的只读描述，由类型推断和工具链消费。 */
typedef struct ZrLibGenericParameterDescriptor {
    const TZrChar *name;
    const TZrChar *documentation;
    const TZrChar *const *constraintTypeNames;
    TZrSize constraintTypeCount;
} ZrLibGenericParameterDescriptor;

/** @brief 枚举成员的类型和值描述，供模块物化与静态类型投影使用。 */
typedef struct ZrLibEnumMemberDescriptor {
    const TZrChar *name;
    EZrLibConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
} ZrLibEnumMemberDescriptor;

/** @brief 为 Native 值读取保留的借用视图；value 不转移所有权。
 * @note TODO: 首方调用链未发现构造或消费，需核实是否仍是对外 ABI 的必要组成。
 */
typedef struct ZrLibValueView {
    const SZrTypeValue *value;
    EZrValueType runtimeType;
    const TZrChar *typeName;
} ZrLibValueView;

/** @brief 内联实参在当前栈帧中的借用字节区间；栈增长或安全点后地址需重取。 */
typedef struct ZrLibInlineSpan {
    TZrPtr address;
    TZrUInt32 byteSize;
    TZrUInt32 byteAlign;
    TZrUInt32 typeLayoutId;
    TZrBool available;
} ZrLibInlineSpan;

/** @brief 将借用的内联区间绑定到已校验的布局注册表，供池化等零复制路径使用。 */
typedef struct ZrLibInlineArgumentView {
    ZrLibInlineSpan span;
    const SZrTypeLayout *typeLayout;
    SZrTypeLayoutRegistryView registry;
} ZrLibInlineArgumentView;

/** @brief 一次 Native 回调的描述符、实参及栈布局快照；仅在回调期间有效。
 * @note argumentValues 与栈地址可能因 GC 或栈增长改变，使用公开访问器重取。
 */
typedef struct ZrLibCallContext {
    SZrState *state;
    const struct ZrLibModuleDescriptor *moduleDescriptor;
    const struct ZrLibTypeDescriptor *typeDescriptor;
    const struct ZrLibFunctionDescriptor *functionDescriptor;
    const struct ZrLibMethodDescriptor *methodDescriptor;
    const struct ZrLibMetaMethodDescriptor *metaMethodDescriptor;
    struct SZrObjectPrototype *ownerPrototype;
    struct SZrObjectPrototype *constructTargetPrototype;
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argumentBase;
    SZrTypeValue *argumentValues;
    SZrTypeValue **argumentValuePointers;
    TZrSize argumentCount;
    SZrTypeValue *selfValue;
    SZrFunctionStackAnchor functionBaseAnchor;
    TZrStackValuePointer stackBasePointer;
    TZrSize rawArgumentCount;
    TZrBool stackLayoutUsesReceiver;
    TZrBool stackLayoutAnchored;
    const SZrFunction *inlineFrameFunction;
    TZrStackValuePointer inlineFrameBase;
    TZrUInt32 inlineArgumentStartSlot;
} ZrLibCallContext;

/** @brief 在 Native 回调内跨分配点保护临时值的栈根；Begin/End 必须成对使用。 */
typedef struct ZrLibTempValueRoot {
    SZrState *state;
    struct SZrCallInfo *callInfo;
    SZrFunctionStackAnchor savedStackTopAnchor;
    SZrFunctionStackAnchor savedCallInfoBaseAnchor;
    SZrFunctionStackAnchor savedCallInfoTopAnchor;
    SZrFunctionStackAnchor savedCallInfoReturnAnchor;
    SZrFunctionStackAnchor slotAnchor;
    TZrStackValuePointer savedStackTopPointer;
    TZrStackValuePointer savedCallInfoBasePointer;
    TZrStackValuePointer savedCallInfoTopPointer;
    TZrStackValuePointer savedCallInfoReturnPointer;
    TZrStackValuePointer slotPointer;
    TZrStackValuePointer stackBasePointer;
    TZrBool hasSavedCallInfoBase;
    TZrBool hasSavedCallInfoTop;
    TZrBool hasSavedCallInfoReturn;
    TZrBool restoreCallInfoTopFromSavedStackTop;
    TZrBool usesDirectPointers;
    TZrBool active;
} ZrLibTempValueRoot;

/** @brief 描述符的运行时调用入口；成功时按 dispatchFlags 写入 result。 */
typedef TZrBool (*FZrLibBoundCallback)(ZrLibCallContext *context, SZrTypeValue *result);
/** @brief 只读内联 getter 的受限快速路径，绕开通用调用上下文。 */
typedef TZrBool (*FZrLibMetaMethodReadonlyInlineGetFastCallback)(SZrState *state,
                                                                 const SZrTypeValue *selfValue,
                                                                 const SZrTypeValue *argument0,
                                                                 SZrTypeValue *result);
/** @brief 无结果内联 setter 的受限快速路径；调用方负责保证接收者布局有效。 */
typedef TZrBool (*FZrLibMetaMethodReadonlyInlineSetNoResultFastCallback)(SZrState *state,
                                                                         const SZrTypeValue *selfValue,
                                                                         const SZrTypeValue *argument0,
                                                                         const SZrTypeValue *argument1);

/** @brief Native 调用的栈根、结果和安全点约束；生成闭包与 core 快速分派必须一致解释。 */
typedef enum EZrLibNativeDispatchFlag {
    ZR_LIB_NATIVE_DISPATCH_FLAG_NONE = 0,
    ZR_LIB_NATIVE_DISPATCH_FLAG_GC_AWARE = 0,
    ZR_LIB_NATIVE_DISPATCH_FLAG_STACK_ROOT_CONTEXT = 1u << 0,
    ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SELF_REBIND = 1u << 1,
    ZR_LIB_NATIVE_DISPATCH_FLAG_INLINE_VALUE_CONTEXT = 1u << 2,
    ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_ALWAYS_WRITTEN = 1u << 3,
    ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_INLINE_VALUE_CONTEXT = 1u << 4,
    ZR_LIB_NATIVE_DISPATCH_FLAG_RESULT_OPTIONAL = 1u << 5,
    ZR_LIB_NATIVE_DISPATCH_FLAG_READONLY_RECEIVER = 1u << 6,
    ZR_LIB_NATIVE_DISPATCH_FLAG_BLOCKING_DETACHED = 1u << 7,
    ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SAFEPOINT_CRITICAL = 1u << 8,
    ZR_LIB_NATIVE_DISPATCH_FLAG_SAFEPOINT_MODE_MASK =
            ZR_LIB_NATIVE_DISPATCH_FLAG_BLOCKING_DETACHED |
            ZR_LIB_NATIVE_DISPATCH_FLAG_NO_SAFEPOINT_CRITICAL
} EZrLibNativeDispatchFlag;

/** @brief 模块函数的声明与回调契约；编译器、模块物化器及分派器共用同一描述符。 */
typedef struct ZrLibFunctionDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 contractRole;
    TZrUInt32 dispatchFlags;
} ZrLibFunctionDescriptor;

/** @brief 类型方法契约，含静态性及属性引用语义；物化后的闭包借用此描述符。 */
typedef struct ZrLibMethodDescriptor {
    const TZrChar *name;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    TZrBool isStatic;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    TZrUInt32 contractRole;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 dispatchFlags;
    const TZrChar *propertyName;
    EZrLibReferenceAccess propertyReferenceAccess;
    TZrBool propertyExportsWritableRef;
} ZrLibMethodDescriptor;

/** @brief 运算符/元方法契约；可选快速回调只适用于声明的内联布局和分派标志。 */
typedef struct ZrLibMetaMethodDescriptor {
    EZrMetaType metaType;
    TZrUInt16 minArgumentCount;
    TZrUInt16 maxArgumentCount;
    FZrLibBoundCallback callback;
    const TZrChar *returnTypeName;
    const TZrChar *documentation;
    const ZrLibParameterDescriptor *parameters;
    TZrSize parameterCount;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt32 dispatchFlags;
    FZrLibMetaMethodReadonlyInlineGetFastCallback readonlyInlineGetFastCallback;
    FZrLibMetaMethodReadonlyInlineSetNoResultFastCallback readonlyInlineSetNoResultFastCallback;
    TZrUInt32 contractRole;
} ZrLibMetaMethodDescriptor;

/** @brief 模块常量及其显式类型名称，供物化与编译期公共契约投影。 */
typedef struct ZrLibConstantDescriptor {
    const TZrChar *name;
    EZrLibConstantKind kind;
    TZrInt64 intValue;
    TZrFloat64 floatValue;
    const TZrChar *stringValue;
    TZrBool boolValue;
    const TZrChar *documentation;
    const TZrChar *typeName;
} ZrLibConstantDescriptor;

/** @brief 模块导出的依赖链接；物化器按名称导入目标模块。 */
typedef struct ZrLibModuleLinkDescriptor {
    const TZrChar *name;
    const TZrChar *moduleName;
    const TZrChar *documentation;
} ZrLibModuleLinkDescriptor;

/** @brief 属性的稳定角色 ID，供编译器识别原生注解而不依赖显示名称。 */
typedef struct ZrLibAttributeRoleDescriptor {
    const TZrChar *qualifiedName;
    TZrUInt32 attributeId;
    TZrUInt32 role;
    TZrUInt32 targetFlags;
    TZrUInt32 retention;
    TZrBool repeatable;
    const TZrChar *typeName;
} ZrLibAttributeRoleDescriptor;

/** @brief 提供者公开的规范类型角色和投影方式；类型推断按角色查找唯一提供者。 */
typedef struct ZrLibCanonicalTypeRoleDescriptor {
    const TZrChar *canonicalName;
    EZrCanonicalTypeRole role;
    EZrCanonicalTypeRole parentRole;
    TZrUInt32 surfaceFlags;
    EZrCanonicalTypeProjectionKind projectionKind;
} ZrLibCanonicalTypeRoleDescriptor;

/** @brief 模块物化后的提供者扩展入口；仅在描述符仍存活时由加载器调用。 */
typedef TZrBool (*FZrLibModuleMaterializeCallback)(SZrState *state,
                                                   struct SZrObjectModule *module,
                                                   const struct ZrLibModuleDescriptor *descriptor);

/** @brief 原生类型的完整公开契约，包含成员、协议、构造和 FFI 降低信息。
 * @note 所有数组与字符串均由提供者持有；注册表只借用 descriptor 指针。
 */
typedef struct ZrLibTypeDescriptor {
    const TZrChar *name;
    EZrObjectPrototypeType prototypeType;
    const ZrLibFieldDescriptor *fields;
    TZrSize fieldCount;
    const ZrLibMethodDescriptor *methods;
    TZrSize methodCount;
    const ZrLibMetaMethodDescriptor *metaMethods;
    TZrSize metaMethodCount;
    const TZrChar *documentation;
    const TZrChar *extendsTypeName;
    const TZrChar *const *implementsTypeNames;
    TZrSize implementsTypeCount;
    const ZrLibEnumMemberDescriptor *enumMembers;
    TZrSize enumMemberCount;
    const TZrChar *enumValueTypeName;
    TZrBool allowValueConstruction;
    TZrBool allowBoxedConstruction;
    const TZrChar *constructorSignature;
    const ZrLibGenericParameterDescriptor *genericParameters;
    TZrSize genericParameterCount;
    TZrUInt64 protocolMask;
    const TZrChar *ffiLoweringKind;
    const TZrChar *ffiViewTypeName;
    const TZrChar *ffiUnderlyingTypeName;
    const TZrChar *ffiOwnerMode;
    const TZrChar *ffiReleaseHook;
} ZrLibTypeDescriptor;

/* 不带契约角色的字段默认值；需要稳定角色时用 ROLE_INIT。 */
#define ZR_LIB_FIELD_DESCRIPTOR_INIT(NAME, TYPE_NAME, DOCUMENTATION)                                                  \
    {(NAME), (TYPE_NAME), (DOCUMENTATION), 0U, ZR_FALSE, ZR_FALSE}

/* 以稳定角色标记字段，供编译器与反射按契约查询。 */
#define ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(NAME, TYPE_NAME, DOCUMENTATION, CONTRACT_ROLE)                              \
    {(NAME), (TYPE_NAME), (DOCUMENTATION), (CONTRACT_ROLE), ZR_FALSE, ZR_FALSE}

/* 常规方法声明的默认属性与分派模式；变长描述符字段由此补齐。 */
#define ZR_LIB_METHOD_DESCRIPTOR_INIT(NAME, MIN_ARGUMENT_COUNT, MAX_ARGUMENT_COUNT, CALLBACK, RETURN_TYPE_NAME,       \
                                      DOCUMENTATION, IS_STATIC, PARAMETERS, PARAMETER_COUNT)                          \
    {(NAME),                                                                                                          \
     (MIN_ARGUMENT_COUNT),                                                                                            \
     (MAX_ARGUMENT_COUNT),                                                                                            \
     (CALLBACK),                                                                                                      \
     (RETURN_TYPE_NAME),                                                                                              \
     (DOCUMENTATION),                                                                                                 \
     (IS_STATIC),                                                                                                     \
     (PARAMETERS),                                                                                                    \
     (PARAMETER_COUNT),                                                                                               \
     0U,                                                                                                              \
     ZR_NULL,                                                                                                         \
     0U,                                                                                                              \
     0U,                                                                                                              \
     ZR_NULL,                                                                                                         \
     ZR_LIB_REFERENCE_ACCESS_NONE,                                                                                    \
     ZR_FALSE}

/* 将方法映射到跨模块合同角色，保留与普通方法相同的默认分派约束。 */
#define ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT(NAME, MIN_ARGUMENT_COUNT, MAX_ARGUMENT_COUNT, CALLBACK, RETURN_TYPE_NAME,  \
                                           DOCUMENTATION, IS_STATIC, PARAMETERS, PARAMETER_COUNT, CONTRACT_ROLE)      \
    {(NAME),                                                                                                          \
     (MIN_ARGUMENT_COUNT),                                                                                            \
     (MAX_ARGUMENT_COUNT),                                                                                            \
     (CALLBACK),                                                                                                      \
     (RETURN_TYPE_NAME),                                                                                              \
     (DOCUMENTATION),                                                                                                 \
     (IS_STATIC),                                                                                                     \
     (PARAMETERS),                                                                                                    \
     (PARAMETER_COUNT),                                                                                               \
     (CONTRACT_ROLE),                                                                                                 \
     ZR_NULL,                                                                                                         \
     0U,                                                                                                              \
     0U,                                                                                                              \
     ZR_NULL,                                                                                                         \
     ZR_LIB_REFERENCE_ACCESS_NONE,                                                                                    \
     ZR_FALSE}

/* 不声明协议或 FFI 降低方式的类型默认初始化，供静态模块表复用。 */
#define ZR_LIB_TYPE_DESCRIPTOR_INIT(NAME, PROTOTYPE_TYPE, FIELDS, FIELD_COUNT, METHODS, METHOD_COUNT, META_METHODS,  \
                                    META_METHOD_COUNT, DOCUMENTATION, EXTENDS_TYPE_NAME, IMPLEMENTS_TYPE_NAMES,       \
                                    IMPLEMENTS_TYPE_COUNT, ENUM_MEMBERS, ENUM_MEMBER_COUNT, ENUM_VALUE_TYPE_NAME,    \
                                    ALLOW_VALUE_CONSTRUCTION, ALLOW_BOXED_CONSTRUCTION, CONSTRUCTOR_SIGNATURE,        \
                                    GENERIC_PARAMETERS, GENERIC_PARAMETER_COUNT)                                      \
    {(NAME),                                                                                                          \
     (PROTOTYPE_TYPE),                                                                                                \
     (FIELDS),                                                                                                        \
     (FIELD_COUNT),                                                                                                   \
     (METHODS),                                                                                                       \
     (METHOD_COUNT),                                                                                                  \
     (META_METHODS),                                                                                                  \
     (META_METHOD_COUNT),                                                                                             \
     (DOCUMENTATION),                                                                                                 \
     (EXTENDS_TYPE_NAME),                                                                                             \
     (IMPLEMENTS_TYPE_NAMES),                                                                                         \
     (IMPLEMENTS_TYPE_COUNT),                                                                                         \
     (ENUM_MEMBERS),                                                                                                  \
     (ENUM_MEMBER_COUNT),                                                                                             \
     (ENUM_VALUE_TYPE_NAME),                                                                                          \
     (ALLOW_VALUE_CONSTRUCTION),                                                                                      \
     (ALLOW_BOXED_CONSTRUCTION),                                                                                      \
     (CONSTRUCTOR_SIGNATURE),                                                                                         \
     (GENERIC_PARAMETERS),                                                                                            \
     (GENERIC_PARAMETER_COUNT),                                                                                       \
     (TZrUInt64)0,                                                                                                    \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL}

/* 带协议掩码的类型声明，供协议投影和运行时物化共享。 */
#define ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT(NAME, PROTOTYPE_TYPE, FIELDS, FIELD_COUNT, METHODS, METHOD_COUNT,       \
                                             META_METHODS, META_METHOD_COUNT, DOCUMENTATION, EXTENDS_TYPE_NAME,       \
                                             IMPLEMENTS_TYPE_NAMES, IMPLEMENTS_TYPE_COUNT, ENUM_MEMBERS,              \
                                             ENUM_MEMBER_COUNT, ENUM_VALUE_TYPE_NAME, ALLOW_VALUE_CONSTRUCTION,       \
                                             ALLOW_BOXED_CONSTRUCTION, CONSTRUCTOR_SIGNATURE, GENERIC_PARAMETERS,     \
                                             GENERIC_PARAMETER_COUNT, PROTOCOL_MASK)                                  \
    {(NAME),                                                                                                          \
     (PROTOTYPE_TYPE),                                                                                                \
     (FIELDS),                                                                                                        \
     (FIELD_COUNT),                                                                                                   \
     (METHODS),                                                                                                       \
     (METHOD_COUNT),                                                                                                  \
     (META_METHODS),                                                                                                  \
     (META_METHOD_COUNT),                                                                                             \
     (DOCUMENTATION),                                                                                                 \
     (EXTENDS_TYPE_NAME),                                                                                             \
     (IMPLEMENTS_TYPE_NAMES),                                                                                         \
     (IMPLEMENTS_TYPE_COUNT),                                                                                         \
     (ENUM_MEMBERS),                                                                                                  \
     (ENUM_MEMBER_COUNT),                                                                                             \
     (ENUM_VALUE_TYPE_NAME),                                                                                          \
     (ALLOW_VALUE_CONSTRUCTION),                                                                                      \
     (ALLOW_BOXED_CONSTRUCTION),                                                                                      \
     (CONSTRUCTOR_SIGNATURE),                                                                                         \
     (GENERIC_PARAMETERS),                                                                                            \
     (GENERIC_PARAMETER_COUNT),                                                                                       \
     (PROTOCOL_MASK),                                                                                                 \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL,                                                                                                         \
     ZR_NULL}

/* 带 FFI 降低、视图和释放钩子的类型声明；描述符生命周期须覆盖外部句柄。 */
#define ZR_LIB_TYPE_DESCRIPTOR_FFI_INIT(NAME, PROTOTYPE_TYPE, FIELDS, FIELD_COUNT, METHODS, METHOD_COUNT,            \
                                        META_METHODS, META_METHOD_COUNT, DOCUMENTATION, EXTENDS_TYPE_NAME,           \
                                        IMPLEMENTS_TYPE_NAMES, IMPLEMENTS_TYPE_COUNT, ENUM_MEMBERS,                  \
                                        ENUM_MEMBER_COUNT, ENUM_VALUE_TYPE_NAME, ALLOW_VALUE_CONSTRUCTION,           \
                                        ALLOW_BOXED_CONSTRUCTION, CONSTRUCTOR_SIGNATURE, GENERIC_PARAMETERS,         \
                                        GENERIC_PARAMETER_COUNT, FFI_LOWERING_KIND, FFI_VIEW_TYPE_NAME,             \
                                        FFI_UNDERLYING_TYPE_NAME, FFI_OWNER_MODE, FFI_RELEASE_HOOK)                 \
    {(NAME),                                                                                                          \
     (PROTOTYPE_TYPE),                                                                                                \
     (FIELDS),                                                                                                        \
     (FIELD_COUNT),                                                                                                   \
     (METHODS),                                                                                                       \
     (METHOD_COUNT),                                                                                                  \
     (META_METHODS),                                                                                                  \
     (META_METHOD_COUNT),                                                                                             \
     (DOCUMENTATION),                                                                                                 \
     (EXTENDS_TYPE_NAME),                                                                                             \
     (IMPLEMENTS_TYPE_NAMES),                                                                                         \
     (IMPLEMENTS_TYPE_COUNT),                                                                                         \
     (ENUM_MEMBERS),                                                                                                  \
     (ENUM_MEMBER_COUNT),                                                                                             \
     (ENUM_VALUE_TYPE_NAME),                                                                                          \
     (ALLOW_VALUE_CONSTRUCTION),                                                                                      \
     (ALLOW_BOXED_CONSTRUCTION),                                                                                      \
     (CONSTRUCTOR_SIGNATURE),                                                                                         \
     (GENERIC_PARAMETERS),                                                                                            \
     (GENERIC_PARAMETER_COUNT),                                                                                       \
     (TZrUInt64)0,                                                                                                    \
     (FFI_LOWERING_KIND),                                                                                             \
     (FFI_VIEW_TYPE_NAME),                                                                                            \
     (FFI_UNDERLYING_TYPE_NAME),                                                                                      \
     (FFI_OWNER_MODE),                                                                                                \
     (FFI_RELEASE_HOOK)}

/** @brief 首方与插件模块的共同 ABI 描述符，注册、类型检查和物化均借用其中的表。
 * @note 插件卸载前必须移除所有借用此描述符的注册记录与原生闭包。
 */
typedef struct ZrLibModuleDescriptor {
    TZrUInt32 abiVersion;
    const TZrChar *moduleName;
    const ZrLibConstantDescriptor *constants;
    TZrSize constantCount;
    const ZrLibFunctionDescriptor *functions;
    TZrSize functionCount;
    const ZrLibTypeDescriptor *types;
    TZrSize typeCount;
    const ZrLibTypeHintDescriptor *typeHints;
    TZrSize typeHintCount;
    const TZrChar *typeHintsJson;
    const TZrChar *documentation;
    const ZrLibModuleLinkDescriptor *moduleLinks;
    TZrSize moduleLinkCount;
    const TZrChar *moduleVersion;
    TZrUInt32 minRuntimeAbi;
    TZrUInt64 requiredCapabilities;
    FZrLibModuleMaterializeCallback onMaterialize;
    EZrLibrary_ProviderPhase providerPhase;
    const ZrLibAttributeRoleDescriptor *attributeRoles;
    TZrSize attributeRoleCount;
    const TZrChar *publicContractHash;
    EZrProviderContractRole providerContractRole;
    const ZrLibCanonicalTypeRoleDescriptor *canonicalTypeRoles;
    TZrSize canonicalTypeRoleCount;
    TZrBool isContractOnly;
} ZrLibModuleDescriptor;

/** @brief 返回本次回调的显式参数数量，不包含实例接收者。 */
ZR_LIBRARY_API TZrSize ZrLib_CallContext_ArgumentCount(const ZrLibCallContext *context);
/** @brief 借用实例方法的接收者值；静态函数可返回空。 */
ZR_LIBRARY_API SZrTypeValue *ZrLib_CallContext_Self(const ZrLibCallContext *context);
/** @brief 借用指定实参；仅在当前调用及下一次栈增长或安全点前使用。 */
ZR_LIBRARY_API SZrTypeValue *ZrLib_CallContext_Argument(const ZrLibCallContext *context, TZrSize index);
/** @brief 将 out/ref 参数写回原始调用位置；仅对有可写实参位置的参数有效。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_WriteBackArgument(ZrLibCallContext *context,
                                                           TZrSize index,
                                                           const SZrTypeValue *value);
/** @brief 借用内联结构实参的帧字节区间；GC 或栈增长后须重新取得。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_InlineArgumentSpan(const ZrLibCallContext *context,
                                                            TZrSize index,
                                                            ZrLibInlineSpan *outSpan);
/** @brief 取得内联实参及其可验证布局，供零复制消费者检查尺寸与对齐。
 * @note 元数据和帧存储均借用；栈增长或安全点后须重新取得。
 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_InlineArgumentView(
        const ZrLibCallContext *context,
        TZrSize index,
        ZrLibInlineArgumentView *outView);
/** @brief 取得当前方法声明者原型，供原生方法确认成员归属。 */
ZR_LIBRARY_API struct SZrObjectPrototype *ZrLib_CallContext_OwnerPrototype(const ZrLibCallContext *context);
/** @brief 取得构造调用的实际目标原型，支持派生类型继承原生构造器。 */
ZR_LIBRARY_API struct SZrObjectPrototype *ZrLib_CallContext_GetConstructTargetPrototype(const ZrLibCallContext *context);
/** @brief 按描述符允许的参数范围检查调用，范围不符时抛出原生调用异常。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_CheckArity(const ZrLibCallContext *context,
                                                    TZrSize minArgumentCount,
                                                    TZrSize maxArgumentCount);
/** @brief 按 Native API 的整数契约读取实参；缺失或类型不符会抛出调用异常。
 * @note BUG: 当前实现也接受 float/double，却未检查数值范围就转 int64；
 *       超范围输入可触发 C 未定义行为，见 native_binding_dispatch.c 的 ReadInt 分支。
 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadInt(const ZrLibCallContext *context,
                                                 TZrSize index,
                                                 TZrInt64 *outValue);
/** @brief 按 Native API 的浮点契约读取实参。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadFloat(const ZrLibCallContext *context,
                                                   TZrSize index,
                                                   TZrFloat64 *outValue);
/** @brief 按 Native API 的布尔契约读取实参。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadBool(const ZrLibCallContext *context,
                                                  TZrSize index,
                                                  TZrBool *outValue);
/** @brief 借用字符串实参对象；后续分配前应按 GC 根规则保护。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadString(const ZrLibCallContext *context,
                                                    TZrSize index,
                                                    SZrString **outValue);
/** @brief 借用对象实参，不负责验证应用层具体类型。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadObject(const ZrLibCallContext *context,
                                                    TZrSize index,
                                                    SZrObject **outValue);
/** @brief 借用数组实参，供 Native 回调按数组协议继续读取。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadArray(const ZrLibCallContext *context,
                                                   TZrSize index,
                                                   SZrObject **outValue);
/** @brief 借用函数、闭包或 Native 指针实参值；存活范围受当前调用帧约束。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_ReadFunction(const ZrLibCallContext *context,
                                                      TZrSize index,
                                                      SZrTypeValue **outValue);
/** @brief 用当前调用名与实参位置抛出类型错误；不会正常返回。 */
ZR_LIBRARY_API ZR_NO_RETURN void ZrLib_CallContext_RaiseTypeError(const ZrLibCallContext *context,
                                                                  TZrSize index,
                                                                  const TZrChar *expectedType);
/** @brief 用当前调用名与允许范围抛出参数个数错误；不会正常返回。 */
ZR_LIBRARY_API ZR_NO_RETURN void ZrLib_CallContext_RaiseArityError(const ZrLibCallContext *context,
                                                                   TZrSize minArgumentCount,
                                                                   TZrSize maxArgumentCount);

/** @brief 在 VM 栈上建立临时根，保护跨 GC 分配点的 Native 局部值。 */
ZR_LIBRARY_API TZrBool ZrLib_TempValueRoot_Begin(SZrState *state, ZrLibTempValueRoot *root);
/** @brief 从回调上下文建立同样的临时根；结束前不得泄漏 root。 */
ZR_LIBRARY_API TZrBool ZrLib_CallContext_BeginTempValueRoot(const ZrLibCallContext *context,
                                                            ZrLibTempValueRoot *root);
/** @brief 借用当前临时根槽位；只在根 active 时有效。 */
ZR_LIBRARY_API SZrTypeValue *ZrLib_TempValueRoot_Value(ZrLibTempValueRoot *root);
/** @brief 将值放入临时根，使后续 GC 能追踪其对象引用。 */
ZR_LIBRARY_API TZrBool ZrLib_TempValueRoot_SetValue(ZrLibTempValueRoot *root, const SZrTypeValue *value);
/** @brief 将对象按指定 VM 值类型放入临时根。 */
ZR_LIBRARY_API TZrBool ZrLib_TempValueRoot_SetObject(ZrLibTempValueRoot *root,
                                                     SZrObject *object,
                                                     EZrValueType type);
/** @brief 清除根中的对象引用，同时保留根作用域。 */
ZR_LIBRARY_API void ZrLib_TempValueRoot_SetNull(ZrLibTempValueRoot *root);
/** @brief 结束临时根并恢复调用帧锚点；必须与成功的 Begin 成对。 */
ZR_LIBRARY_API void ZrLib_TempValueRoot_End(ZrLibTempValueRoot *root);

/** @brief 为 Native 回调结果构造空值；目标槽位由调用方持有。 */
ZR_LIBRARY_API void ZrLib_Value_SetNull(SZrTypeValue *value);
/** @brief 为回调结果写入布尔 VM 值。 */
ZR_LIBRARY_API void ZrLib_Value_SetBool(SZrState *state, SZrTypeValue *value, TZrBool boolValue);
/** @brief 为回调结果写入有符号整数 VM 值。 */
ZR_LIBRARY_API void ZrLib_Value_SetInt(SZrState *state, SZrTypeValue *value, TZrInt64 intValue);
/** @brief 为回调结果写入浮点 VM 值。 */
ZR_LIBRARY_API void ZrLib_Value_SetFloat(SZrState *state, SZrTypeValue *value, TZrFloat64 floatValue);
/** @brief 从 C 字符串建立 VM 字符串结果；可能触发分配和 GC。 */
ZR_LIBRARY_API void ZrLib_Value_SetString(SZrState *state, SZrTypeValue *value, const TZrChar *stringValue);
/** @brief 将已有 VM 字符串对象写入结果；调用方须保持对象可追踪。 */
ZR_LIBRARY_API void ZrLib_Value_SetStringObject(SZrState *state, SZrTypeValue *value, SZrString *stringObject);
/** @brief 将已有 VM 对象按显式值类型写入结果。 */
ZR_LIBRARY_API void ZrLib_Value_SetObject(SZrState *state, SZrTypeValue *value, SZrObject *object, EZrValueType type);
/** @brief 将外部地址写成 Native 指针值；地址的所有权不随此操作转移。 */
ZR_LIBRARY_API void ZrLib_Value_SetNativePointer(SZrState *state, SZrTypeValue *value, TZrPtr pointerValue);

/** @brief 分配普通对象供原生模块返回；分配后须考虑 GC 根。 */
ZR_LIBRARY_API SZrObject *ZrLib_Object_New(SZrState *state);
/** @brief 分配 VM 数组供原生模块返回。 */
ZR_LIBRARY_API SZrObject *ZrLib_Array_New(SZrState *state);
/** @brief 以 C 字段名设置对象属性；无返回状态，调用者不可据此断言写入成功。 */
ZR_LIBRARY_API void ZrLib_Object_SetFieldCString(SZrState *state,
                                                 SZrObject *object,
                                                 const TZrChar *fieldName,
                                                 const SZrTypeValue *value);
/** @brief 按 C 字段名借用属性值；对象变化或 GC 后须重新读取。 */
ZR_LIBRARY_API const SZrTypeValue *ZrLib_Object_GetFieldCString(SZrState *state,
                                                                SZrObject *object,
                                                                const TZrChar *fieldName);
/** @brief 向 VM 数组写入值；调用者须检查失败并保持元素对象为根。 */
ZR_LIBRARY_API TZrBool ZrLib_Array_PushValue(SZrState *state, SZrObject *array, const SZrTypeValue *value);
/** @brief 返回 VM 数组当前逻辑长度。 */
ZR_LIBRARY_API TZrSize ZrLib_Array_Length(SZrObject *array);
/** @brief 借用数组元素值；后续变更或 GC 后不得复用。 */
ZR_LIBRARY_API const SZrTypeValue *ZrLib_Array_Get(SZrState *state, SZrObject *array, TZrSize index);

/** @brief 从已注册的类型名称创建实例；类型查找可能导致模块导入。 */
ZR_LIBRARY_API SZrObject *ZrLib_Type_NewInstance(SZrState *state, const TZrChar *typeName);
/** @brief 从已解析原型创建实例，供持有原型的高频 Native 路径使用。 */
ZR_LIBRARY_API SZrObject *ZrLib_Type_NewInstanceWithPrototype(SZrState *state, SZrObjectPrototype *prototype);
/** @brief 查找已物化类型原型；返回借用引用。 */
ZR_LIBRARY_API SZrObjectPrototype *ZrLib_Type_FindPrototype(SZrState *state, const TZrChar *typeName);

/** @brief 查询已加载模块；不会在此接口中主动导入。 */
ZR_LIBRARY_API SZrObjectModule *ZrLib_Module_GetLoaded(SZrState *state, const TZrChar *moduleName);
/** @brief 借用已加载模块导出值，供 Native 模块间调用。 */
ZR_LIBRARY_API const SZrTypeValue *ZrLib_Module_GetExport(SZrState *state,
                                                          const TZrChar *moduleName,
                                                          const TZrChar *exportName);
/** @brief 从 Native 闭包借用描述符返回类型名，供 FFI 回调等路径推断结果。 */
ZR_LIBRARY_API const TZrChar *ZrLib_CallableValue_GetNativeBindingReturnTypeName(
        SZrState *state,
        const SZrTypeValue *callableValue);

/** @brief 从 C 边界同步调用 VM 函数或 Native 闭包；参数与结果由调用者管理。 */
ZR_LIBRARY_API TZrBool ZrLib_CallValue(SZrState *state,
                                       const SZrTypeValue *callable,
                                       const SZrTypeValue *receiver,
                                       const SZrTypeValue *arguments,
                                       TZrSize argumentCount,
                                       SZrTypeValue *result);
/** @brief 查找模块导出并同步调用，供运行时模块跨模块复用行为。 */
ZR_LIBRARY_API TZrBool ZrLib_CallModuleExport(SZrState *state,
                                              const TZrChar *moduleName,
                                              const TZrChar *exportName,
                                              const SZrTypeValue *arguments,
                                              TZrSize argumentCount,
                                              SZrTypeValue *result);

#endif // ZR_VM_LIBRARY_NATIVE_BINDING_H
