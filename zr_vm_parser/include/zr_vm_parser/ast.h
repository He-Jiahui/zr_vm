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

    // 字面量类型
    ZR_AST_BOOLEAN_LITERAL,
    ZR_AST_INTEGER_LITERAL,
    ZR_AST_FLOAT_LITERAL,
    ZR_AST_STRING_LITERAL,
    ZR_AST_CHAR_LITERAL,
    ZR_AST_NULL_LITERAL,
    ZR_AST_IDENTIFIER_LITERAL,

    // 复合字面量
    ZR_AST_ARRAY_LITERAL,
    ZR_AST_OBJECT_LITERAL,
    ZR_AST_KEY_VALUE_PAIR,
    ZR_AST_UNPACK_LITERAL,
    ZR_AST_GENERATOR_EXPRESSION,

    // 语句类型
    ZR_AST_BLOCK,
    ZR_AST_EXPRESSION_STATEMENT,
    ZR_AST_RETURN_STATEMENT,
    ZR_AST_BREAK_CONTINUE_STATEMENT,
    ZR_AST_THROW_STATEMENT,
    ZR_AST_OUT_STATEMENT,
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

// 赋值操作符
typedef struct SZrAssignmentOperator {
    const TChar *op; // "=", "+=", "-=", "*=", "/=", "%="
} SZrAssignmentOperator;

// 二元操作符
typedef struct SZrBinaryOperator {
    const TChar *op; // "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "<<", ">>", "|", "^", "&"
} SZrBinaryOperator;

// 一元操作符
typedef struct SZrUnaryOperator {
    const TChar *op; // "!", "~", "+", "-", "$", "new"
} SZrUnaryOperator;

// 标识符
typedef struct SZrIdentifier {
    SZrString *name;
} SZrIdentifier;

// 类型定义
typedef struct SZrType {
    SZrAstNode *name; // Identifier 或 GenericType 或 TupleType
    struct SZrType *subType; // 子类型（可选）
    TInt32 dimensions; // 数组维度
    
    // 数组大小约束
    TZrSize arrayFixedSize;          // 数组固定大小（0表示未固定）
    TZrSize arrayMinSize;            // 数组最小大小
    TZrSize arrayMaxSize;            // 数组最大大小
    TBool hasArraySizeConstraint;    // 是否有数组大小约束
} SZrType;

// 泛型类型
typedef struct SZrGenericType {
    SZrIdentifier *name;
    SZrAstNodeArray *params; // Type 数组
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
} SZrParameter;

// 字面量节点结构
typedef struct SZrBooleanLiteral {
    TBool value;
} SZrBooleanLiteral;

typedef struct SZrIntegerLiteral {
    TInt64 value;
    SZrString *literal; // 原始字符串
} SZrIntegerLiteral;

typedef struct SZrFloatLiteral {
    TDouble value;
    SZrString *literal;
    TBool isSingle; // 是否为单精度
} SZrFloatLiteral;

typedef struct SZrStringLiteral {
    SZrString *value;
    TBool hasError;
    SZrString *literal;
} SZrStringLiteral;

typedef struct SZrCharLiteral {
    TChar value;
    TBool hasError;
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
    const TChar *op; // "&&" 或 "||"
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
    TBool isStatement;
} SZrIfExpression;

typedef struct SZrSwitchExpression {
    SZrAstNode *expr;
    SZrAstNodeArray *cases; // SwitchCase 数组
    SZrAstNode *defaultCase; // SwitchDefault（可选）
    TBool isStatement;
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
    TBool hasNamedArgs;                 // 是否有命名参数
} SZrFunctionCall;

typedef struct SZrMemberExpression {
    SZrAstNode *property;
    TBool computed; // true 表示使用 []，false 表示使用 .
} SZrMemberExpression;

typedef struct SZrPrimaryExpression {
    SZrAstNode *property;
    SZrAstNodeArray *members; // MemberExpression 或 FunctionCall 数组
} SZrPrimaryExpression;

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
    TBool isStatement;
} SZrWhileLoop;

typedef struct SZrForLoop {
    SZrAstNode *init; // VariableDeclaration 或 null
    SZrAstNode *cond; // ExpressionStatement 或 null
    SZrAstNode *step; // Expression（可选）
    SZrAstNode *block;
    TBool isStatement;
} SZrForLoop;

typedef struct SZrForeachLoop {
    SZrAstNode *pattern; // DestructuringPattern, DestructuringArrayPattern, 或 Identifier
    SZrType *typeInfo; // 可选
    SZrAstNode *expr;
    SZrAstNode *block;
    TBool isStatement;
} SZrForeachLoop;

// 声明节点结构
typedef struct SZrModuleDeclaration {
    SZrAstNode *name; // Identifier 或 StringLiteral
} SZrModuleDeclaration;

typedef struct SZrVariableDeclaration {
    SZrAstNode *pattern; // DestructuringPattern, DestructuringArrayPattern, 或 Identifier
    SZrAstNode *value; // Expression
    SZrType *typeInfo; // 可选
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrVariableDeclaration;

typedef struct SZrFunctionDeclaration {
    SZrIdentifier *name;
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

typedef struct SZrStructDeclaration {
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *inherits; // Type 数组
    SZrAstNodeArray *members; // StructField, StructMethod, StructMetaFunction 数组
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrStructDeclaration;

typedef struct SZrStructField {
    EZrAccessModifier access;
    TBool isStatic;
    SZrIdentifier *name;
    SZrType *typeInfo; // 可选
    SZrAstNode *init; // 可选表达式
} SZrStructField;

typedef struct SZrStructMethod {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TBool isStatic;
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
} SZrStructMethod;

typedef struct SZrStructMetaFunction {
    EZrAccessModifier access;
    TBool isStatic;
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
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrEnumDeclaration;

typedef struct SZrEnumMember {
    SZrIdentifier *name;
    SZrAstNode *value; // 可选表达式
} SZrEnumMember;

// 类声明
typedef struct SZrClassDeclaration {
    SZrIdentifier *name;
    SZrGenericDeclaration *generic; // 可选
    SZrAstNodeArray *inherits; // Type 数组
    SZrAstNodeArray *members; // ClassField, ClassMethod, ClassProperty, ClassMetaFunction 数组
    SZrAstNodeArray *decorators; // DecoratorExpression 数组
    EZrAccessModifier accessModifier; // 可见性修饰符，默认 ZR_ACCESS_PRIVATE
} SZrClassDeclaration;

// 类字段
typedef struct SZrClassField {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TBool isStatic;
    SZrIdentifier *name;
    SZrType *typeInfo; // 可选
    SZrAstNode *init; // 可选表达式
} SZrClassField;

// 类方法
typedef struct SZrClassMethod {
    SZrAstNodeArray *decorators;
    EZrAccessModifier access;
    TBool isStatic;
    SZrIdentifier *name;
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
    TBool isStatic;
    SZrAstNode *modifier; // PropertyGet 或 PropertySet 节点
} SZrClassProperty;

// 类元函数
typedef struct SZrClassMetaFunction {
    EZrAccessModifier access;
    TBool isStatic;
    SZrIdentifier *meta; // MetaIdentifier
    SZrAstNodeArray *params; // Parameter 数组
    SZrParameter *args; // 可变参数（可选）
    SZrAstNodeArray *superArgs; // Expression 数组（可选）
    SZrType *returnType; // 可选
    SZrAstNode *body; // Block
} SZrClassMetaFunction;

// 属性 Getter
typedef struct SZrPropertyGet {
    SZrIdentifier *name;
    SZrType *targetType; // 可选
    SZrAstNode *body; // Block
} SZrPropertyGet;

// 属性 Setter
typedef struct SZrPropertySet {
    SZrIdentifier *name;
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
    TBool hasGet;
    TBool hasSet;
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
    TBool isStatement;
} SZrBlock;

typedef struct SZrExpressionStatement {
    SZrAstNode *expr;
} SZrExpressionStatement;

typedef struct SZrReturnStatement {
    SZrAstNode *expr; // 可选表达式
} SZrReturnStatement;

typedef struct SZrBreakContinueStatement {
    TBool isBreak;
    SZrAstNode *expr; // 可选表达式
} SZrBreakContinueStatement;

typedef struct SZrThrowStatement {
    SZrAstNode *expr;
} SZrThrowStatement;

typedef struct SZrOutStatement {
    SZrAstNode *expr;
} SZrOutStatement;

typedef struct SZrTryCatchFinallyStatement {
    SZrAstNode *block;
    SZrAstNodeArray *catchPattern; // Parameter 数组（可选）
    SZrAstNode *catchBlock; // Block（可选）
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

        // 字面量
        SZrBooleanLiteral booleanLiteral;
        SZrIntegerLiteral integerLiteral;
        SZrFloatLiteral floatLiteral;
        SZrStringLiteral stringLiteral;
        SZrCharLiteral charLiteral;

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
        SZrReturnStatement returnStatement;
        SZrBreakContinueStatement breakContinueStatement;
        SZrThrowStatement throwStatement;
        SZrOutStatement outStatement;
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
ZR_PARSER_API SZrAstNodeArray *ZrAstNodeArrayNew(SZrState *state, TZrSize initialCapacity);
ZR_PARSER_API void ZrAstNodeArrayAdd(SZrState *state, SZrAstNodeArray *array, SZrAstNode *node);
ZR_PARSER_API void ZrAstNodeArrayFree(SZrState *state, SZrAstNodeArray *array);

// AST 节点创建辅助函数（将在 parser.c 中实现）
// 这些函数用于创建各种类型的 AST 节点

#endif // ZR_VM_PARSER_AST_H
