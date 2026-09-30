#ifndef ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H
#define ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H

#include "zr_vm_parser/type_inference.h"

/**
 * @brief 按源码名查找已登记的 type-as-value 别名，并复制其推导类型到输出槽。
 * @pre cs 已初始化；result 是已初始化且没有待释放嵌套存储的输出槽；name 非空。
 * @return 找到 binding 并复制类型时为 true；名称无效或未登记时为 false。
 * @note binding 属于 CompilerState；调用方拥有复制后的类型并负责释放，typeName 字符串仍由 GC 管理。
 * @warning BUG: 泛型别名深复制的 nested Array_Init OOM 可达，后续 Array_Push 会断言或向空 head 写入。
 *          入口：compiler.c:760 -> compiler_bindings.c:786/719/740；cast 经 type_inference.c:2887、type_inference_cast.c:38-39 到别名解析。
 *          Copy 在 type_system.c:407 后只检查源 head/length (408-410) 并 Push:414；array.h:36-42,74,82-88；尚无 OOM 注入。
 */
TZrBool type_inference_resolve_type_value_alias(
        SZrCompilerState *cs,
        SZrString *name,
        SZrInferredType *result);

/**
 * @brief 将已推导类型的源码拼写登记到该类型 AST use-site 的语义展示快照。
 * @pre cs 的 semanticContext、推导类型、活跃 typeUse AST 和 alias 均在调用期间有效；
 *      根 use 不带维度、ownership、reference 或 readonly 修饰。
 * @note TypeId 来自 type，范围和拼写来自 typeUse/alias；语义发布会复制 source 与 alias，
 *       失败只省略可选展示事实。
 */
void type_inference_publish_explicit_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        SZrString *alias,
        const SZrType *typeUse);

/**
 * @brief 将泛型类型 use 的名称和参数写成有限长度源码展示串，再发布到当前快照。
 * @pre cs/state、已推导类型和对应泛型 AST 在调用期间有效。
 * @note 超出格式窗口或遇到不支持的 AST 节点时只跳过别名，不改变类型转换结果；
 *       临时字符串由 GC 管理。
 */
void type_inference_publish_generic_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse);

/**
 * @brief 为受支持的 primitive 名称保留源码 spelling，并关联到已推导的 canonical type。
 * @pre cs/state、推导类型和活跃类型 AST 有效；名称必须通过 primitive mapper 的检查。
 * @note 只记录展示事实，不把源码别名改成新的类型身份。
 */
void type_inference_publish_primitive_type_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse);

/**
 * @brief 从 CompilerState 的 typeValueAliases 中取出源码别名，记录到内层类型 use-site。
 * @pre cs 的 typeValueAliases 已初始化；推导类型和活跃标识符类型 AST 在调用期间有效。
 * @note binding/name 只借用当前编译状态；包装修饰仅在局部 range 视图中清除，
 *       输入 AST 不变。
 */
void type_inference_publish_type_value_display_alias(
        SZrCompilerState *cs,
        const SZrInferredType *type,
        const SZrType *typeUse);

#endif // ZR_VM_PARSER_TYPE_INFERENCE_TYPE_DISPLAY_ALIAS_H
