#include "compiler_decorator_contract.h"

#include "compiler_attribute_binding.h"
#include "compiler_internal.h"

/**
 * @brief 本编译阶段识别为类 FFI wrapper 元数据的内建指令名集合。
 * @note 这只是分类白名单；参数数量、值域和指令间组合由 wrapper binder 统一校验，
 *       不能把命中名称当成已验证的 FFI 契约。TODO: 本表与 wrapper binder 的白名单重复维护；
 *       确认是否收敛为单一来源，避免新增叶名称时两条校验链分叉。
 */
static const TZrChar *const kBuiltinFfiWrapperLeafNames[] = {
        "lowering",
        "viewType",
        "underlying",
        "ownerMode",
        "releaseHook",
};

/**
 * @brief 从 decorator AST 中识别受支持的 `zr.ffi.<leaf>` 路径。
 * @pre decoratorNode 指向仍存活的 decorator AST；outHasCall 可为空。
 * @return 命中时返回本模块静态白名单中的叶名称（调用方不得释放），否则返回 NULL。
 * @note 返回的 outHasCall 只描述语法上是否带调用；不代表调用参数已通过 FFI wrapper 校验。
 */
const TZrChar *ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
        SZrAstNode *decoratorNode,
        TZrBool *outHasCall) {
    if (outHasCall != ZR_NULL) {
        *outHasCall = ZR_FALSE;
    }

    for (TZrSize index = 0;
         index < ZR_ARRAY_COUNT(kBuiltinFfiWrapperLeafNames);
         index++) {
        const TZrChar *leafName = kBuiltinFfiWrapperLeafNames[index];

        /* FFI wrapper 消费者需要调用 AST 才能取参数；此分类器同时保留裸路径供诊断识别。 */
        if (extern_compiler_match_decorator_path(
                    decoratorNode, leafName, ZR_TRUE, ZR_NULL)) {
            if (outHasCall != ZR_NULL) {
                *outHasCall = ZR_TRUE;
            }
            return leafName;
        }
        if (extern_compiler_match_decorator_path(
                    decoratorNode, leafName, ZR_FALSE, ZR_NULL)) {
            return leafName;
        }
    }

    return ZR_NULL;
}

/**
 * @brief 判断 AST 是否指向内建 FFI wrapper decorator 名称。
 * @pre decoratorNode 指向有效期内的 decorator AST。
 * @return 只确认名称与路径，不验证实参或声明上下文；具体合同由 wrapper binder 检查。
 */
TZrBool ZrParser_DecoratorContract_IsBuiltinFfiWrapper(
        SZrAstNode *decoratorNode) {
    return ZrParser_DecoratorContract_BuiltinFfiWrapperLeafName(
                   decoratorNode, ZR_NULL) != ZR_NULL
           ? ZR_TRUE
           : ZR_FALSE;
}

/**
 * @brief 在编译期声明装饰器消费前，拒绝仍需运行时执行的普通 decorator。
 * @pre cs 是已初始化的 compiler state；decorators 属于当前声明且 AST 仍有效。
 * @return true 表示列表可继续交给属性/编译期处理；false 表示分类已有错误或发现不支持的 runtime decorator。
 * @note 注册属性保留为元数据，comptime decorator 交给声明变换；只有调用方显式允许时，
 *       内建 `zr.ffi.*` 才能越过此门。类 wrapper 的参数与组合随后由独立 binder 校验。
 *       普通 runtime decorator 已从语言执行路径移除，应改用保留属性数据或显式 runtime call。
 */
TZrBool ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators,
        TZrBool allowBuiltinFfiWrapper) {
    if (cs == ZR_NULL) {
        return ZR_FALSE;
    }
    if (decorators == ZR_NULL || decorators->count == 0U) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        /* 已登记属性和 comptime transform 走静态消费路径；内建 wrapper 需受调用方策略限制。 */
        if (decoratorNode == ZR_NULL ||
            ZrParser_Metadata_IsRegisteredAttribute(cs, decoratorNode) ||
            ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode) ||
            (allowBuiltinFfiWrapper &&
             ZrParser_DecoratorContract_IsBuiltinFfiWrapper(decoratorNode))) {
            if (cs->hasError) {
                return ZR_FALSE;
            }
            continue;
        }

        /* 未被静态机制接管的 decorator 不能隐式执行并产生 module-init 副作用。 */
        ZrParser_Compiler_Error(
                cs,
                "decorator.runtime_removed: use retained attribute data or an explicit runtime call",
                decoratorNode->location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
