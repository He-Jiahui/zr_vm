//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_AST_H
#define ZR_VM_PARSER_AST_H

#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/location.h"

#include <stddef.h>

// AST 节点类型枚举
// 参考: zr/src/types/keywords.ts
enum EZrAstNodeType {
    // 顶层
    ZR_AST_SCRIPT,
    ZR_AST_MODULE_DECLARATION,

    // 声明类型
    ZR_AST_STRUCT_DECLARATION,
    ZR_AST_CLASS_DECLARATION,
    ZR_AST_INTERFACE_DECLARATION,
    ZR_AST_ENUM_DECLARATION,
    ZR_AST_FUNCTION_DECLARATION,
    ZR_AST_VARIABLE_DECLARATION,
    ZR_AST_TEST_DECLARATION,
    ZR_AST_COMPILE_TIME_DECLARATION,
    ZR_AST_EXTERN_BLOCK,
    ZR_AST_EXTERN_FUNCTION_DECLARATION,
    ZR_AST_EXTERN_DELEGATE_DECLARATION,
    ZR_AST_INTERMEDIATE_STATEMENT,

    // 结构体成员
    ZR_AST_STRUCT_FIELD,
    ZR_AST_STRUCT_METHOD,
    ZR_AST_STRUCT_META_FUNCTION,

    // 类成员
    ZR_AST_CLASS_FIELD,
    ZR_AST_CLASS_METHOD,
    ZR_AST_CLASS_PROPERTY,
    ZR_AST_CLASS_META_FUNCTION,

    // 接口成员
    ZR_AST_INTERFACE_FIELD_DECLARATION,
    ZR_AST_INTERFACE_METHOD_SIGNATURE,
    ZR_AST_INTERFACE_PROPERTY_SIGNATURE,
    ZR_AST_INTERFACE_META_SIGNATURE,

    // 枚举成员
    ZR_AST_ENUM_MEMBER,

    // 表达式类型
    ZR_AST_ASSIGNMENT_EXPRESSION,
    ZR_AST_BINARY_EXPRESSION,
    ZR_AST_LOGICAL_EXPRESSION,
    ZR_AST_CONDITIONAL_EXPRESSION,
    ZR_AST_UNARY_EXPRESSION,
    ZR_AST_TYPE_CAST_EXPRESSION,
    ZR_AST_LAMBDA_EXPRESSION,
    ZR_AST_IF_EXPRESSION,
    ZR_AST_SWITCH_EXPRESSION,
    ZR_AST_FUNCTION_CALL,
    ZR_AST_MEMBER_EXPRESSION,
    ZR_AST_PRIMARY_EXPRESSION,
    ZR_AST_IMPORT_EXPRESSION,
    ZR_AST_TYPE_QUERY_EXPRESSION,
    ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION,
    ZR_AST_CONSTRUCT_EXPRESSION,

    // 字面量类型
    ZR_AST_BOOLEAN_LITERAL,
    ZR_AST_INTEGER_LITERAL,
    ZR_AST_FLOAT_LITERAL,
    ZR_AST_STRING_LITERAL,
    ZR_AST_TEMPLATE_STRING_LITERAL,
    ZR_AST_CHAR_LITERAL,
    ZR_AST_NULL_LITERAL,
    ZR_AST_IDENTIFIER_LITERAL,
    ZR_AST_INTERPOLATED_SEGMENT,

    // 复合字面量
    ZR_AST_ARRAY_LITERAL,
    ZR_AST_OBJECT_LITERAL,
    ZR_AST_KEY_VALUE_PAIR,
    ZR_AST_UNPACK_LITERAL,
    ZR_AST_GENERATOR_EXPRESSION,

    // 语句类型
    ZR_AST_BLOCK,
    ZR_AST_EXPRESSION_STATEMENT,
    ZR_AST_USING_STATEMENT,
    ZR_AST_RETURN_STATEMENT,
    ZR_AST_BREAK_CONTINUE_STATEMENT,
    ZR_AST_THROW_STATEMENT,
    ZR_AST_OUT_STATEMENT,
    ZR_AST_CATCH_CLAUSE,
    ZR_AST_TRY_CATCH_FINALLY_STATEMENT,

    // 循环表达式
    ZR_AST_WHILE_LOOP,
    ZR_AST_FOR_LOOP,
    ZR_AST_FOREACH_LOOP,

    // Switch 相关
    ZR_AST_SWITCH_CASE,
    ZR_AST_SWITCH_DEFAULT,

    // 类型相关
    ZR_AST_TYPE,
    ZR_AST_GENERIC_TYPE,
    ZR_AST_TUPLE_TYPE,
    ZR_AST_GENERIC_DECLARATION,

    // 参数和参数列表
    ZR_AST_PARAMETER,
    ZR_AST_PARAMETER_LIST,

    // 解构
    ZR_AST_DESTRUCTURING_OBJECT,
    ZR_AST_DESTRUCTURING_ARRAY,

    // 装饰器
    ZR_AST_DECORATOR_EXPRESSION,

    // 元函数标识符
    ZR_AST_META_IDENTIFIER,

    // 中间代码相关
    ZR_AST_INTERMEDIATE_DECLARATION,
    ZR_AST_INTERMEDIATE_CONSTANT,
    ZR_AST_INTERMEDIATE_INSTRUCTION,
    ZR_AST_INTERMEDIATE_INSTRUCTION_PARAMETER,

    // 访问修饰符
    ZR_AST_ACCESS_MODIFIER,

    // 属性类型
    ZR_AST_PROPERTY_GET,
    ZR_AST_PROPERTY_SET,
};

typedef enum EZrAstNodeType EZrAstNodeType;

// 前向声明
typedef struct SZrAstNode SZrAstNode;

// AST 节点数组
typedef struct SZrAstNodeArray {
    SZrAstNode **nodes;
    TZrSize count;
    TZrSize capacity;
} SZrAstNodeArray;

// 访问修饰符
enum EZrAccessModifier {
    ZR_ACCESS_PUBLIC,
    ZR_ACCESS_PRIVATE,
    ZR_ACCESS_PROTECTED,
};

typedef enum EZrAccessModifier EZrAccessModifier;

enum EZrOwnershipQualifier {
    ZR_OWNERSHIP_QUALIFIER_NONE = 0,
    ZR_OWNERSHIP_QUALIFIER_UNIQUE,
    ZR_OWNERSHIP_QUALIFIER_SHARED,
    ZR_OWNERSHIP_QUALIFIER_WEAK,
    ZR_OWNERSHIP_QUALIFIER_BORROWED,
};

typedef enum EZrOwnershipQualifier EZrOwnershipQualifier;

enum EZrOwnershipBuiltinKind {
    ZR_OWNERSHIP_BUILTIN_KIND_NONE = 0,
    ZR_OWNERSHIP_BUILTIN_KIND_UNIQUE,
    ZR_OWNERSHIP_BUILTIN_KIND_SHARED,
    ZR_OWNERSHIP_BUILTIN_KIND_WEAK,
    ZR_OWNERSHIP_BUILTIN_KIND_USING,
    ZR_OWNERSHIP_BUILTIN_KIND_UPGRADE,
    ZR_OWNERSHIP_BUILTIN_KIND_RELEASE,
};

typedef enum EZrOwnershipBuiltinKind EZrOwnershipBuiltinKind;

enum EZrGenericParameterKind {
    ZR_GENERIC_PARAMETER_TYPE,
    ZR_GENERIC_PARAMETER_CONST_INT,
};

typedef enum EZrGenericParameterKind EZrGenericParameterKind;

enum EZrGenericVariance {
    ZR_GENERIC_VARIANCE_NONE,
    ZR_GENERIC_VARIANCE_IN,
    ZR_GENERIC_VARIANCE_OUT,
};

typedef enum EZrGenericVariance EZrGenericVariance;

enum EZrParameterPassingMode {
    ZR_PARAMETER_PASSING_MODE_VALUE,
    ZR_PARAMETER_PASSING_MODE_IN,
    ZR_PARAMETER_PASSING_MODE_OUT,
    ZR_PARAMETER_PASSING_MODE_REF,
};

typedef enum EZrParameterPassingMode EZrParameterPassingMode;

// 赋值操作符
typedef struct SZrAssignmentOperator {
    const TZrChar *op; // "=", "+=", "-=", "*=", "/=", "%="
} SZrAssignmentOperator;

// 二元操作符
typedef struct SZrBinaryOperator {
    const TZrChar *op; // "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", "|", "^", "&"
} SZrBinaryOperator;

// 一元操作符
typedef struct SZrUnaryOperator {
    const TZrChar *op; // "!", "~", "+", "-", "$", "new"
} SZrUnaryOperator;

// 标识符
typedef struct SZrIdentifier {
    SZrString *name;
} SZrIdentifier;

// 类型定义
typedef struct SZrType {
    SZrAstNode *name; // Identifier 或 GenericType 或 TupleType
    struct SZrType *subType; // 子类型（可选）
    TZrInt32 dimensions; // 数组维度
    EZrOwnershipQualifier ownershipQualifier; // 特殊所有权限定
    
    // 数组大小约束
    TZrSize arrayFixedSize;          // 数组固定大小（0表示未固定）
    TZrSize arrayMinSize;            // 数组最小大小
    TZrSize arrayMaxSize;            // 数组最大大小
    TZrBool hasArraySizeConstraint;    // 是否有数组大小约束
    SZrAstNode *arraySizeExpression; // 编译期数组长度表达式（可选）
} SZrType;

// 泛型类型
typedef struct SZrGenericType {
    SZrIdentifier *name;
    SZrAstNodeArray *params; // Type / const expression 数组
} SZrGenericType;

// 元组类型
typedef struct SZrTupleType {
    SZrAstNodeArray *elements; // Type 数组
} SZrTupleType;

// 泛型声明
typedef struct SZrGenericDeclaration {
    SZrAstNodeArray *params; // Parameter 数组
} SZrGenericDeclaration;

// 参数
typedef struct SZrParameter {
    SZrIdentifier *name;
    SZrType *typeInfo; // 可选
    SZrAstNode *defaultValue; // 可选表达式
    TZrBool isConst; // 是否为 const 参数
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
    EZrParameterPassingMode passingMode;
    EZrGenericParameterKind genericKind;
    EZrGenericVariance variance;
    SZrAstNodeArray *genericTypeConstraints; // Type 数组
    TZrBool genericRequiresClass;
    TZrBool genericRequiresStruct;
    TZrBool genericRequiresNew;
} SZrParameter;

// 字面量节点结构
typedef struct SZrBooleanLiteral {
    TZrBool value;
} SZrBooleanLiteral;

typedef struct SZrIntegerLiteral {
    TZrInt64 value;
    SZrString *literal; // 原始字符串
} SZrIntegerLiteral;

typedef struct SZrFloatLiteral {
    TZrDouble value;
    SZrString *literal;
    TZrBool isSingle; // 是否为单精度
} SZrFloatLiteral;

typedef struct SZrStringLiteral {
    SZrString *value;
    TZrBool hasError;
    SZrString *literal;
} SZrStringLiteral;

typedef struct SZrInterpolatedSegment {
    SZrAstNode *expression;
} SZrInterpolatedSegment;

typedef struct SZrTemplateStringLiteral {
    SZrAstNodeArray *segments; // StringLiteral / InterpolatedSegment
} SZrTemplateStringLiteral;

typedef struct SZrCharLiteral {
    TZrChar value;
    TZrBool hasError;
    SZrString *literal;
} SZrCharLiteral;

// 表达式节点结构
typedef struct SZrBinaryExpression {
    SZrAstNode *left;
    SZrAstNode *right;
    SZrBinaryOperator op;
} SZrBinaryExpression;

typedef struct SZrUnaryExpression {
    SZrUnaryOperator op;
    SZrAstNode *argument;
} SZrUnaryExpression;

typedef struct SZrTypeCastExpression {
    SZrType *targetType;  // 目标类型
    SZrAstNode *expression;  // 要转换的表达式
} SZrTypeCastExpression;

typedef struct SZrAssignmentExpression {
    SZrAstNode *left;
    SZrAstNode *right;
    SZrAssignmentOperator op;
} SZrAssignmentExpression;

typedef struct SZrConditionalExpression {
    SZrAstNode *test;
    SZrAstNode *consequent;
    SZrAstNode *alternate;
} SZrConditionalExpression;

typedef struct SZrLogicalExpression {
    SZrAstNode *left;
    SZrAstNode *right;
    const TZrChar *op; // "&&" 或 "||"
} SZrLogicalExpression;

typedef struct SZrLambdaExpression {
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrAstNode *block;
} SZrLambdaExpression;

typedef struct SZrIfExpression {
    SZrAstNode *condition;
    SZrAstNode *thenExpr;
    SZrAstNode *elseExpr; // 可选
    TZrBool isStatement;
} SZrIfExpression;

typedef struct SZrSwitchExpression {
    SZrAstNode *expr;
    SZrAstNodeArray *cases; // SwitchCase 数组
    SZrAstNode *defaultCase; // SwitchDefault（可选）
    TZrBool isStatement;
} SZrSwitchExpression;

typedef struct SZrSwitchCase {
    SZrAstNode *value;
    SZrAstNode *block;
} SZrSwitchCase;

typedef struct SZrSwitchDefault {
    SZrAstNode *block;
} SZrSwitchDefault;

// 命名参数信息
typedef struct SZrNamedArgument {
    SZrString *name;                    // 参数名（可选，ZR_NULL表示位置参数）
    SZrAstNode *value;                  // 参数值表达式
} SZrNamedArgument;

typedef struct SZrFunctionCall {
    SZrAstNodeArray *args;              // Expression 数组（现有）
    SZrArray *argNames;                 // 参数名数组（SZrString*），可选，与args对应，ZR_NULL表示位置参数
    TZrBool hasNamedArgs;                 // 是否有命名参数
    SZrAstNodeArray *genericArguments;  // Type / const expression 数组（可选）
} SZrFunctionCall;

typedef struct SZrMemberExpression {
    SZrAstNode *property;
    TZrBool computed; // true 表示使用 []，false 表示使用 .
} SZrMemberExpression;

typedef struct SZrPrimaryExpression {
    SZrAstNode *property;
    SZrAstNodeArray *members; // MemberExpression 或 FunctionCall 数组
} SZrPrimaryExpression;

typedef struct SZrImportExpression {
    SZrAstNode *modulePath; // StringLiteral
} SZrImportExpression;

typedef struct SZrTypeQueryExpression {
    SZrAstNode *operand;
} SZrTypeQueryExpression;

typedef struct SZrPrototypeReferenceExpression {
    SZrAstNode *target;
} SZrPrototypeReferenceExpression;

typedef struct SZrConstructExpression {
    SZrAstNode *target;
    SZrAstNodeArray *args; // Expression 数组，可选
    EZrOwnershipQualifier ownershipQualifier;
    TZrBool isUsing;
    TZrBool isNew;
    EZrOwnershipBuiltinKind builtinKind;
} SZrConstructExpression;

// 字面量表达式
typedef struct SZrArrayLiteral {
    SZrAstNodeArray *elements; // Expression 数组
} SZrArrayLiteral;

typedef struct SZrObjectLiteral {
    SZrAstNodeArray *properties; // KeyValuePair 数组
} SZrObjectLiteral;

typedef struct SZrKeyValuePair {
    SZrAstNode *key; // Identifier, String, 或 Expression（计算键）
    SZrAstNode *value; // Expression
} SZrKeyValuePair;

typedef struct SZrUnpackLiteral {
    SZrAstNode *element;
} SZrUnpackLiteral;

// 生成器表达式（{{}}）
typedef struct SZrGeneratorExpression {
    SZrAstNode *block; // Block，包含 out 语句
} SZrGeneratorExpression;

// 循环表达式
typedef struct SZrWhileLoop {
    SZrAstNode *cond;
    SZrAstNode *block;
    TZrBool isStatement;
} SZrWhileLoop;

typedef struct SZrForLoop {
    SZrAstNode *init; // VariableDeclaration 或 null
    SZrAstNode *cond; // ExpressionStatement 或 null
    SZrAstNode *step; // Expression（可选）
    SZrAstNode *block;
    TZrBool isStatement;
} SZrForLoop;

typedef struct SZrForeachLoop {
    SZrAstNode *pattern; // DestructuringPattern, DestructuringArrayPattern, 或 Identifier
    SZrType *typeInfo; // 可选
    SZrAstNode *expr;
    SZrAstNode *block;
    TZrBool isStatement;
} SZrForeachLoop;

// 声明节点结构
typedef struct SZrModuleDeclaration {
    SZrAstNode *name; // StringLiteral
} SZrModuleDeclaration;

typedef struct SZrVariableDeclaration {
    SZrAstNode *pattern; // DestructuringPattern, DestructuringArrayPattern, 或 Identifier
    SZrAstNode *value; // Expression
    SZrType *typeInfo; // 可选
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
    TZrBool isConst; // 是否为 const 变量
} SZrVariableDeclaration;

typedef struct SZrFunctionDeclaration {
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
} SZrFunctionDeclaration;

typedef struct SZrTestDeclaration {
    SZrIdentifier *name; // 可选，测试名称
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrAstNode *body; // Block
} SZrTestDeclaration;

// 编译期声明类型
enum EZrCompileTimeDeclarationType {
    ZR_COMPILE_TIME_FUNCTION,
    ZR_COMPILE_TIME_VARIABLE,
    ZR_COMPILE_TIME_STATEMENT,
    ZR_COMPILE_TIME_EXPRESSION
};

typedef enum EZrCompileTimeDeclarationType EZrCompileTimeDeclarationType;

typedef struct SZrCompileTimeDeclaration {
    EZrCompileTimeDeclarationType declarationType;  // FUNCTION, VARIABLE, STATEMENT, EXPRESSION
    SZrAstNode *declaration;                       // 对应的声明节点（函数、变量等）
} SZrCompileTimeDeclaration;

typedef struct SZrExternBlock {
    SZrAstNode *libraryName; // StringLiteral
    SZrAstNodeArray *declarations; // ExternFunctionDeclaration / ExternDelegateDeclaration / Struct / Enum
} SZrExternBlock;

typedef struct SZrExternFunctionDeclaration {
    SZrIdentifier *name;
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
} SZrExternFunctionDeclaration;

typedef struct SZrExternDelegateDeclaration {
    SZrIdentifier *name;
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
} SZrExternDelegateDeclaration;

typedef struct SZrStructDeclaration {
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *inherits; // Type 数组
    SZrAstNodeArray *members; // StructField, StructMethod, StructMetaFunction 数组
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrStructDeclaration;

typedef struct SZrStructField {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    TZrBool isUsingManaged; // 是否使用 field-scoped using 生命周期管理
    TZrBool isConst; // 是否为 const 字段
    SZrIdentifier *name;
    SZrType *typeInfo; // 可选
    SZrAstNode *init; // 可选表达式
} SZrStructField;

typedef struct SZrStructMethod {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    EZrOwnershipQualifier receiverQualifier;
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
} SZrStructMethod;

typedef struct SZrStructMetaFunction {
    EZrAccessModifier access;
    TZrBool isStatic;
    SZrIdentifier *meta; // MetaIdentifier
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
} SZrStructMetaFunction;

typedef struct SZrEnumDeclaration {
    SZrIdentifier *name;
    SZrType *baseType; // 可选，继承类型（int, string, float, bool）
    SZrAstNodeArray *members; // EnumMember 数组
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrEnumDeclaration;

typedef struct SZrEnumMember {
    SZrIdentifier *name;
    SZrAstNode *value; // 可选表达式
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
} SZrEnumMember;

// 类声明
typedef struct SZrClassDeclaration {
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *inherits; // Type 数组
    SZrAstNodeArray *members; // ClassField, ClassMethod, ClassProperty, ClassMetaFunction 数组
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
    TZrBool isOwned;
} SZrClassDeclaration;

// 类字段
typedef struct SZrClassField {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    TZrBool isUsingManaged; // 是否使用 field-scoped using 生命周期管理
    TZrBool isConst; // 是否为 const 字段
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrType *typeInfo; // 可选
    SZrAstNode *init; // 可选表达式
} SZrClassField;

// 类方法
typedef struct SZrClassMethod {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    EZrOwnershipQualifier receiverQualifier;
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
} SZrClassMethod;

// 类属性
typedef struct SZrClassProperty {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TZrBool isStatic;
    SZrAstNode *modifier; // PropertyGet 或 PropertySet 节点
} SZrClassProperty;

// 类元函数
typedef struct SZrClassMetaFunction {
    EZrAccessModifier access;
    TZrBool isStatic;
    SZrIdentifier *meta; // MetaIdentifier
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    TZrBool hasSuperCall; // 是否显式声明了 super(...)
    SZrAstNodeArray *superArgs; // Expression 数组（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
} SZrClassMetaFunction;

// 属性 Getter
typedef struct SZrPropertyGet {
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrType *targetType; // 可选
    SZrAstNode *body; // Block
} SZrPropertyGet;

// 属性 Setter
typedef struct SZrPropertySet {
    SZrIdentifier *name;
    SZrFileRange nameLocation;
    SZrIdentifier *param;
    SZrType *targetType; // 可选
    SZrAstNode *body; // Block
} SZrPropertySet;

// 接口声明
typedef struct SZrInterfaceDeclaration {
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *inherits; // Type 数组
    SZrAstNodeArray *members; // InterfaceFieldDeclaration, InterfaceMethodSignature, InterfacePropertySignature,
                              // InterfaceMetaSignature 数组
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrInterfaceDeclaration;

// 接口字段声明
typedef struct SZrInterfaceFieldDeclaration {
    EZrAccessModifier access;
    TZrBool isConst; // 是否为 const 字段
    SZrIdentifier *name;
    SZrType *typeInfo; // 可选
} SZrInterfaceFieldDeclaration;

// 接口方法签名
typedef struct SZrInterfaceMethodSignature {
    EZrAccessModifier access;
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
} SZrInterfaceMethodSignature;

// 接口属性签名
typedef struct SZrInterfacePropertySignature {
    EZrAccessModifier access;
    TZrBool hasGet;
    TZrBool hasSet;
    SZrIdentifier *name;
    SZrType *typeInfo; // 可选
} SZrInterfacePropertySignature;

// 接口元函数签名
typedef struct SZrInterfaceMetaSignature {
    EZrAccessModifier access;
    SZrIdentifier *meta; // MetaIdentifier
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
} SZrInterfaceMetaSignature;

// 语句节点结构
typedef struct SZrBlock {
    SZrAstNodeArray *body; // Statement 数组
    TZrBool isStatement;
} SZrBlock;

typedef struct SZrExpressionStatement {
    SZrAstNode *expr;
} SZrExpressionStatement;

typedef struct SZrUsingStatement {
    SZrAstNode *resource;
    SZrAstNode *body; // Block（可选，仅 using (expr) { ... }）
    TZrBool isBlockScoped;
} SZrUsingStatement;

typedef struct SZrReturnStatement {
    SZrAstNode *expr; // 可选表达式
} SZrReturnStatement;

typedef struct SZrBreakContinueStatement {
    TZrBool isBreak;
    SZrAstNode *expr; // 可选表达式
} SZrBreakContinueStatement;

typedef struct SZrThrowStatement {
    SZrAstNode *expr;
} SZrThrowStatement;

typedef struct SZrOutStatement {
    SZrAstNode *expr;
} SZrOutStatement;

typedef struct SZrCatchClause {
    SZrAstNodeArray *pattern; // Parameter 数组（可选）
    SZrAstNode *block; // Block
} SZrCatchClause;

typedef struct SZrTryCatchFinallyStatement {
    SZrAstNode *block;
    SZrAstNodeArray *catchClauses; // CatchClause 数组（可选）
    SZrAstNode *finallyBlock; // Block（可选）
} SZrTryCatchFinallyStatement;

// Intermediate 语句
typedef struct SZrIntermediateStatement {
    SZrAstNode *declaration; // IntermediateDeclaration
    SZrAstNodeArray *instructions; // IntermediateInstruction 数组
} SZrIntermediateStatement;

// Intermediate 声明
typedef struct SZrIntermediateDeclaration {
    SZrIdentifier *name;
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNodeArray *closures; // Parameter 数组（可选）
    SZrAstNodeArray *constants; // IntermediateConstant 数组（可选）
    SZrAstNodeArray *locals; // Parameter 数组（可选）
} SZrIntermediateDeclaration;

// Intermediate 常量
typedef struct SZrIntermediateConstant {
    SZrIdentifier *name;
    SZrAstNode *value; // Literal
} SZrIntermediateConstant;

// Intermediate 指令
typedef struct SZrIntermediateInstruction {
    SZrIdentifier *name;
    SZrAstNodeArray *values; // IntermediateInstructionParameter 数组
} SZrIntermediateInstruction;

// Intermediate 指令参数
typedef struct SZrIntermediateInstructionParameter {
    SZrString *value; // 标识符名或数字字面量字符串
} SZrIntermediateInstructionParameter;

// 解构
typedef struct SZrDestructuringObject {
    SZrAstNodeArray *keys; // Identifier 数组
} SZrDestructuringObject;

typedef struct SZrDestructuringArray {
    SZrAstNodeArray *keys; // Identifier 数组
} SZrDestructuringArray;

// 装饰器
typedef struct SZrDecoratorExpression {
    SZrAstNode *expr;
} SZrDecoratorExpression;

// 元函数标识符
typedef struct SZrMetaIdentifier {
    SZrIdentifier *name;
} SZrMetaIdentifier;

// Script 节点
typedef struct SZrScript {
    SZrAstNode *moduleName; // ModuleDeclaration（可选）
    SZrAstNodeArray *statements; // TopLevelStatement 数组
} SZrScript;

// AST 节点联合体
// 所有节点都包含 type 和 location
typedef struct SZrAstNode {
    EZrAstNodeType type;
    SZrFileRange location;

    union {
        // 顶层
        SZrScript script;
        SZrModuleDeclaration moduleDeclaration;

        // 声明
        SZrStructDeclaration structDeclaration;
        SZrClassDeclaration classDeclaration;
        SZrInterfaceDeclaration interfaceDeclaration;
        SZrEnumDeclaration enumDeclaration;
        SZrFunctionDeclaration functionDeclaration;
        SZrTestDeclaration testDeclaration;
        SZrCompileTimeDeclaration compileTimeDeclaration;
        SZrExternBlock externBlock;
        SZrExternFunctionDeclaration externFunctionDeclaration;
        SZrExternDelegateDeclaration externDelegateDeclaration;
        SZrVariableDeclaration variableDeclaration;

        // 结构体成员
        SZrStructField structField;
        SZrStructMethod structMethod;
        SZrStructMetaFunction structMetaFunction;

        // 类成员
        SZrClassField classField;
        SZrClassMethod classMethod;
        SZrClassProperty classProperty;
        SZrClassMetaFunction classMetaFunction;

        // 接口成员
        SZrInterfaceFieldDeclaration interfaceFieldDeclaration;
        SZrInterfaceMethodSignature interfaceMethodSignature;
        SZrInterfacePropertySignature interfacePropertySignature;
        SZrInterfaceMetaSignature interfaceMetaSignature;

        // 属性
        SZrPropertyGet propertyGet;
        SZrPropertySet propertySet;

        // 枚举成员
        SZrEnumMember enumMember;

        // 表达式
        SZrBinaryExpression binaryExpression;
        SZrUnaryExpression unaryExpression;
        SZrTypeCastExpression typeCastExpression;
        SZrAssignmentExpression assignmentExpression;
        SZrConditionalExpression conditionalExpression;
        SZrLogicalExpression logicalExpression;
        SZrLambdaExpression lambdaExpression;
        SZrIfExpression ifExpression;
        SZrSwitchExpression switchExpression;
        SZrFunctionCall functionCall;
        SZrMemberExpression memberExpression;
        SZrPrimaryExpression primaryExpression;
        SZrImportExpression importExpression;
        SZrTypeQueryExpression typeQueryExpression;
        SZrPrototypeReferenceExpression prototypeReferenceExpression;
        SZrConstructExpression constructExpression;

        // 字面量
        SZrBooleanLiteral booleanLiteral;
        SZrIntegerLiteral integerLiteral;
        SZrFloatLiteral floatLiteral;
        SZrStringLiteral stringLiteral;
        SZrTemplateStringLiteral templateStringLiteral;
        SZrCharLiteral charLiteral;
        SZrInterpolatedSegment interpolatedSegment;

        // 复合字面量
        SZrArrayLiteral arrayLiteral;
        SZrObjectLiteral objectLiteral;
        SZrKeyValuePair keyValuePair;
        SZrUnpackLiteral unpackLiteral;
        SZrGeneratorExpression generatorExpression;

        // 循环
        SZrWhileLoop whileLoop;
        SZrForLoop forLoop;
        SZrForeachLoop foreachLoop;

        // Switch
        SZrSwitchCase switchCase;
        SZrSwitchDefault switchDefault;

        // 语句
        SZrBlock block;
        SZrExpressionStatement expressionStatement;
        SZrUsingStatement usingStatement;
        SZrReturnStatement returnStatement;
        SZrBreakContinueStatement breakContinueStatement;
        SZrThrowStatement throwStatement;
        SZrOutStatement outStatement;
        SZrCatchClause catchClause;
        SZrTryCatchFinallyStatement tryCatchFinallyStatement;
        SZrIntermediateStatement intermediateStatement;
        SZrIntermediateDeclaration intermediateDeclaration;
        SZrIntermediateConstant intermediateConstant;
        SZrIntermediateInstruction intermediateInstruction;
        SZrIntermediateInstructionParameter intermediateInstructionParameter;

        // 类型
        SZrType type;
        SZrGenericType genericType;
        SZrTupleType tupleType;
        SZrGenericDeclaration genericDeclaration;

        // 参数
        SZrParameter parameter;

        // 解构
        SZrDestructuringObject destructuringObject;
        SZrDestructuringArray destructuringArray;

        // 其他
        SZrIdentifier identifier;
        SZrDecoratorExpression decoratorExpression;
        SZrMetaIdentifier metaIdentifier;
    } data;
} SZrAstNode;

// AST 节点数组操作
ZR_PARSER_API SZrAstNodeArray *ZrParser_AstNodeArray_New(SZrState *state, TZrSize initialCapacity);
ZR_PARSER_API void ZrParser_AstNodeArray_Add(SZrState *state, SZrAstNodeArray *array, SZrAstNode *node);
ZR_PARSER_API void ZrParser_AstNodeArray_Free(SZrState *state, SZrAstNodeArray *array);

// AST 节点创建辅助函数（将在 parser.c 中实现）
// 这些函数用于创建各种类型的 AST 节点

#endif // ZR_VM_PARSER_AST_H
