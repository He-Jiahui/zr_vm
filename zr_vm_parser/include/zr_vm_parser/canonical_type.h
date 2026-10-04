#ifndef ZR_VM_PARSER_CANONICAL_TYPE_H
#define ZR_VM_PARSER_CANONICAL_TYPE_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_common/zr_type_conf.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"

#ifndef ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
#define ZR_VM_PARSER_SEMANTIC_ID_TYPES_DECLARED
/**
 * @brief 语义快照中的类型、符号、重载集合和生命周期区域句柄，按各自身份域使用。
 * @note 零值表示无效身份；Reset 会重新使用编号，旧句柄不能跨上下文或分析轮次解释。
 * 这些标量不拥有对应记录；构造器解析另以保留的符号值表示合成默认初始化。
 */
typedef TZrUInt32 TZrTypeId;
typedef TZrUInt32 TZrSymbolId;
typedef TZrUInt32 TZrOverloadSetId;
typedef TZrUInt32 TZrLifetimeRegionId;
#endif

struct SZrSemanticContext;
struct SZrInferredType;

/**
 * @brief 规范类型图的载荷判别标签，供身份比较、类型投影和显示读取对应 union 成员。
 * @note kind 与 data 必须一致；ERROR/NEVER 不含子类型载荷，其他形状按各自契约引用同一上下文。
 */
enum EZrCanonicalTypeKind {
    ZR_CANONICAL_TYPE_PRIMITIVE = 0,
    ZR_CANONICAL_TYPE_NOMINAL,
    ZR_CANONICAL_TYPE_GENERIC_PARAMETER,
    ZR_CANONICAL_TYPE_GENERIC_INSTANCE,
    ZR_CANONICAL_TYPE_ARRAY,
    ZR_CANONICAL_TYPE_TUPLE,
    ZR_CANONICAL_TYPE_UNION,
    ZR_CANONICAL_TYPE_ERROR,
    ZR_CANONICAL_TYPE_NEVER,
    ZR_CANONICAL_TYPE_REF,
    ZR_CANONICAL_TYPE_OWNER,
    ZR_CANONICAL_TYPE_READONLY_VIEW,
    ZR_CANONICAL_TYPE_NULLABLE,
    ZR_CANONICAL_TYPE_FUNCTION,
};

typedef enum EZrCanonicalTypeKind EZrCanonicalTypeKind;

/**
 * @brief 区分数组存储形式的身份键，供类型归一化和后续消费方解释。
 * @note 此标签不携带长度、布局或分配器；当前 GC 分类对三种数组均保守返回 BARRIERED。
 */
enum EZrCanonicalArrayStorageKind {
    ZR_CANONICAL_ARRAY_STORAGE_MANAGED = 0,
    ZR_CANONICAL_ARRAY_STORAGE_INLINE,
    ZR_CANONICAL_ARRAY_STORAGE_NATIVE,
};

typedef enum EZrCanonicalArrayStorageKind EZrCanonicalArrayStorageKind;

enum EZrCanonicalRefAccess {
    ZR_CANONICAL_REF_WRITABLE = 0,
    ZR_CANONICAL_REF_READONLY,
};

typedef enum EZrCanonicalRefAccess EZrCanonicalRefAccess;

enum EZrCanonicalOwnerKind {
    ZR_CANONICAL_OWNER_UNIQUE = 0,
    ZR_CANONICAL_OWNER_SHARED,
    ZR_CANONICAL_OWNER_WEAK,
    ZR_CANONICAL_OWNER_ATOMIC_SHARED,
};

typedef enum EZrCanonicalOwnerKind EZrCanonicalOwnerKind;

/**
 * @brief 参数的规范传递形式，供语法归一化、签名身份与各消费方的投影使用。
 * @note 函数参数的非 VALUE 形式使用 REF 类型节点；IN、REF_READONLY 要求只读引用，
 * REF、OUT 要求可写引用。具体合法组合由 ValidateParameterContract 校验。
 */
enum EZrCanonicalPassingForm {
    ZR_CANONICAL_PASSING_VALUE = 0,
    ZR_CANONICAL_PASSING_IN,
    ZR_CANONICAL_PASSING_REF,
    ZR_CANONICAL_PASSING_REF_READONLY,
    ZR_CANONICAL_PASSING_OUT,
};

typedef enum EZrCanonicalPassingForm EZrCanonicalPassingForm;

/**
 * @brief 参数契约声明的逃逸上界；它参与身份比较，本身不执行借用或生命周期检查。
 * @note 当前函数参数校验接受 FUNCTION，REF/REF_READONLY 还接受 CALLER；
 * 这两种形式使用 FUNCTION 时表示 scoped。其他枚举值不因此成为合法函数参数组合。
 */
enum EZrCanonicalEscapeUpperBound {
    ZR_CANONICAL_ESCAPE_BLOCK = 0,
    ZR_CANONICAL_ESCAPE_FUNCTION,
    ZR_CANONICAL_ESCAPE_CALLER,
    ZR_CANONICAL_ESCAPE_HEAP_STATIC,
    ZR_CANONICAL_ESCAPE_UNKNOWN,
};

typedef enum EZrCanonicalEscapeUpperBound EZrCanonicalEscapeUpperBound;

/** @brief 参数在调用入口的初始化要求；OUT 为 UNINITIALIZED，其他当前函数参数形式为 INITIALIZED。 */
enum EZrCanonicalEntryInitialization {
    ZR_CANONICAL_ENTRY_INITIALIZED = 0,
    ZR_CANONICAL_ENTRY_UNINITIALIZED,
};

typedef enum EZrCanonicalEntryInitialization EZrCanonicalEntryInitialization;

/**
 * @brief 参数在正常返回时的初始化要求；OUT 为 DEFINITELY_INITIALIZED。
 * @note UNCHANGED 表示没有额外的初始化状态承诺，不表示禁止修改参数值；
 * 函数体的 out 确定赋值检查由编译器的独立数据流路径完成。
 */
enum EZrCanonicalExitInitialization {
    ZR_CANONICAL_EXIT_UNCHANGED = 0,
    ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED,
};

typedef enum EZrCanonicalExitInitialization EZrCanonicalExitInitialization;

/**
 * @brief 实参调用语法所需的标记，独立于传递形式与引用的读写权限。
 * @note 当前函数契约中 REF/REF_READONLY 都要求 REF，OUT 要求 OUT，VALUE/IN 为 NONE。
 */
enum EZrCanonicalCallSiteMarker {
    ZR_CANONICAL_CALL_SITE_NONE = 0,
    ZR_CANONICAL_CALL_SITE_REF,
    ZR_CANONICAL_CALL_SITE_OUT,
};

typedef enum EZrCanonicalCallSiteMarker EZrCanonicalCallSiteMarker;

/**
 * @brief 隐式接收者的访问契约，单独参与函数身份，不占显式参数契约数组的位置。
 * @note NONE 用于无接收者的签名；READONLY/MUTABLE 供成员调用能力检查与借用记录使用。
 * 接收者的运行时参数槽、派发与借用检查由相应调用路径建立。
 */
enum EZrCanonicalReceiverEffect {
    ZR_CANONICAL_RECEIVER_NONE = 0,
    ZR_CANONICAL_RECEIVER_READONLY,
    ZR_CANONICAL_RECEIVER_MUTABLE,
};

typedef enum EZrCanonicalReceiverEffect EZrCanonicalReceiverEffect;

/**
 * @brief 可组合的 callable 身份标志，供校验、显示和二进制签名映射使用。
 * @note InternFunction 只接受下列已知位；当前声明语法的效果推导只产生 ASYNC。
 * 各执行或 ABI 投影路径另行判断支持范围，这些位不构成统一的运行时效果检查。
 */
#define ZR_CANONICAL_CALLABLE_EFFECT_NONE ((TZrUInt32)0U)
#define ZR_CANONICAL_CALLABLE_EFFECT_THROWS ((TZrUInt32)1U << 0U)
#define ZR_CANONICAL_CALLABLE_EFFECT_ASYNC ((TZrUInt32)1U << 1U)
#define ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR ((TZrUInt32)1U << 2U)

/**
 * @brief 定义登记与查询共享的能力位，可组合为调用方需要的能力集合。
 * @note 源声明、导入元数据和 FFI 契约负责提供位值；登记检查已知位，HasCapabilities 要求全部所需位存在。
 * 这些位不是自动推导的证明，也不建立运行时权限或 GC 根；SEND/SYNC 的数值与优化 ownership 摘要约定一致。
 */
#define ZR_CANONICAL_TYPE_CAPABILITY_NONE ((TZrUInt32)0U)
#define ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE ((TZrUInt32)1U << 0U)
#define ZR_CANONICAL_TYPE_CAPABILITY_GC_CLASS ((TZrUInt32)1U << 1U)
#define ZR_CANONICAL_TYPE_CAPABILITY_RESOURCE_CLASS ((TZrUInt32)1U << 2U)
#define ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE ((TZrUInt32)1U << 3U)
#define ZR_CANONICAL_TYPE_CAPABILITY_READONLY_TYPE ((TZrUInt32)1U << 4U)
#define ZR_CANONICAL_TYPE_CAPABILITY_REF_LIKE ((TZrUInt32)1U << 5U)
#define ZR_CANONICAL_TYPE_CAPABILITY_HAS_DROP ((TZrUInt32)1U << 6U)
#define ZR_CANONICAL_TYPE_CAPABILITY_HAS_GC_REFERENCES ((TZrUInt32)1U << 7U)
#define ZR_CANONICAL_TYPE_CAPABILITY_HAS_OWNERSHIP_FIELDS ((TZrUInt32)1U << 8U)
#define ZR_CANONICAL_TYPE_CAPABILITY_BLITTABLE ((TZrUInt32)1U << 9U)
#define ZR_CANONICAL_TYPE_CAPABILITY_SEND ((TZrUInt32)1U << 10U)
#define ZR_CANONICAL_TYPE_CAPABILITY_SYNC ((TZrUInt32)1U << 11U)

/**
 * @brief 无已登记构造器的可构造定义在零实参解析成功时使用的合成初始化身份。
 * @note 此值不对应普通符号记录；显式构造器登记拒绝它，lowering 据此选择默认初始化路径。
 */
#define ZR_CANONICAL_SYNTHESIZED_DEFAULT_CONSTRUCTOR_ID ((TZrSymbolId)UINT32_MAX)

/**
 * @brief 规范类型所需扫描策略的保守分类，供 union payload 合并及定义登记使用。
 * @note 复合类型按 FREE、MAPPED、BARRIERED 的顺序取最强类别；未知定义或开放参数采用 BARRIERED。
 * 本分类不建立运行时 GC 根，也不等同于实际值布局已经验证。
 */
enum EZrCanonicalGcScanKind {
    ZR_CANONICAL_GC_SCAN_FREE = 0,
    ZR_CANONICAL_GC_SCAN_MAPPED,
    ZR_CANONICAL_GC_SCAN_BARRIERED,
};

typedef enum EZrCanonicalGcScanKind EZrCanonicalGcScanKind;

/**
 * @brief 区分泛型类型实参、整数常量和开放常量参数，使身份比较及替换读取对应载荷。
 * @note 定义登记使用 TYPE/CONST_INT 参数类别；CONST_PARAMETER 表示开放实参或名称绑定。
 */
enum EZrCanonicalGenericArgumentKind {
    /** @brief 载荷为当前上下文中的类型 ID，可指向开放类型参数。 */
    ZR_CANONICAL_GENERIC_ARGUMENT_TYPE = 0,
    /** @brief 实参载荷为有符号 64 位整数；定义参数类别也用此值表示整数常量形参。 */
    ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT,
    /** @brief 实参载荷为 owner/ordinal 开放常量参数引用，供后续替换按声明位置取值。 */
    ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER,
};

typedef enum EZrCanonicalGenericArgumentKind EZrCanonicalGenericArgumentKind;

/**
 * @brief 以所属定义域和参数序号保留尚未替换的常量参数，使同名参数不共享身份。
 * @note displayName 是借用的来源信息，hash 和相等比较只使用 owner/ordinal；
 * 序号与所属声明的一致性由调用方维持，此载荷不拥有声明或字符串对象。
 */
typedef struct SZrCanonicalConstParameterReference {
    /** @brief 非零的参数所属域身份键；入表检查不证明该符号有实际声明。 */
    TZrSymbolId ownerSymbolId;
    /** @brief 所属声明参数列表的从零起序号；入表不校验声明范围。 */
    TZrUInt32 ordinal;
    /** @brief 可空的借用来源名称；不参与身份，记录复制后仍须由调用方维持字符串有效性。 */
    SZrString *displayName;
} SZrCanonicalConstParameterReference;

/**
 * @brief 用显式类别承载有序泛型实参，保留类型、已知整数与开放常量引用的身份差异。
 * @note 仅 kind 对应的 data 成员有效；记录复制保留类型句柄及借用名称，不复制其对象图。
 */
typedef struct SZrCanonicalGenericArgument {
    /** @brief 选择当前有效载荷的身份标签，调用方须与所写 data 成员一致。 */
    EZrCanonicalGenericArgumentKind kind;
    union {
        /** @brief TYPE 实参的当前上下文句柄，引用成员图而不取得独立所有权。 */
        TZrTypeId typeId;
        /** @brief CONST_INT 实参的精确有符号整数身份值；此记录不执行声明的常量域检查。 */
        TZrInt64 constIntValue;
        /** @brief CONST_PARAMETER 的开放身份引用；displayName 保持借用且不参与实参相等。 */
        SZrCanonicalConstParameterReference constParameter;
    } data;
} SZrCanonicalGenericArgument;

/**
 * @brief 保存内建值类别的规范类型身份键，供类型查询、格式化和运行时投影共用。
 * @note 载荷由所属 canonical 节点保存；类别可含对象、数组等内建值标签，不限于数值标量。
 */
typedef struct SZrCanonicalPrimitiveType {
    /** @brief 身份比较使用的有效 EZrValueType 值；显示别名不参与此键。 */
    EZrValueType valueType;
} SZrCanonicalPrimitiveType;

/**
 * @brief 保存具名定义的身份与展示信息，使不同模块中的同名定义保持独立。
 * @note token 非零时以模块身份和 token 区分定义；token 为零时以模块身份和名称区分。
 * 字符串指针仅借用，节点不复制或释放字符串；入表后须保持其有效性及内容不变。
 */
typedef struct SZrCanonicalNominalType {
    /** @brief 可空的模块身份键；空指针与空字符串不合并，非空值按字符串内容比较。 */
    SZrString *moduleIdentity;
    /** @brief 非空且字节长度非零的展示名称；token 为零时也参与身份，命中已有节点时保留首次名称。 */
    SZrString *name;
    /** @brief 模块内定义身份；零值选择名称身份路径，本接口不校验元数据表类别。 */
    TZrUInt32 definitionToken;
} SZrCanonicalNominalType;

/**
 * @brief 以所属定义域和序号归一开放类型参数，供类型替换按形参位置查找实参。
 * @note 身份不依赖源拼写；非零 owner 及任意 UInt32 序号可以入表，不表示声明或范围已验证。
 */
typedef struct SZrCanonicalGenericParameterType {
    /** @brief 非零的类型参数所属域身份键，调用方须保持其声明域含义一致。 */
    TZrSymbolId ownerSymbolId;
    /** @brief 所属参数列表的从零起序号，与 owner 一起组成身份。 */
    TZrUInt32 ordinal;
} SZrCanonicalGenericParameterType;

/**
 * @brief 保存定义根和有序实参形成的泛型实例身份，也允许记录尚待替换的开放实例。
 * @note 节点拥有实参列表缓冲区；子类型句柄引用当前上下文，实参附带名称仍仅借用。
 */
typedef struct SZrCanonicalGenericInstanceType {
    /** @brief 实例的定义身份根；存在于池中不表示其定义种类、形参个数或声明域已验证。 */
    TZrTypeId definitionTypeId;
    /** @brief 节点复制并拥有的非空有序实参记录；保留类别、位置和重复项，Reset 时释放缓冲区。 */
    SZrArray arguments; // SZrCanonicalGenericArgument
} SZrCanonicalGenericInstanceType;

/**
 * @brief 描述以元素类型、维数和存储类别区分身份的数组类型，供类型投影与布局决策使用。
 * @note rank 表示维数，运行时数组长度不写入此身份；元素 ID 引用所属上下文类型图。
 */
typedef struct SZrCanonicalArrayType {
    /** @brief 元素类型句柄，可引用开放泛型或复合类型；载荷不复制元素类型图。 */
    TZrTypeId elementTypeId;
    /** @brief 正的数组维数，与嵌套一维数组分别构成身份。 */
    TZrUInt32 rank;
    /** @brief 区分 managed、inline 与 native 存储的身份键，供后续布局及 GC 查询解释。 */
    EZrCanonicalArrayStorageKind storageKind;
} SZrCanonicalArrayType;

/**
 * @brief 保存 tuple 按位置排列的成员类型 ID，使位置和重复成员都参与结构身份。
 * @note 节点拥有列表缓冲区，成员只引用所属上下文的类型图；空列表表示空 tuple。
 */
typedef struct SZrCanonicalTypeList {
    /** @brief 节点复制并拥有的 TZrTypeId 序列；保留输入顺序和重复项，清空类型图时释放缓冲区。 */
    SZrArray elementTypeIds; // TZrTypeId
} SZrCanonicalTypeList;

/**
 * @brief 保存 union 定义身份及按变体索引排列的 payload 类型图，供泛型替换和变体查询共用。
 * @note 节点拥有非空变体列表的缓冲区；定义及 payload ID 均引用所属上下文的类型图。
 */
typedef struct SZrCanonicalUnionType {
    /** @brief union 的身份根句柄；入口仅检查 ID 存在，调用方须满足实际消费者对定义种类及替换形状的约束。 */
    TZrTypeId definitionTypeId;
    /** @brief 节点复制并拥有的有序 payload ID 序列；重复 payload 保留各自变体位置。 */
    SZrArray variantTypeIds; // TZrTypeId
} SZrCanonicalUnionType;

typedef struct SZrCanonicalRefType {
    TZrTypeId pointeeTypeId;
    EZrCanonicalRefAccess access;
} SZrCanonicalRefType;

typedef struct SZrCanonicalOwnerType {
    TZrTypeId targetTypeId;
    EZrCanonicalOwnerKind ownerKind;
} SZrCanonicalOwnerType;

typedef struct SZrCanonicalTargetType {
    TZrTypeId targetTypeId;
} SZrCanonicalTargetType;

/**
 * @brief 可复制的七字段参数契约；函数身份按参数顺序比较全部字段。
 * @note 语法归一化、旧签名适配和二进制读取负责形成输入，InternFunction 再校验
 * 当前函数参数组合。构造器也复用此结构，但其登记与匹配使用各自的检查入口。
 * 参数名、默认值和 AST 不存放在此结构中；类型 ID 属于对应 semantic context。
 */
typedef struct SZrCanonicalParameterContract {
    TZrTypeId typeId;
    EZrCanonicalPassingForm passingForm;
    EZrCanonicalEscapeUpperBound escapeUpperBound;
    EZrCanonicalEntryInitialization entryInitialization;
    EZrCanonicalExitInitialization exitInitialization;
    TZrBool acceptsTemporary;
    EZrCanonicalCallSiteMarker callSiteMarker;
} SZrCanonicalParameterContract;

/**
 * @brief 定义登记保存的构造参数匹配元数据，连接参数名、传递契约和可省略性。
 * @note 登记按值复制记录并拥有列表缓冲区，name 仍借用，调用方须维持名称对象有效。
 * hasDefaultValue 仅许可省略；默认值内容与求值、实际初始化由编译器的成员元数据和 lowering 处理。
 * 构造器校验使用独立入口，不应假定等同于函数参数 ValidateParameterContract 的完整组合检查。
 */
typedef struct SZrCanonicalConstructorParameter {
    SZrString *name;
    SZrCanonicalParameterContract contract;
    TZrBool hasDefaultValue;
} SZrCanonicalConstructorParameter;

/**
 * @brief 构造解析借用的源序实参视图，用于位置或具名映射、类型模式及 ref/out 标记匹配。
 * @note typeId 属于解析上下文，name 可空且仅借用；不保存表达式、临时值或默认值。
 * 实参求值、类型转换、借用与初始化状态检查由实际编译路径负责。
 */
typedef struct SZrCanonicalValueArgument {
    TZrTypeId typeId;
    SZrString *name;
    EZrCanonicalCallSiteMarker callSiteMarker;
} SZrCanonicalValueArgument;

/**
 * @brief 构造绑定的诊断结果，区分可构造性、公开性、唯一匹配和输入形态失败。
 * @note 只有 RESOLVED 时构造器 ID 与实参到参数的索引映射可消费；失败时忽略映射。
 * INVALID_ARGUMENTS 也用于临时候选索引分配失败；可构造性检查先于实参校验。
 */
typedef enum EZrValueConstructorResolution {
    ZR_VALUE_CONSTRUCTOR_RESOLVED = 0,
    ZR_VALUE_CONSTRUCTOR_NOT_CONSTRUCTIBLE,
    ZR_VALUE_CONSTRUCTOR_NO_MATCH,
    ZR_VALUE_CONSTRUCTOR_INACCESSIBLE,
    ZR_VALUE_CONSTRUCTOR_AMBIGUOUS,
    ZR_VALUE_CONSTRUCTOR_INVALID_ARGUMENTS
} EZrValueConstructorResolution;

/**
 * @brief 规范函数节点的身份数据：有序参数契约、返回类型、接收者契约和效果位。
 * @note InternFunction 按值复制参数数组；数组由所属 semantic context 管理，
 * CanonicalType_Reset/Free 释放它。查询取得的节点与数组视图仅供借用，
 * 不应跨类型池增长、重置或释放保留；保存类型 ID 并在需要时重新查询。
 * 该身份不直接证明运行时 ABI、类型闭合性、值布局或 GC 根；消费方另行检查。
 */
typedef struct SZrCanonicalFunctionType {
    SZrArray parameterContracts; // SZrCanonicalParameterContract
    TZrTypeId returnTypeId;
    EZrCanonicalReceiverEffect receiverEffect;
    TZrUInt32 effectFlags;
} SZrCanonicalFunctionType;

/**
 * @brief 把当前声明作用域中的源参数名关联到类型句柄或开放常量身份，供推导类型转换使用。
 * @note 绑定表和名称均由调用方管理；转换仅借用表，常量引用可将名称借用指针保留到实例记录。
 * 当前名称绑定使用 TYPE 或 CONST_PARAMETER，已知整数实参由 GenericArgument 承载。
 */
typedef struct SZrCanonicalGenericBinding {
    /** @brief 借用的参数源名，用于当前绑定表中的内容匹配；不作为 canonical 参数身份键。 */
    SZrString *name;
    /** @brief 区分读取 typeId 的类型绑定与读取 owner/ordinal 的开放常量绑定。 */
    EZrCanonicalGenericArgumentKind kind;
    /** @brief TYPE 绑定返回的当前上下文类型句柄，常量绑定不使用此字段承载整数值。 */
    TZrTypeId typeId;
    /** @brief 开放常量绑定的所属定义域身份，须与声明及替换方采用的域一致。 */
    TZrSymbolId ownerSymbolId;
    /** @brief 开放常量绑定在所属声明中的参数位置，供替换按序取实参。 */
    TZrUInt32 ordinal;
} SZrCanonicalGenericBinding;

/**
 * @brief 当前语义上下文驻留的规范类型节点，供 ID 查询与结构身份索引共享。
 * @note kind 决定 data 的活动成员；structuralHash 只筛选候选，完整载荷比较才决定身份。
 * 节点拥有 genericInstance、tuple、union 与 function 的列表缓冲区，子类型 ID 和名称均为引用。
 * Find 返回的节点地址仅借用，类型池增长、Reset 或 Free 后须重新查询；调用方不得改写已驻留身份。
 */
typedef struct SZrCanonicalTypeNode {
    TZrTypeId id;
    EZrCanonicalTypeKind kind;
    TZrUInt64 structuralHash;
    union {
        SZrCanonicalPrimitiveType primitive;
        SZrCanonicalNominalType nominal;
        SZrCanonicalGenericParameterType genericParameter;
        SZrCanonicalGenericInstanceType genericInstance;
        SZrCanonicalArrayType array;
        SZrCanonicalTypeList typeList;
        SZrCanonicalUnionType unionType;
        SZrCanonicalRefType refType;
        SZrCanonicalOwnerType owner;
        SZrCanonicalTargetType target;
        SZrCanonicalFunctionType function;
    } data;
} SZrCanonicalTypeNode;

/**
 * @brief 将内建值类别归一为当前语义上下文中的类型 ID，供复合类型和语义事实复用。
 * @pre context 为已初始化且未释放的语义上下文。
 * @return 相同上下文中重复请求同一有效类别复用 ID；空上下文或越界类别返回无效 ID。
 * @note 新类型入表可能移动节点数组，之前 Find 返回的节点指针须重新取得；
 * ID 只属于当前上下文，清空或销毁类型图后不可继续使用。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternPrimitive(
        struct SZrSemanticContext *context,
        EZrValueType valueType);

/**
 * @brief 按模块和定义身份归一具名类型，供源类型、泛型定义及制品导入共享类型 ID。
 * @pre context 为已初始化且未释放的语义上下文；非空字符串为有效的运行时字符串，
 * 调用方须维持入表字符串的有效性和内容不变，比较遵循 ZrCore_String_Equal 的契约。
 * @return 空上下文、空名称指针或零字节名称返回无效 ID；已有身份复用首次节点及其名称。
 * @note moduleIdentity 可空；非零 token 忽略名称差异，零 token 将名称纳入身份。
 * 新类型入表可能使旧节点指针失效；返回 ID 在该上下文类型图清空或销毁后不可继续使用。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternNominal(
        struct SZrSemanticContext *context,
        SZrString *moduleIdentity,
        SZrString *name,
        TZrUInt32 definitionToken);

/**
 * @brief 为仅含类型实参的调用方归一泛型实例，将每个输入 ID 包装为 TYPE 后使用 Ex 的身份规则。
 * @pre context 已初始化且未释放；非空输入列表须可读，定义及实参 ID 来自其当前类型图。
 * @return 输入列表为空或含池中不存在的 ID 时返回无效 ID；定义有效性检查遵循 Ex。
 * @note 输入缓冲区只在调用中借用；需要整数常量或开放常量参数时调用 Ex。
 * 返回 ID 属于当前类型图，清空或销毁后不可使用；新入表可能使旧 Find 指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternGenericInstance(
        struct SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const TZrTypeId *argumentTypeIds,
        TZrSize argumentCount);

/**
 * @brief 按定义根和非空有序带类别实参归一实例，保留常量及开放参数供泛型替换使用。
 * @pre context 已初始化且未释放；定义及 TYPE 实参 ID 来自其当前类型图；
 * arguments 在调用中可读且 kind/data 一致，开放常量引用的 owner 非零，借用名称须保持有效。
 * @return 空上下文、未知定义 ID、空列表、非法类别、未知 TYPE ID 或零 owner 常量引用返回无效 ID。
 * @note 入表复制实参记录但不复制子图或字符串，displayName 不参与身份且命中时保留首次记录。
 * 此入口不完整验证定义种类、形参数目/类别适配、实际 owner 声明或 ordinal 范围，也不证明实例闭合；
 * 调用方须满足实际消费者的要求。ID 清空后不可使用，新节点可能使旧 Find 指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternGenericInstanceEx(
        struct SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const SZrCanonicalGenericArgument *arguments,
        TZrSize argumentCount);

/**
 * @brief 按 owner 和 ordinal 归一开放类型参数，使不同声明域中的同名参数保持独立身份。
 * @pre context 已初始化且未释放；调用方为参数选择含义稳定的非零 owner 和对应声明序号。
 * @return 空上下文或零 owner 返回无效 ID；相同 owner/ordinal 在当前类型图复用 ID。
 * @note 此入口不查询 owner 的实际符号声明或验证 ordinal 范围；源名称和后续泛型合法性另由消费方处理。
 * ID 只属于当前类型图，清空或销毁后不可使用；新节点可能使旧 Find 指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternGenericParameter(
        struct SZrSemanticContext *context,
        TZrSymbolId ownerSymbolId,
        TZrUInt32 ordinal);

/**
 * @brief 归一数组形状，按元素类型、维数和存储类别复用当前上下文中的类型 ID。
 * @pre context 已初始化且未释放；elementTypeId 须来自其当前类型图，rank 为正且存储类别有效。
 * @return 空上下文、池中不存在的元素 ID、零维数或越界存储类别返回无效 ID。
 * @note 此入口检查已有成员和形状，开放泛型的闭合、布局及 GC 适用性由后续阶段判断。
 * 新节点入表可能移动节点数组；旧 Find 指针须重新查询，ID 在类型图清空或销毁后不可使用。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternArray(
        struct SZrSemanticContext *context,
        TZrTypeId elementTypeId,
        TZrUInt32 rank,
        EZrCanonicalArrayStorageKind storageKind);

/**
 * @brief 按成员位置归一 tuple 类型，使顺序、成员个数及重复项共同决定结构身份。
 * @pre context 已初始化且未释放；非空列表须可读且所有 ID 来自其当前类型图。
 * @return 空上下文或非空列表含无效成员时返回无效 ID；零成员允许空指针并归一为空 tuple。
 * @note 输入缓冲区仅在调用中借用，入表时复制 ID 序列；调用后可释放输入，成员图仍由 context 管理。
 * 不排序、去重或展平成员；泛型闭合及消费阶段合法性另行验证。
 * 新节点可使旧 Find 指针失效，返回 ID 在类型图清空或销毁后不可使用。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternTuple(
        struct SZrSemanticContext *context,
        const TZrTypeId *elementTypeIds,
        TZrSize elementCount);

/**
 * @brief 按定义身份和有序变体 payload 图归一 union，保留声明索引供泛型投影及变体消费使用。
 * @pre context 已初始化且未释放；定义 ID 和非空 payload 列表中的 ID 须来自其当前类型图。
 * @return 空上下文、池中不存在的定义或成员、空变体列表或非空列表空指针返回无效 ID。
 * @note 输入缓冲区只借用到调用结束，入表时复制 ID；顺序和重复 payload 均参与身份。
 * 此入口只检查定义 ID 存在性，不验证定义种类、声明约束及泛型替换形状；调用方须满足实际消费者的要求。
 * TODO: 确认是否需要统一定义种类与替换形状的检查入口；现有投影对已建 union 可直接返回。
 * 新节点可能移动节点数组；旧 Find 指针须重新查询，ID 在类型图清空或销毁后不可使用。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternUnion(
        struct SZrSemanticContext *context,
        TZrTypeId definitionTypeId,
        const TZrTypeId *variantTypeIds,
        TZrSize variantCount);

/**
 * @brief 在当前语义快照中驻留 Error 单例，为错误类型标记提供可查询的规范身份。
 * @pre 非 NULL context 的 state、类型池及索引已初始化，且仍处于有效生命周期。
 * @return 同一 context 在重置前重复调用复用同一 ID；NULL context 返回 ZR_SEMANTIC_ID_INVALID。
 * @note 节点由 context 管理；Reset/Free 后不得复用旧 ID。首次驻留可能扩容，使 Find 返回的借用指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternError(struct SZrSemanticContext *context);
/**
 * @brief 在当前语义快照中驻留 Never 单例，保留与 Error 不同的类型标记身份。
 * @pre 非 NULL context 的 state、类型池及索引已初始化，且仍处于有效生命周期。
 * @return 同一 context 在重置前重复调用复用同一 ID；NULL context 返回 ZR_SEMANTIC_ID_INVALID。
 * @note 节点由 context 管理；Reset/Free 后不得复用旧 ID。首次驻留可能扩容，使 Find 返回的借用指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternNever(struct SZrSemanticContext *context);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternRef(
        struct SZrSemanticContext *context,
        TZrTypeId pointeeTypeId,
        EZrCanonicalRefAccess access);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternOwner(
        struct SZrSemanticContext *context,
        TZrTypeId targetTypeId,
        EZrCanonicalOwnerKind ownerKind);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternReadonlyView(
        struct SZrSemanticContext *context,
        TZrTypeId targetTypeId);

ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternNullable(
        struct SZrSemanticContext *context,
        TZrTypeId targetTypeId);

/**
 * @brief 校验并驻留函数身份，供语法归一化、签名适配、重绑定与 artifact 读取复用。
 * @param parameterContracts 借用的连续参数契约，顺序就是显式参数顺序；数量为零时可为 NULL。
 * @param returnTypeId 必须能在同一 context 的规范类型池中查询到。
 * @note 每个参数类型也必须属于此池；非 VALUE 参数还须满足引用权限及其他字段组合。
 * receiverEffect 必须在枚举范围内，effectFlags 只能包含已知 callable 位。
 * 同一完整身份返回已有 ID；新身份按值复制输入数组，调用返回后输入数组可释放。
 * 调用方提供正常初始化的 context 与可读数组；这里不检查函数体、实参表达式、
 * ABI 支持、类型闭合性或 GC 根，也不提供底层分配失败的事务回滚保证。
 * @return 逻辑输入校验失败返回 ZR_SEMANTIC_ID_INVALID；成功返回本 context 的类型 ID。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_InternFunction(
        struct SZrSemanticContext *context,
        const SZrCanonicalParameterContract *parameterContracts,
        TZrSize parameterCount,
        TZrTypeId returnTypeId,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags);

/**
 * @brief 按 semantic type ID 查询已驻留的 canonical type 节点。
 * @pre context 持有该类型对应的 canonical graph；无效 ID 或未驻留类型返回 NULL。
 * @note 返回 context 所有的借用指针；后续 Intern 可能扩容 canonicalTypes，Reset/Free 会使其失效。
 */
ZR_PARSER_API const SZrCanonicalTypeNode *ZrParser_CanonicalType_Find(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId);

/**
 * @brief 为函数身份驻留、显示及图查询检查参数契约与其规范类型的一致性。
 * @return 空契约、未知类型或不支持的字段组合返回 false；不修改上下文或输入。
 * @note 检查类型存在、引用权限和传递字段组合；不检查实参表达式、函数体的 out 确定赋值或实际逃逸。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_ValidateParameterContract(
        const struct SZrSemanticContext *context,
        const SZrCanonicalParameterContract *contract);

/**
 * @brief 将当前规范类型图显示到调用方缓冲区，供签名、类型提示及语义展示复用。
 * @pre buffer 可写且 bufferSize 包含结尾零字节；类型图与其借用名称在调用期间保持有效。
 * @return 成功写入以零结尾的文本；缓冲区不足、类型无效或递归超限返回 false，buffer != NULL 且 bufferSize > 0 时清为空字符串。
 * @note 不修改类型图；开放参数显示 owner/ordinal 身份，结果是展示文本，不替代类型 ID。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_Format(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize);

/**
 * @brief 将具名类型拼写驻留为 nominal 身份，并使用已登记定义的投影供调用方查询。
 * @pre context 已初始化且未释放；qualifiedName 是有效的非空运行时字符串，调用方维持来源名称的存活。
 * @return 输入无效或投影失败返回无效 ID；无投影时返回该名的 nominal ID。
 * @note 使用最后一个内部点号划分模块与名称，采用零 token 名称身份；不查询声明是否存在。
 * 调用可能增加类型图；失败不回滚已经驻留的节点，新节点可能使旧 Find 指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromName(
        struct SZrSemanticContext *context,
        SZrString *qualifiedName);

/**
 * @brief 将推导类型的结构形状及限定转换到当前规范类型图，供语义事实和后续 lowering 使用。
 * @pre context 已初始化且未释放；type 的结构数组有效，借用名称由其 owner 保持存活。
 * @return 不支持的形状、无效类型或递归超限返回无效 ID；成功 ID 属于当前上下文。
 * @note 输入仅借用且不修改；此入口不提供声明泛型名称绑定，开放常量参数须使用带绑定版本。
 * 数组长度、数值区间等值约束不进入本层类型身份；失败可留下已驻留子类型，新入表可使旧节点指针失效。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromInferred(
        struct SZrSemanticContext *context,
        const struct SZrInferredType *type);

/**
 * @brief 按当前声明的名称绑定将推导类型转换为规范图，保留类型形参和开放常量身份。
 * @pre 非空绑定表覆盖 genericBindingCount 个可读记录；类型句柄属于 context，名称由调用方保持有效。
 * @return 绑定表形态、类型形状或开放常量解析失败返回无效 ID。
 * @note 输入表仅调用中借用；开放常量实例可保留绑定名称的借用指针。
 * TYPE 名称匹配采用首个同名绑定；此入口不验证绑定所属的真实声明，也不提供失败时类型图回滚。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromInferredWithGenericBindings(
        struct SZrSemanticContext *context,
        const struct SZrInferredType *type,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount);

/**
 * @brief 将旧推导签名与传递模式适配为规范函数身份，供 callable 值与已解析调用使用。
 * @pre context 已初始化；非空数组分别承载 SZrInferredType 和 EZrParameterPassingMode，returnType 可读。
 * @return 参数或返回类型转换失败、契约不合法时返回无效 ID。
 * @note 缺少某参数的 passing mode 时按 VALUE 处理；输入只借用，非 VALUE 模式由适配层构造 REF 类型。
 * 此入口不带声明泛型绑定；语法级 scoped/ref readonly 等细化由调用方继续完成，失败不回滚子类型。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromFunctionSignature(
        struct SZrSemanticContext *context,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        const struct SZrInferredType *returnType,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags);

/**
 * @brief 在声明泛型作用域内适配参数与返回类型，供类型环境发布规范函数身份。
 * @pre 输入数组及绑定表满足对应类型转换契约；参数顺序与传递模式顺序一致，returnType 非空。
 * @return 转换或 InternFunction 校验失败返回无效 ID；成功 ID 引用当前上下文。
 * @note 借用输入并释放内部临时契约数组；缺失传递模式按 VALUE 处理。
 * 它不读取 AST 的 scoped、ref readonly 或额外语法信息，调用方仍须做声明细化；失败可留下已驻留子类型。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_FromFunctionSignatureWithGenericBindings(
        struct SZrSemanticContext *context,
        const SZrArray *parameterTypes,
        const SZrArray *parameterPassingModes,
        const struct SZrInferredType *returnType,
        EZrCanonicalReceiverEffect receiverEffect,
        TZrUInt32 effectFlags,
        const SZrCanonicalGenericBinding *genericBindings,
        TZrSize genericBindingCount);

/**
 * @brief 用解析后类型替换函数模板的参数和返回类型，同时保留源声明的调用契约。
 * @pre 两个 ID 来自当前上下文且指向参数数量相同的 FUNCTION 节点。
 * @return 输入不匹配或最终契约无法驻留时返回无效 ID。
 * @note 保留模板的传递、逃逸、初始化、临时值、标记、接收者和效果字段；双方均为外层 REF 时沿用模板权限。
 * 此入口不证明泛型替换正确或函数调用兼容；结果由类型图管理，失败不回滚已驻留的引用节点。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_RebindFunctionSignature(
        struct SZrSemanticContext *context,
        TZrTypeId templateFunctionTypeId,
        TZrTypeId resolvedFunctionTypeId);

/**
 * @brief 为具名非泛型定义登记能力与扫描类别，使能力查询和构造绑定共享元数据。
 * @pre context 已初始化且未释放；typeId 指向 NOMINAL，能力仅含已知位，gcScanKind 在有效范围。
 * @return 输入或与既有记录的泛型形状、投影冲突时返回 false；相同定义可更新能力与扫描类别。
 * @note 登记表由 context 管理；泛型类别按值复制，不拥有 owner 对应的声明。
 * 逻辑校验失败不改既有记录；内部 Array 分配失败不提供可恢复或事务回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterDefinition(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind);

/**
 * @brief 为仅含类型参数的泛型定义登记 owner、参数数量、能力与扫描类别。
 * @note owner 非零且数量为正；每一形参类别设为 TYPE，需要整数常量形参时使用 Ex。
 * @pre context 已初始化且未释放；typeId 指向 NOMINAL，能力仅含已知位，gcScanKind 在有效范围。
 * @return 输入或与既有记录的泛型形状、投影冲突时返回 false；相同定义可更新能力与扫描类别。
 * @note 登记表由 context 管理；泛型类别按值复制，不拥有 owner 对应的声明。
 * 逻辑校验失败不改既有记录；内部 Array 分配失败不提供可恢复或事务回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterGenericDefinition(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind);

/**
 * @brief 为泛型定义登记有序形参类别，使实例能力和构造解析检查数量与类别。
 * @note 正参数数量要求非零 owner 和可读类别表；类别仅接受 TYPE/CONST_INT，空形状要求 owner 为零。
 * @pre context 已初始化且未释放；typeId 指向 NOMINAL，能力仅含已知位，gcScanKind 在有效范围。
 * @return 输入或与既有记录的泛型形状、投影冲突时返回 false；相同定义可更新能力与扫描类别。
 * @note 登记表由 context 管理；泛型类别按值复制，不拥有 owner 对应的声明。
 * 逻辑校验失败不改既有记录；内部 Array 分配失败不提供可恢复或事务回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterGenericDefinitionEx(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        const EZrCanonicalGenericArgumentKind *parameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind);

/**
 * @brief 将非泛型定义关联到已驻留 union 投影，供名称转换返回可消费的 payload 图。
 * @note 非零 projectionTypeId 必须是定义根等于 typeId 的 UNION；无效 ID 表示本次不提供投影。
 * 已有投影不能换为不同非零 ID；传无效 ID 不会清除既有投影。
 * @pre context 已初始化且未释放；typeId 指向 NOMINAL，能力仅含已知位，gcScanKind 在有效范围。
 * @return 输入或与既有记录的泛型形状、投影冲突时返回 false；相同定义可更新能力与扫描类别。
 * @note 登记表由 context 管理；泛型类别按值复制，不拥有 owner 对应的声明。
 * 逻辑校验失败不改既有记录；内部 Array 分配失败不提供可恢复或事务回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterDefinitionProjection(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId);

/**
 * @brief 同时登记泛型形参形状与 union 投影模板，供名称和实例转换按 owner/ordinal 替换 payload。
 * @note 参数类别规则同 Ex；非零 projectionTypeId 须为以 typeId 为定义根的 UNION。
 * 此登记不证明模板所有开放参数均属该 owner；已有投影只可重复，无效投影 ID 不清除旧值。
 * @pre context 已初始化且未释放；typeId 指向 NOMINAL，能力仅含已知位，gcScanKind 在有效范围。
 * @return 输入或与既有记录的泛型形状、投影冲突时返回 false；相同定义可更新能力与扫描类别。
 * @note 登记表由 context 管理；泛型类别按值复制，不拥有 owner 对应的声明。
 * 逻辑校验失败不改既有记录；内部 Array 分配失败不提供可恢复或事务回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterGenericDefinitionProjection(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId ownerSymbolId,
        const EZrCanonicalGenericArgumentKind *parameterKinds,
        TZrSize genericParameterCount,
        TZrUInt32 capabilityFlags,
        EZrCanonicalGcScanKind gcScanKind,
        TZrTypeId projectionTypeId);

/**
 * @brief 将已登记的 nominal 或泛型实例投影为可供 payload 消费的规范类型图。
 * @return 未知 ID 返回无效 ID；已是 UNION 或无已登记投影时返回原 ID。
 * @note 已登记泛型实例先核对实参数量与类别，再按 owner/ordinal 替换投影中的开放参数；不保证所有开放参数闭合。
 * 调用可能驻留新节点，失败不回滚类型图；旧 Find 指针须重新查询。
 * TODO: 核查递归替换在池增长时保留 node/instantiation 借用指针的稳定性，入口见 canonical_type_substitute_projection。
 */
ZR_PARSER_API TZrTypeId ZrParser_CanonicalType_ResolveProjection(
        struct SZrSemanticContext *context,
        TZrTypeId typeId);

/**
 * @brief 为 union 登记等消费方查询当前类型图的保守 GC 扫描类别。
 * @pre outGcScanKind 指向可写结果，context 的类型图在调用期间有效。
 * @return 类型未知、输出为空或递归超限返回 false；失败保留输出原值。
 * @note nominal/泛型实例使用已登记定义类别，未知定义与开放类型采用 BARRIERED；tuple/union 汇总成员的最强类别。
 * 此查询不建立 GC 根，不验证实际运行时布局或泛型类型闭合。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_GetGcScanKind(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrCanonicalGcScanKind *outGcScanKind);

/**
 * @brief 为仅有位置值参数的调用方登记构造器，将类型列表适配为无名称、无默认值的 VALUE 契约。
 * @pre 目标已登记为 VALUE_CONSTRUCTIBLE；参数 ID 来自当前类型图，非零数量的列表可读。
 * @return 适配分配、输入校验或合同登记失败返回 false；零参数允许空列表。
 * @note 临时适配数组在返回前释放，持久记录由定义登记表管理；完整名称与默认参数使用 Contract 入口。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterConstructor(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId constructorSymbolId,
        const TZrTypeId *parameterTypeIds,
        TZrSize parameterCount,
        TZrBool isPublic);

/**
 * @brief 将构造器的公开性、参数名、传递契约与可省略性登记到类型定义，供值构造统一匹配。
 * @pre 目标已登记为 VALUE_CONSTRUCTIBLE；非空参数表可读，类型 ID 与名称在对应上下文中有效。
 * @return 非法 ID、参数、默认值顺序、重复名称或与同符号已有记录不一致时返回 false；同合同重复登记成功。
 * @note 复制参数记录，名称保持借用；默认值必须组成后缀，内容不存入此表。
 * 本入口使用构造器专用校验，并非函数参数完整组合校验；内部 Array 分配失败没有事务回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_RegisterConstructorContract(
        struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrSymbolId constructorSymbolId,
        const SZrCanonicalConstructorParameter *parameters,
        TZrSize parameterCount,
        TZrBool isPublic);

/**
 * @brief 查询已登记定义是否含全部所需能力，供构造许可、ref-like 限制和 FFI 契约消费使用。
 * @return 定义不可解析、所需集合含未知位或能力不足时返回 false。
 * @note union 先找定义根，泛型实例须匹配已登记的参数数量与类别；空需求也要求有可解析定义。
 * 此查询不推导能力，复合包装类型也不自动继承目标能力。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_HasCapabilities(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrUInt32 requiredCapabilityFlags);

/**
 * @brief 为无名称、无 ref/out 标记的位置实参查询唯一公开构造器。
 * @pre 参数 ID 属于当前类型图；outConstructorSymbolId 可写，零实参允许空输入列表。
 * @return 仅完整 Contract 解析为 RESOLVED 时返回 true；其他状态或适配分配失败返回 false。
 * @note 非空输出在解析前置为无效 ID；零实参无已登记构造器时可返回合成默认初始化身份。
 * 不求值实参或默认值，不执行隐式类型转换；失败不提供细分诊断，需细分时用 Contract 入口。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalType_ResolveValueConstructor(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const TZrTypeId *argumentTypeIds,
        TZrSize argumentCount,
        TZrSymbolId *outConstructorSymbolId);

/**
 * @brief 按源序实参视图选择唯一公开构造器，并为绑定层给出实参到参数的映射。
 * @pre outConstructorSymbolId 可写；正 argumentCount 的输入可读，可选 outParameterIndices 可写至少该数量的元素。
 * @return 返回具体解析状态；只有 RESOLVED 时构造 ID 与映射可消费，未登记或不具可构造能力返回 NOT_CONSTRUCTIBLE。
 * @note 先匹配位置/名称、ref/out 标记与规范类型模式，再检查遗漏参数是否有默认值；不求值、不执行隐式转换。
 * 非空构造 ID 输出先清为无效值；索引初置 UINT32_MAX，但歧义退出前可能已写入早先候选，失败须忽略。
 * 无构造器且零实参时选择合成默认初始化；临时索引申请失败也归 INVALID_ARGUMENTS。
 */
ZR_PARSER_API EZrValueConstructorResolution
ZrParser_CanonicalType_ResolveValueConstructorContract(
        const struct SZrSemanticContext *context,
        TZrTypeId typeId,
        const SZrCanonicalValueArgument *arguments,
        TZrSize argumentCount,
        TZrSymbolId *outConstructorSymbolId,
        TZrUInt32 *outParameterIndices);

/**
 * @brief 在语义上下文复用前清空规范类型池和对应的查找索引。
 * @pre context 是已初始化且仍持有有效 state 的语义上下文。
 * @note 旧规范类型 ID 随分析轮次失效；节点自己的容器被释放，池容量留给下一轮分析。
 */
ZR_PARSER_API void ZrParser_CanonicalType_Reset(struct SZrSemanticContext *context);
/**
 * @brief 在语义上下文销毁时释放规范类型池及查找索引。
 * @pre context 是仍持有有效 state 的语义上下文；调用后不得再使用该池。
 */
ZR_PARSER_API void ZrParser_CanonicalType_Free(struct SZrSemanticContext *context);
/**
 * @brief 初始化 canonical type 结构哈希索引；随 semantic context 一起创建。
 * @pre context 的 state 已有效，canonicalTypes 数组已初始化。
 * BUG: 桶数组构造使用不返回失败状态的 Array_Init/Push，分配失败可能导致 NULL 写入且调用方无从恢复。
 */
ZR_PARSER_API void ZrParser_CanonicalTypeIndex_Init(struct SZrSemanticContext *context);
/**
 * @brief 清空索引并保留桶容量，供 semantic context reset 后重新 interning。
 * @pre canonicalTypes 中的节点已按 canonical type reset 的生命周期失效。
 */
ZR_PARSER_API void ZrParser_CanonicalTypeIndex_Reset(struct SZrSemanticContext *context);
/** @brief 释放索引存储；由 canonical type free 在节点存储生命周期结束时调用。 */
ZR_PARSER_API void ZrParser_CanonicalTypeIndex_Free(struct SZrSemanticContext *context);
/**
 * @brief 把一个刚加入 canonicalTypes 的节点挂入其 structuralHash 桶。
 * @pre 每个 nodeIndex 只插入一次，且节点、桶和链数组均处于同一 context 生命周期。
 * @note 返回 false 表示上下文、索引或节点索引无效；true 不代表 hash 相等的节点结构相等。
 * TODO: 重复插入会形成自环；现有唯一调用路径保证单次插入，但该限制尚未由接口强制。
 * BUG: 数组扩容失败无法通过 void Array_Push 反馈，插入和桶重建不能提供 OOM 回滚保证。
 */
ZR_PARSER_API TZrBool ZrParser_CanonicalTypeIndex_Insert(
        struct SZrSemanticContext *context,
        TZrSize nodeIndex);
/** @brief 返回 structuralHash 桶的候选链头；空桶或索引不可用时返回 ZR_MAX_SIZE。 */
ZR_PARSER_API TZrSize ZrParser_CanonicalTypeIndex_First(
        const struct SZrSemanticContext *context,
        TZrUInt64 structuralHash);
/** @brief 沿 First 返回的候选链继续遍历；链尾以 ZR_MAX_SIZE 表示。 */
ZR_PARSER_API TZrSize ZrParser_CanonicalTypeIndex_Next(
        const struct SZrSemanticContext *context,
        TZrSize nodeIndex);
ZR_PARSER_API void ZrParser_CanonicalTypeDefinition_Init(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeDefinition_Reset(struct SZrSemanticContext *context);
ZR_PARSER_API void ZrParser_CanonicalTypeDefinition_Free(struct SZrSemanticContext *context);

#endif // ZR_VM_PARSER_CANONICAL_TYPE_H
