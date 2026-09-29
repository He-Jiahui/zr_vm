#ifndef ZR_VM_PARSER_COMPILER_ATTRIBUTE_BINDING_H
#define ZR_VM_PARSER_COMPILER_ATTRIBUTE_BINDING_H

#include "compiler_internal.h"

/**
 * @brief 在普通函数与编译期函数进入各自编译阶段前校验内建函数属性。
 * @pre cs 有效，functionNode 是函数声明 AST。
 * @return 角色与 conditional 签名均有效时为 true；诊断失败时为 false。
 */
TZrBool ZrParser_Metadata_ValidateFunctionAttributes(
        SZrCompilerState *cs,
        SZrAstNode *functionNode);
/**
 * @brief 在签名预声明阶段绑定 readonly struct 的 attributeUsage schema。
 * @pre cs 有效；typeNode 是本次编译保留的 struct AST。
 * @note 无 usage 的 struct 不注册；同一 source range 的第二次访问保持幂等。
 *       名称和字段名借用 AST；字段绑定数组由 compiler state 持有并在释放时回收。
 * @return schema 合法或不适用时为 true；重复名、形状或字段契约错误时为 false。
 */
TZrBool ZrParser_Metadata_RegisterAttributeSchema(
        SZrCompilerState *cs,
        SZrAstNode *typeNode);
/**
 * @brief 把 type decorator 中的用户 schema 属性写入类型原型 metadata。
 * @pre cs/state、typeInfo 有效；decorators 与 location 来自当前声明 AST。
 * @note 每条 metadata 保留 schema id、TypeId、retention、源码行和强类型字段值，
 *       之后经运行时反射暴露。
 * @return 所有匹配属性均通过 target、参数和常量类型校验时为 true。
 */
TZrBool ZrParser_Metadata_ApplyTypeAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        SZrTypePrototypeInfo *typeInfo,
        SZrFileRange location);
/**
 * @brief 按显式 target 将用户 schema 属性写入成员 metadata。
 * @pre cs/state、memberInfo 有效；target 与被编译成员种类相符。
 * @note 失败可能发生在此前某个属性已写入对象后；调用方须传播失败并放弃该声明结果。
 * @return 所有匹配属性绑定成功时为 true。
 */
TZrBool ZrParser_Metadata_ApplyMemberAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        EZrParserAttributeTarget target,
        SZrTypeMemberInfo *memberInfo,
        SZrFileRange location);
/**
 * @brief 把函数上的用户 schema 属性保存在函数 metadata，并追加公开 decorator 名称。
 * @pre cs/state、function 有效；decorators 来自该函数 AST。
 * @note metadata 对象随函数常量序列化；名称数组使用原生分配并借用 AST 字符串，
 *       扩展成功后才回收旧数组。
 * @return 属性验证及名称数组扩展均成功时为 true。
 */
TZrBool ZrParser_Metadata_ApplyFunctionAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        SZrFunction *function,
        SZrFileRange location);
/**
 * @brief 把参数上的用户 schema 属性保存在参数 metadata，并追加 decorator 名称。
 * @pre cs/state、parameter 有效；decorators 来自该参数 AST。
 * @note metadata 随参数描述符输出；名称数组使用原生分配并借用 AST 字符串，
 *       扩展成功后才回收旧数组。
 * @return 属性验证及名称数组扩展均成功时为 true。
 */
TZrBool ZrParser_Metadata_ApplyParameterAttributes(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        SZrFunctionMetadataParameter *parameter,
        SZrFileRange location);
/**
 * @brief 识别并处理可按项目 feature 省略的直接函数调用语句。
 * @pre cs/state、handled 有效；expression 是表达式语句中的 AST 表达式。
 * @note 仅直接标识符调用适用；feature 关闭时仍先推导表达式类型。
 *       handled 为 true 后调用方必须跳过代码生成。
 * @return 查找、签名、feature 与类型检查均未报错时为 true。
 */
TZrBool ZrParser_Metadata_TryElideConditionalCall(
        SZrCompilerState *cs,
        SZrAstNode *expression,
        TZrBool *handled);
/**
 * @brief 判断 decorator 是否解析为已注册的内建角色或用户 attribute schema。
 * @pre cs/state 与 decoratorNode 来自当前编译。
 * @return 找到注册项时为 true；未知或形状不支持时为 false。
 */
TZrBool ZrParser_Metadata_IsRegisteredAttribute(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode);
/**
 * @brief 从带内建角色的 decorator 中取出 role 与 AST 调用节点。
 * @pre cs/state、role、call 有效；decoratorNode 来自当前编译 AST。
 * @note call 是借用 AST 指针；普通用户 schema 不附带内建 role，返回 false。
 * @return 解析结果含内建或 provider role schema 时为 true。
 */
TZrBool ZrParser_Metadata_ParseAttributeRole(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        EZrParserAttributeRole *role,
        SZrFunctionCall **call);
/**
 * @brief 查询函数是否恰好带有指定的无参数内建 role 属性。
 * @pre cs/state、functionNode、hasRole 有效；functionNode 是函数声明 AST。
 * @note 重复 role 或带实参的 role 报编译错误；未出现时成功并写 false。
 * @return 属性扫描未发生结构或重复错误时为 true。
 */
TZrBool ZrParser_Metadata_FunctionHasRole(
        SZrCompilerState *cs,
        SZrAstNode *functionNode,
        EZrParserAttributeRole role,
        TZrBool *hasRole);

#endif // ZR_VM_PARSER_COMPILER_ATTRIBUTE_BINDING_H
