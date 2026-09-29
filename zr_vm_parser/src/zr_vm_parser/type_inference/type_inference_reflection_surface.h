#ifndef ZR_VM_PARSER_TYPE_INFERENCE_REFLECTION_SURFACE_H
#define ZR_VM_PARSER_TYPE_INFERENCE_REFLECTION_SURFACE_H

#include "zr_vm_core/global.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_library/native_registry.h"

/**
 * @brief 按规范名称查询全局注册表中的 canonical type role。
 * @param global 持有 native registry 的 VM 全局状态。
 * @param canonicalName provider 登记的规范类型名；指针须非空。
 * @return 命中时返回 provider 描述符中的借用指针；注册表未就绪或无匹配项时返回 `ZR_NULL`。
 * @note 返回值由 provider 描述符持有；调用方不得释放，并须在描述符仍有效时使用。
 */
const ZrLibCanonicalTypeRoleDescriptor *
ZrParser_ReflectionCompileSurface_Find(
        SZrGlobalState *global,
        const TZrChar *canonicalName);

/**
 * @brief 按稳定 role 查询全局注册表中的 canonical type descriptor。
 * @param global 持有 native registry 的 VM 全局状态。
 * @param role 要查询的稳定 canonical type role。
 * @return 命中时返回 provider 描述符中的借用指针；role 未注册时返回 `ZR_NULL`。
 * @note 返回值由 provider 描述符持有；调用方不得释放，并须在描述符仍有效时使用。
 */
const ZrLibCanonicalTypeRoleDescriptor *
ZrParser_ReflectionCompileSurface_FindByRole(
        SZrGlobalState *global,
        EZrCanonicalTypeRole role);

/**
 * @brief 取得反射类型分类对应的 canonical descriptor 名称。
 * @param global 持有 native registry 的 VM 全局状态。
 * @param category 要映射的 reflection type category。
 * @return erased 分类使用稳定 `REFLECTION_TYPE` role；其他支持分类按 reflection provider 的 projection 查找。
 *         分类不受支持、projection 未注册或匹配不唯一时返回 `ZR_NULL`。
 * @note 成功结果是 provider 描述符持有的借用字符串；调用方不得释放，且不可越过描述符生命周期保存。
 */
const TZrChar *ZrParser_ReflectionCompileSurface_DescriptorName(
        SZrGlobalState *global,
        EZrReflectionTypeCategory category);

#endif
