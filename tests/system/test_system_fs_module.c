/* 从模块元数据与 ZR 源码调用两侧验证 zr.system.fs 对象模型及 FileStream handle_id 降低。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"
#include "test_support.h"
#include "zr_vm_ffi_fixture_path.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_lib_system/module.h"
#include "zr_vm_library.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_meta_conf.h"

#ifndef ZR_ARRAY_COUNT
/**
 * @brief 提供真数组的编译期元素数约定。
 *
 * ## Summary
 * 仅在头文件未提供同名宏时作为本翻译单元后备；当前文件未找到实际展开调用。
 *
 * ## Parameters
 * @param[in] value 真数组表达式，不接受裸指针或依赖求值副作用的输入。
 *
 * ## Returns
 * 元素数量表达式，不登记测试或管理存储。
 *
 * ## Constraints and lifecycle
 * TODO: 当前后备未使用，沿头文件宏约定及历史测试版本核查保留意图；不能据仓内未展开排除未来或头文件条件消费者。
 *
 * ## References
 * 本文件的#ifndef ZR_ARRAY_COUNT后备边界；metadata数组遍历当前使用ZrLib_Array_Length。
 */
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/**
 * @brief 提供 system_fs 每例开始前的 Unity 生命周期入口。
 *
 * ## Summary
 * UnityDefaultTestRun 在每次 RUN_TEST 前派发本回调，使本文件拥有独立的生命周期符号。
 *
 * ## Returns
 * 无返回值；当前不建立共享夹具，也不重置文件系统产物。
 *
 * ## Constraints and lifecycle
 * 本例取得的 VM、Function 与路径资源由用例侧管理；不能依赖此空回调补建状态。
 *
 * ## References
 * UnityDefaultTestRun 的 setUp 派发；main 注册八个本地用例。
 */
void setUp(void) {
}
/* BUG: 持有测试 state 后的断言失败会经 Unity longjmp 跳过局部销毁；空 tearDown 不回收 state。 */
/**
 * @brief 提供 system_fs 每例结束后的 Unity 生命周期入口。
 *
 * ## Summary
 * UnityDefaultTestRun 在用例退出或受保护的断言跳转后调用 tearDown；现有泄漏标记指向此所有权缺口。
 *
 * ## Returns
 * 无返回值；当前不回收用例的局部 State、Function 或生成路径。
 *
 * ## Constraints and lifecycle
 * BUG: 成功取得用例资源后的 Unity 硬断言会跳过正常尾部释放；本空回调不兜底，该条件下资源保留至进程退出。
 *
 * ## References
 * UnityDefaultTestRun 的 tearDown 派发；create_test_state/destroy_test_state 是用例侧资源边界。
 */
void tearDown(void) {
}

/**
 * @brief 保留本测试的 panic 回调槽位，不增加测试专属处理。
 *
 * ## Summary
 * create_test_state 将回调交给 ZrTests_State_Create；支持层分发器保留并在 fatal hook 前调用它。
 *
 * ## Parameters
 * @param[in] state 借用的发生 panic 的 VM 状态；当前忽略，不释放或保存。
 *
 * ## Returns
 * 无返回值；正常返回给测试支持分发器，不自行把 panic 变成成功判据。
 *
 * ## Constraints and lifecycle
 * 回调不是 State 的所有者；支持分发器仍可继续调用已安装的 fatal hook。
 *
 * ## References
 * create_test_state；zr_tests_runtime_panic_handler_dispatch。
 */
static void test_panic_handler(SZrState *state) {
    ZR_UNUSED_PARAMETER(state);
}

/**
 * @brief 创建可解析源码并导入 system/container/ffi 模块的本例 VM。
 *
 * ## Summary
 * 八个用例复用相同的创建顺序；新状态不跨用例共享。
 *
 * ## Returns
 * @return NULL 表示支持层未给出 State；非空结果交给本例，最终通过 destroy_test_state 释放其 Global。
 *
 * ## Constraints and lifecycle
 * 注册是后续导入、源码编译及 FFI fixture 的前提；当前没有检查三个库的注册返回值，不能把非空 State 当作注册全部成功。
 *
 * ## References
 * 本文件八个用例的 create_test_state 调用；ZrTests_Runtime_State_Create 提供拥有型 Global 生命周期。
 */
static SZrState *create_test_state(void) {
    SZrState *state = ZrTests_State_Create(test_panic_handler);
    if (state != ZR_NULL) {
        ZrParser_ToGlobalState_Register(state);
        /* TODO: system/container/ffi 的注册返回值未消费；沿各 Register 的失败返回核实如何把部分注册状态交给用例诊断，不能仅凭 state 非空接受准备完成。 */
        ZrVmLibSystem_Register(state->global);
        ZrVmLibContainer_Register(state->global);
        ZrVmLibFfi_Register(state->global);
    }
    return state;
}

/**
 * @brief 结束本例 VM 和其模块、字符串及对象的有效期。
 *
 * ## Summary
 * 普通执行用例先显式 Free Function，再调用本 helper；元数据用例直接结束 VM 生命周期。
 *
 * ## Parameters
 * @param[in] state 本例拥有的 State；NULL 由支持层按无操作处理，非空状态销毁后不得再用。
 *
 * ## Returns
 * 无返回值；支持层销毁关联 Global，所有借用的模块、字段和字符串视图随之失效。
 *
 * ## Constraints and lifecycle
 * 销毁只发生在当前调用路径可达时；本 helper 不为被 longjmp 跳过的尾部提供自动清理。
 *
 * ## References
 * 本文件用例的正常尾部；ZrTests_Runtime_State_Destroy。
 */
static void destroy_test_state(SZrState *state) {
    ZrTests_State_Destroy(state);
}

/**
 * @brief 为元数据检查取得已注册的 system 叶模块。
 *
 * ## Summary
 * 元数据用例分别检查 filesystem 描述和 IOException 的继承描述。
 *
 * ## Parameters
 * @param[in] state 借用已准备的 State；NULL 返回 NULL。
 * @param[in] moduleName 借用以 NUL 结尾的模块名；本例使用固定的 zr.system.fs/exception，NULL 返回 NULL。
 *
 * ## Returns
 * @return 返回 VM 模块对象或 NULL；对象不交给本 helper 的调用者单独 free，使用受 State 生命周期约束。
 *
 * ## Constraints and lifecycle
 * 导入可物化模块；调用方需检查返回值，不能仅从提前登记推断导入成功。
 *
 * ## References
 * test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields；ZrCore_Module_ImportByPath。
 */
static SZrObjectModule *import_module(SZrState *state, const TZrChar *moduleName) {
    SZrString *modulePath;

    if (state == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    modulePath = ZrCore_String_CreateFromNative(state, (TZrNativeString)moduleName);
    if (modulePath == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_ImportByPath(state, modulePath);
}

/**
 * @brief 把本例合成的 ZR 源码交给解析和编译入口，保留诊断但不执行。
 *
 * ## Summary
 * 运行用例再调用执行支持；ordinary-call 负例仅以 NULL 为判据，probe 仅检查编译产物。
 *
 * ## Parameters
 * @param[in] state 借用准备好的 State；必须保持到返回 Function 使用结束。
 * @param[in] source 借用 NUL 结尾的源码，按 strlen 取长，不能包含作为内容的内嵌 NUL。
 * @param[in] sourceNameText 借用 NUL 结尾的诊断源名；三个输入任一为 NULL 都返回 NULL。
 *
 * ## Returns
 * @return 成功的 Function 交给用例在 State 销毁前显式释放；NULL 表示输入、解析或编译未完成，不区分具体原因。
 *
 * ## Constraints and lifecycle
 * 正常编译路径释放 AST，包括编译返回空的路径；未经此正常路径的 panic/非局部退出不由本 helper 保证释放。
 *
 * ## References
 * 本文件七个源码用例的 compile_source；ZrParser_Parse、ZrParser_Compiler_Compile 和 ZrParser_Ast_Free。
 */
static SZrFunction *compile_source(SZrState *state, const TZrChar *source, const TZrChar *sourceNameText) {
    SZrAstNode *ast;
    SZrString *sourceName;
    SZrFunction *compiled;

    if (state == ZR_NULL || source == ZR_NULL || sourceNameText == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrCore_String_CreateFromNative(state, (TZrNativeString)sourceNameText);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    ast = ZrParser_Parse(state, source, strlen(source), sourceName);
    if (ast == ZR_NULL) {
        if (state->hasCurrentException) {
            ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
        }
        return ZR_NULL;
    }

    compiled = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    if (compiled == ZR_NULL && state->hasCurrentException) {
        ZrCore_Exception_PrintUnhandled(state, &state->currentException, stderr);
    }

    return compiled;
}

/**
 * @brief 为元数据名称比较借用字符串字节视图。
 *
 * ## Summary
 * string_equals_cstring 将该视图立即交给 strcmp；不是通用的按 VM 字节长度比较。
 *
 * ## Parameters
 * @param[in] state 借用当前 State；NULL 返回 NULL。
 * @param[in] value 借用元数据值；要求字符串标签及非空对象，否则返回 NULL。
 *
 * ## Returns
 * @return 借用字符串的 NUL 字节视图；调用者不能 free，释放或移动所属字符串后必须重新取得。
 *
 * ## Constraints and lifecycle
 * 视图可以包含内嵌 NUL；本文件只拿它即时比较固定元数据标识，不保存跨 GC 或 State 销毁。
 *
 * ## References
 * string_equals_cstring；ZrCore_String_GetNativeString。
 */
static const TZrChar *string_value_native(SZrState *state, const SZrTypeValue *value) {
    if (state == ZR_NULL || value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

/**
 * @brief 判定元数据标识是否匹配本例约定的 C 字符串。
 *
 * ## Summary
 * 名称数组搜索及元数据字段断言复用同一比较口径，避免把字段缺失当作匹配。
 *
 * ## Parameters
 * @param[in] state 借用当前 State，转交字符串视图 helper。
 * @param[in] value 借用待检查的 VM 字符串值。
 * @param[in] text 借用预期 NUL 文本；NULL 不匹配。
 *
 * ## Returns
 * @return 只有可读字符串视图和预期文本的 strcmp 结果相等时为真；错误标签、空值等为假。
 *
 * ## Constraints and lifecycle
 * 使用 C 字符串语义，内嵌 NUL 后的字节不参与比较；仅适用于本例的固定名称和契约标识。
 *
 * ## References
 * find_named_entry_in_array、string_array_contains 及元数据用例字段断言。
 */
static TZrBool string_equals_cstring(SZrState *state, const SZrTypeValue *value, const TZrChar *text) {
    const TZrChar *nativeText = string_value_native(state, value);
    return nativeText != ZR_NULL && text != ZR_NULL && strcmp(nativeText, text) == 0;
}

/**
 * @brief 按字段名借用元数据对象或原型的可见属性值。
 *
 * ## Summary
 * 元数据用例查公开反射字段，也查 FileStream 原型上的隐藏 FFI 约定。
 *
 * ## Parameters
 * @param[in] state 借用 State；NULL 返回 NULL。
 * @param[in] object 借用保持可达的元数据对象/原型；NULL 返回 NULL。
 * @param[in] fieldName 借用 NUL 结尾的固定字段名；NULL 返回 NULL。
 *
 * ## Returns
 * @return 借用 Object_GetValue 的结果或 NULL；不转移值存储所有权，不能 free 或在对象存储改写后沿用。
 *
 * ## Constraints and lifecycle
 * 字段名创建可能分配；所属对象须保持可达。Object_GetValue 可以继续查原型链，不表示只查自身字段。
 *
 * ## References
 * 元数据用例、find_named_entry_in_array；ZrCore_Object_GetValue。
 */
static const SZrTypeValue *object_field(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

/**
 * @brief 为元数据检查借用模块的公开导出值。
 *
 * ## Summary
 * metadata 用例读取 moduleInfo，get_module_exported_prototype 读取具名类型导出。
 *
 * ## Parameters
 * @param[in] state 借用 State；NULL 返回 NULL。
 * @param[in] module 借用导入的模块；须保持可达，NULL 返回 NULL。
 * @param[in] exportName 借用 NUL 结尾的导出名；NULL 返回 NULL。
 *
 * ## Returns
 * @return 借用模块属性值或 NULL，不交给调用者释放；模块存储变化或 State 销毁后不能沿用。
 *
 * ## Constraints and lifecycle
 * 公开导出与 protected 表不同；当前实现按模块对象属性查询，缺失仍须由调用方断言。
 *
 * ## References
 * 元数据用例及 get_module_exported_prototype；ZrCore_Module_GetPubExport。
 */
static const SZrTypeValue *get_module_export_value(SZrState *state,
                                                   SZrObjectModule *module,
                                                   const TZrChar *exportName) {
    SZrString *exportString;

    if (state == ZR_NULL || module == ZR_NULL || exportName == ZR_NULL) {
        return ZR_NULL;
    }

    exportString = ZrCore_String_CreateFromNative(state, (TZrNativeString)exportName);
    if (exportString == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_Module_GetPubExport(state, module, exportString);
}

/**
 * @brief 把具名公开导出收窄为可检查隐藏字段的类型原型。
 *
 * ## Summary
 * metadata 用例以它核对真正 FileStream 原型的隐藏 FFI 约定，不只检查 moduleInfo 的文本描述。
 *
 * ## Parameters
 * @param[in] state 借用 State，转交公开导出查询。
 * @param[in] module 借用 filesystem 模块。
 * @param[in] typeName 借用 NUL 结尾的类型导出名；本例查 FileStream。
 *
 * ## Returns
 * @return 借用 OBJECT_PROTOTYPE 或 NULL；导出缺失、值标签或内部种类不符都不接受。
 *
 * ## Constraints and lifecycle
 * 返回对象仍属 VM；不能单独 free，不能越过 State 生命周期。
 *
 * ## References
 * test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields。
 */
static SZrObjectPrototype *get_module_exported_prototype(SZrState *state,
                                                         SZrObjectModule *module,
                                                         const TZrChar *typeName) {
    const SZrTypeValue *value = get_module_export_value(state, module, typeName);
    SZrObject *object;

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL || object->internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) {
        return ZR_NULL;
    }

    return (SZrObjectPrototype *)object;
}

/**
 * @brief 从反射条目数组借用第一个名称匹配的对象。
 *
 * ## Summary
 * metadata 用例查 File/Folder/FileStream、两个接口和 IOException 的反射记录。
 *
 * ## Parameters
 * @param[in] state 借用 State；NULL 返回 NULL。
 * @param[in] array 借用真实 VM 数组；调用方先检查 types 值的 ARRAY 标签并取得对象。
 * @param[in] fieldName 借用条目名称字段的 NUL 文本。
 * @param[in] expectedName 借用本例期望的 NUL 名称；任一空输入返回 NULL。
 *
 * ## Returns
 * @return 第一个可读对象条目且字段匹配的借用对象，找不到为 NULL；不要求名称唯一。
 *
 * ## Constraints and lifecycle
 * 条目对象和数组必须保持可达；跳过非对象条目不是对整个 metadata schema 的验证。
 *
 * ## References
 * test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields；object_field 与 string_equals_cstring。
 */
static SZrObject *find_named_entry_in_array(SZrState *state,
                                            SZrObject *array,
                                            const TZrChar *fieldName,
                                            const TZrChar *expectedName) {
    TZrSize index;

    if (state == ZR_NULL || array == ZR_NULL || fieldName == ZR_NULL || expectedName == ZR_NULL) {
        return ZR_NULL;
    }

    for (index = 0; index < ZrLib_Array_Length(array); index++) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(state, array, index);
        SZrObject *entryObject;
        const SZrTypeValue *nameValue;

        if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
            continue;
        }

        entryObject = ZR_CAST_OBJECT(state, entryValue->value.object);
        if (entryObject == ZR_NULL) {
            continue;
        }

        nameValue = object_field(state, entryObject, fieldName);
        if (string_equals_cstring(state, nameValue, expectedName)) {
            return entryObject;
        }
    }

    return ZR_NULL;
}

/**
 * @brief 检查 FileStream 的 implements 元数据包含约定接口名。
 *
 * ## Summary
 * metadata 用例仅核对 implements 列表同时包含 reader 和 writer，运行接口传参由另一用例覆盖。
 *
 * ## Parameters
 * @param[in] state 借用 State。
 * @param[in] arrayValue 借用 ARRAY 元数据值及真实数组对象；空值或不符的值标签为假。
 * @param[in] expectedText 借用预期 NUL 标识；NULL 为假。
 *
 * ## Returns
 * @return 存在即时 C 字符串匹配的条目为真，否则为假；不是唯一性、顺序或实际接口实现验证。
 *
 * ## Constraints and lifecycle
 * 不拥有数组及其条目；读取过程中保持它们可达，不跨 State 销毁保存视图。
 *
 * ## References
 * metadata 的 implements 断言；ZrLib_Array_Get。
 */
static TZrBool string_array_contains(SZrState *state, const SZrTypeValue *arrayValue, const TZrChar *expectedText) {
    SZrObject *array;
    TZrSize index;

    if (state == ZR_NULL || arrayValue == ZR_NULL || expectedText == ZR_NULL || arrayValue->type != ZR_VALUE_TYPE_ARRAY ||
        arrayValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    array = ZR_CAST_OBJECT(state, arrayValue->value.object);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < ZrLib_Array_Length(array); index++) {
        const SZrTypeValue *entryValue = ZrLib_Array_Get(state, array, index);
        if (string_equals_cstring(state, entryValue, expectedText)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/**
 * @brief 把路径中的反斜杠和双引号转成可嵌入本例 ZR 文本的形式。
 *
 * ## Summary
 * 所有合成源码用例在 snprintf 前调用；它不是任意 ZR 字面量的完整编码器。
 *
 * ## Parameters
 * @param[out] destination 调用者的独立可写缓冲；NULL 或零容量不写入，不能与 source 重叠。
 * @param[in] destinationSize 可写总字节数，含最终 NUL 的位置。
 * @param[in] source 借用 NUL 文本；NULL 在有容量时产生空文本。
 *
 * ## Returns
 * 无返回值；在可写缓冲上交付 NUL 结尾的文本，容量不足时可能只有前缀，调用者无法从返回状态得知截断。
 *
 * ## Constraints and lifecycle
 * 只处理这两类字符，不编码换行或其他控制字符；当前路径来源、长度及字符集合须适合双引号字面量。
 *
 * ## References
 * 七个源码用例的 escape_for_zr_string_literal；本例路径和 FFI fixture 模板构造。
 */
/* TODO: 当前转义只覆盖反斜杠/双引号且不报告容量截断；沿生成路径和 FFI fixture 配置核查允许字符、最坏展开长度及可消费的完整写入判据。 */
static void escape_for_zr_string_literal(char *destination, size_t destinationSize, const char *source) {
    size_t writeIndex = 0;
    size_t readIndex = 0;

    if (destination == NULL || destinationSize == 0) {
        return;
    }

    destination[0] = '\0';
    if (source == NULL) {
        return;
    }

    while (source[readIndex] != '\0' && writeIndex + 1 < destinationSize) {
        if (source[readIndex] == '\\' || source[readIndex] == '"') {
            if (writeIndex + 2 >= destinationSize) {
                break;
            }
            destination[writeIndex++] = '\\';
        }
        destination[writeIndex++] = source[readIndex++];
    }

    destination[writeIndex] = '\0';
}

/**
 * @brief 为本例保留位于 system_fs/runtime 生成树下的测试根路径。
 *
 * ## Summary
 * 七个源码用例用独立根隔离文件操作；时间和进程内计数降低重名概率，但不构成跨进程互斥。
 *
 * ## Parameters
 * @param[in] stem 借用本文件固定的短场景名；按 NUL 文本使用。
 * @param[out] buffer 调用者可写路径缓冲；只有成功时可使用结果。
 * @param[in] bufferSize 总容量含最终 NUL；空输入或零容量为失败。
 *
 * ## Returns
 * @return 真表示取得去掉 .tmp 的路径并清除该路径已有条目；假表示准备未完成，不能继续使用结果。
 *
 * ## Constraints and lifecycle
 * 路径支持会建立父目录；本 helper 不建立最终测试根。已有同名根会被递归删除，名称不是操作系统原子预留。
 *
 * ## References
 * 七个源码用例的 make_unique_test_root；ZrTests_Path_GetGeneratedArtifact。
 */
static TZrBool make_unique_test_root(const TZrChar *stem, TZrChar *buffer, TZrSize bufferSize) {
    /* TODO: 时间/clock/进程内计数不承诺跨进程唯一或线程互斥；并行 system_fs 运行可能选到同名根并触发下面的旧条目删除，需核查原子预留或独立进程命名契约。 */
    static unsigned long long uniqueCounter = 0;
    TZrChar baseName[128];
    TZrChar artifactPath[ZR_TESTS_PATH_MAX];
    TZrSize extensionIndex = 0;

    if (stem == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    snprintf(baseName,
             sizeof(baseName),
             "%s_%lld_%llu_%llu",
             stem,
             (long long)time(ZR_NULL),
             (unsigned long long)clock(),
             uniqueCounter++);
    if (!ZrTests_Path_GetGeneratedArtifact("system_fs", "runtime", baseName, ".tmp", artifactPath, sizeof(artifactPath))) {
        return ZR_FALSE;
    }

    extensionIndex = strlen(artifactPath);
    while (extensionIndex > 0 && artifactPath[extensionIndex - 1] != '.') {
        extensionIndex--;
    }
    if (extensionIndex == 0 || extensionIndex > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, artifactPath, extensionIndex - 1);
    buffer[extensionIndex - 1] = '\0';
    if (ZrLibrary_File_Exist(buffer) != ZR_LIBRARY_FILE_NOT_EXIST &&
        !ZrLibrary_File_Delete(buffer, ZR_TRUE)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

/**
 * @brief 核对 filesystem 与 IOException 的反射表面及 FileStream 原型隐藏的 FFI 包装约定。
 *
 * ## Summary
 * main经RUN_TEST注册本例；公开 types 描述和实际 FileStream 原型隐藏字段来自同一 descriptor 的两条物化路径；本例交叉核对，防止只更新其中一面。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 公开 types 条目和原型隐藏字段双向核对 handle_id/i32/owned/close；接口名存在不等于所有方法行为已验证。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的元数据 RUN_TEST；import_module/get_module_export_value/object_field 读取元数据，get_module_exported_prototype 检查实际原型。
 */
static void test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields(void) {
    SZrTestTimer timer = {0};
    SZrState *state;
    SZrObjectModule *fsModule;
    SZrObjectModule *exceptionModule;
    const SZrTypeValue *moduleInfoValue;
    const SZrTypeValue *typesValue;
    SZrObject *moduleInfo;
    SZrObject *typesArray;
    SZrObject *fileEntry;
    SZrObject *folderEntry;
    SZrObject *streamEntry;
    SZrObject *readerEntry;
    SZrObject *writerEntry;
    SZrObject *ioExceptionEntry;
    SZrObjectPrototype *streamPrototype;
    /* BUG: create_test_state 成功后任一断言失败都会跳过末尾 destroy_test_state，泄漏 state。 */
    ZR_TEST_START("zr.system.fs metadata exposes object surface and wrapper fields");
    timer.startTime = clock();

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    fsModule = import_module(state, "zr.system.fs");
    TEST_ASSERT_NOT_NULL(fsModule);

    moduleInfoValue = get_module_export_value(state, fsModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    TEST_ASSERT_NOT_NULL(moduleInfoValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, moduleInfoValue->type);
    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);

    typesValue = object_field(state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, typesValue->type);
    typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
    TEST_ASSERT_NOT_NULL(typesArray);

    fileEntry = find_named_entry_in_array(state, typesArray, "name", "File");
    folderEntry = find_named_entry_in_array(state, typesArray, "name", "Folder");
    streamEntry = find_named_entry_in_array(state, typesArray, "name", "FileStream");
    readerEntry = find_named_entry_in_array(state, typesArray, "name", "IStreamReader");
    writerEntry = find_named_entry_in_array(state, typesArray, "name", "IStreamWriter");
    TEST_ASSERT_NOT_NULL(fileEntry);
    TEST_ASSERT_NOT_NULL(folderEntry);
    TEST_ASSERT_NOT_NULL(streamEntry);
    TEST_ASSERT_NOT_NULL(readerEntry);
    TEST_ASSERT_NOT_NULL(writerEntry);

    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, fileEntry, "extendsTypeName"), "FileSystemEntry"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, folderEntry, "extendsTypeName"), "FileSystemEntry"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiLoweringKind"), "handle_id"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiUnderlyingTypeName"), "i32"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiOwnerMode"), "owned"));
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, streamEntry, "ffiReleaseHook"), "close"));
    TEST_ASSERT_TRUE(string_array_contains(state, object_field(state, streamEntry, "implements"), "IStreamReader"));
    TEST_ASSERT_TRUE(string_array_contains(state, object_field(state, streamEntry, "implements"), "IStreamWriter"));

    streamPrototype = get_module_exported_prototype(state, fsModule, "FileStream");
    TEST_ASSERT_NOT_NULL(streamPrototype);
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiLoweringKind"),
                                           "handle_id"));
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiUnderlyingTypeName"),
                                           "i32"));
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiOwnerMode"),
                                           "owned"));
    TEST_ASSERT_TRUE(string_equals_cstring(state,
                                           object_field(state, &streamPrototype->super, "__zr_ffiReleaseHook"),
                                           "close"));

    exceptionModule = import_module(state, "zr.system.exception");
    TEST_ASSERT_NOT_NULL(exceptionModule);
    moduleInfoValue = get_module_export_value(state, exceptionModule, ZR_NATIVE_MODULE_INFO_EXPORT_NAME);
    TEST_ASSERT_NOT_NULL(moduleInfoValue);
    moduleInfo = ZR_CAST_OBJECT(state, moduleInfoValue->value.object);
    TEST_ASSERT_NOT_NULL(moduleInfo);
    typesValue = object_field(state, moduleInfo, "types");
    TEST_ASSERT_NOT_NULL(typesValue);
    typesArray = ZR_CAST_OBJECT(state, typesValue->value.object);
    TEST_ASSERT_NOT_NULL(typesArray);
    ioExceptionEntry = find_named_entry_in_array(state, typesArray, "name", "IOException");
    TEST_ASSERT_NOT_NULL(ioExceptionEntry);
    TEST_ASSERT_TRUE(string_equals_cstring(state, object_field(state, ioExceptionEntry, "extendsTypeName"), "RuntimeError"));

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs metadata exposes object surface and wrapper fields");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 从真实 ZR 调用检查路径对象、文本/字节、拷贝移动、枚举与目录删除的组合行为。
 *
 * ## Summary
 * main经RUN_TEST注册本例；合成脚本把具体路径/目录行为的失败编码为负整数；编译成功仍须执行并校验结果1，成功脚本才走根删除。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 脚本将具体检查失败编码为负整数，C 侧要求返回1；成功脚本删除根，早期失败可能保留生成条目。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的路径对象 RUN_TEST；make_unique_test_root/compile_source 准备输入，ZrTests_Function_ExecuteExpectInt64 执行并交付整数判据。
 */
static void test_system_fs_source_runtime_supports_path_objects_and_directory_operations(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = import(\"zr.system.fs\");\n"
            "var root = new fs.Folder(\"%s\");\n"
            "var nested = new fs.Folder(\"%s\");\n"
            "var file = new fs.File(\"%s\");\n"
            "var bytesFile = new fs.File(\"%s\");\n"
            "var copyTarget = \"%s\";\n"
            "var movedTarget = \"%s\";\n"
            "var copiedFolderTarget = \"%s\";\n"
            "var movedFolderTarget = \"%s\";\n"
            "root.create(true);\n"
            "nested.create(true);\n"
            "file.create(true);\n"
            "if (!file.exists()) { return -1; }\n"
            "if (file.name != \"example.data.txt\") { return -2; }\n"
            "if (file.extension != \".txt\") { return -3; }\n"
            "if (file.parent == null || file.parent.fullPath != nested.fullPath) { return -4; }\n"
            "if (file.writeText(\"alpha\") != 5) { return -5; }\n"
            "if (file.readText() != \"alpha\") { return -6; }\n"
            "bytesFile.writeBytes([65, 66, 67]);\n"
            "bytesFile.appendBytes([68]);\n"
            "var bytes = bytesFile.readBytes();\n"
            "if (bytes.length != 4 || bytes[0] != 65 || bytes[3] != 68) { return -7; }\n"
            "var copy = file.copyTo(copyTarget, true);\n"
            "if (!copy.exists() || copy.readText() != \"alpha\") { return -8; }\n"
            "var moved = copy.moveTo(movedTarget, true);\n"
            "if (!moved.exists() || new fs.File(copyTarget).exists()) { return -9; }\n"
            "var entries = root.entries();\n"
            "var files = root.files();\n"
            "var folders = root.folders();\n"
            "var globbed = root.glob(\"*.txt\", true);\n"
            "if (entries.length != 3 || files.length != 2 || folders.length != 1 || globbed.length != 2) { return -10; }\n"
            "if (files[0].name != \"bytes.bin\" || files[1].name != \"moved.txt\") { return -11; }\n"
            "if (folders[0].name != \"nested.dir\") { return -12; }\n"
            "var info = file.refresh();\n"
            "if (file.fileInfo == null) { return -131; }\n"
            "if (info == null) { return -130; }\n"
            "if (!info.exists || !info.isFile || info.size != 5) { return -13; }\n"
            "if (file.fileInfo.name != \"example.data.txt\") { return -141; }\n"
            "if (file.fileInfo.extension != \".txt\") { return -142; }\n"
            "if (info.name != \"example.data.txt\" || info.extension != \".txt\") { return -14; }\n"
            "if (info.parentPath != nested.fullPath) { return -15; }\n"
            "var copiedFolder = nested.copyTo(copiedFolderTarget, true);\n"
            "if (!new fs.File(copiedFolderTarget + \"/example.data.txt\").exists()) { return -16; }\n"
            "var movedFolder = copiedFolder.moveTo(movedFolderTarget, true);\n"
            "if (!new fs.File(movedFolderTarget + \"/example.data.txt\").exists()) { return -17; }\n"
            "movedFolder.delete(true);\n"
            "if (movedFolder.exists()) { return -18; }\n"
            "moved.delete();\n"
            "if (moved.exists()) { return -19; }\n"
            "root.delete(true);\n"
            "if (root.exists()) { return -20; }\n"
            "return 1;\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar nestedPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    TZrChar bytesPath[ZR_TESTS_PATH_MAX];
    TZrChar copyPath[ZR_TESTS_PATH_MAX];
    TZrChar movedPath[ZR_TESTS_PATH_MAX];
    TZrChar copiedFolderPath[ZR_TESTS_PATH_MAX];
    TZrChar movedFolderPath[ZR_TESTS_PATH_MAX];
    char source[16384];
    char escapedRoot[ZR_TESTS_PATH_MAX * 2];
    char escapedNested[ZR_TESTS_PATH_MAX * 2];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    char escapedBytes[ZR_TESTS_PATH_MAX * 2];
    char escapedCopy[ZR_TESTS_PATH_MAX * 2];
    char escapedMoved[ZR_TESTS_PATH_MAX * 2];
    char escapedCopiedFolder[ZR_TESTS_PATH_MAX * 2];
    char escapedMovedFolder[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs runtime supports path objects and directory operations");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("path_objects", rootPath, sizeof(rootPath)));
    /* TODO: 后续路径拼接、转义及源码snprintf均未消费完整写入结果；结合1024字节路径上界和八个占位符核查长构建路径/fixture路径是否会截断或改变测试输入。 */
    snprintf(nestedPath, sizeof(nestedPath), "%s/nested.dir", rootPath);
    snprintf(filePath, sizeof(filePath), "%s/example.data.txt", nestedPath);
    snprintf(bytesPath, sizeof(bytesPath), "%s/bytes.bin", rootPath);
    snprintf(copyPath, sizeof(copyPath), "%s/copy.txt", rootPath);
    snprintf(movedPath, sizeof(movedPath), "%s/moved.txt", rootPath);
    snprintf(copiedFolderPath, sizeof(copiedFolderPath), "%s/nested-copy", rootPath);
    snprintf(movedFolderPath, sizeof(movedFolderPath), "%s/nested-moved", rootPath);

    escape_for_zr_string_literal(escapedRoot, sizeof(escapedRoot), rootPath);
    escape_for_zr_string_literal(escapedNested, sizeof(escapedNested), nestedPath);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    escape_for_zr_string_literal(escapedBytes, sizeof(escapedBytes), bytesPath);
    escape_for_zr_string_literal(escapedCopy, sizeof(escapedCopy), copyPath);
    escape_for_zr_string_literal(escapedMoved, sizeof(escapedMoved), movedPath);
    escape_for_zr_string_literal(escapedCopiedFolder, sizeof(escapedCopiedFolder), copiedFolderPath);
    escape_for_zr_string_literal(escapedMovedFolder, sizeof(escapedMovedFolder), movedFolderPath);

    snprintf(source,
             sizeof(source),
             kSourceTemplate,
             escapedRoot,
             escapedNested,
             escapedFile,
             escapedBytes,
             escapedCopy,
             escapedMoved,
             escapedCopiedFolder,
             escapedMovedFolder);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_path_objects_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs runtime supports path objects and directory operations");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 单独调用拷贝结果的 exists 和 readText，避免复合条件掩盖对象成员调用问题。
 *
 * ## Summary
 * main经RUN_TEST注册本例；拷贝后的成员调用拆成两个结果变量再检查，保留 exists 与 readText 的独立调用路径，不依赖逻辑短路。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 脚本要求拷贝存在且文本为alpha，C侧要求返回1；正常路径不删除该场景的生成根。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的拷贝结果 RUN_TEST；compile_source 与 ZrTests_Function_ExecuteExpectInt64 配合检查两个成员的结果。
 */
static void test_system_fs_copy_result_supports_exists_and_read_text_separately(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = import(\"zr.system.fs\");\n"
            "var root = new fs.Folder(\"%s\");\n"
            "var source = new fs.File(\"%s\");\n"
            "var copyTarget = \"%s\";\n"
            "root.create(true);\n"
            "source.create(true);\n"
            "source.writeText(\"alpha\");\n"
            "var copy = source.copyTo(copyTarget, true);\n"
    /* TODO: 本场景的源码和C尾部都未删除生成根；沿system_fs运行产物保留规则核实成功/失败后的磁盘回收责任，不能从销毁State推断文件已删除。 */
            "var copiedExists = copy.exists();\n"
            "var copiedText = copy.readText();\n"
            "if (!copiedExists) { return -1; }\n"
            "if (copiedText != \"alpha\") { return -2; }\n"
            "return 1;\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar sourcePath[ZR_TESTS_PATH_MAX];
    TZrChar copyPath[ZR_TESTS_PATH_MAX];
    char source[8192];
    char escapedRoot[ZR_TESTS_PATH_MAX * 2];
    char escapedSource[ZR_TESTS_PATH_MAX * 2];
    char escapedCopy[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs copy result supports exists and readText separately");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("copy_result_separate", rootPath, sizeof(rootPath)));
    snprintf(sourcePath, sizeof(sourcePath), "%s/source.txt", rootPath);
    snprintf(copyPath, sizeof(copyPath), "%s/copy.txt", rootPath);

    escape_for_zr_string_literal(escapedRoot, sizeof(escapedRoot), rootPath);
    escape_for_zr_string_literal(escapedSource, sizeof(escapedSource), sourcePath);
    escape_for_zr_string_literal(escapedCopy, sizeof(escapedCopy), copyPath);

    snprintf(source, sizeof(source), kSourceTemplate, escapedRoot, escapedSource, escapedCopy);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_copy_result_separate_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs copy result supports exists and readText separately");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 经 ZR 执行检查 FileStream 模式、读写接口、位置长度、独占打开及 native 句柄调用。
 *
 * ## Summary
 * main经RUN_TEST注册本例；真实执行同时覆盖 stream 接口传参、native 句柄实参和 using 读取分支；成功整数覆盖脚本中的检查，但关闭观测有单独 TODO。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 依赖真实 FFI fixture；脚本显式close部分流并走using读取，但本例不直接断言离开using后资源已关闭。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的流模式 RUN_TEST；FFI fixture 依赖及 FileStream descriptor 提供 native 实参约定，compile_source/执行支持驱动脚本。
 */
static void test_system_fs_source_runtime_supports_stream_modes_and_using(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_tell_fd\")# fn tellFd(fd:i32): i32;\n"
            "}\n"
            "var fs = import(\"zr.system.fs\");\n"
            "var exception = import(\"zr.system.exception\");\n"
            "fn takeReader(reader: fs.IStreamReader): string {\n"
            "  return reader.readText(1);\n"
            "}\n"
            "fn takeWriter(writer: fs.IStreamWriter): int {\n"
            "  return writer.writeText(\"!\");\n"
            "}\n"
            "fn readWithUsing(target: fs.File): string {\n"
            "  var usingStream = target.open(\"r\");\n"
            "  using usingStream;\n"
            "  return usingStream.readText(-1);\n"
            "}\n"
            "var file = new fs.File(\"%s\");\n"
            "var exclusiveFile = new fs.File(\"%s\");\n"
            "file.parent.create(true);\n"
            "var stream = file.open(\"w+\");\n"
            "if (stream.closed || stream.length != 0 || stream.position != 0) { return -1; }\n"
            "if (tellFd(stream) != 0) { return -21; }\n"
            "if (stream.mode != \"w+\") { return -2; }\n"
            "if (stream.path != file.fullPath) { return -3; }\n"
            "if (stream.writeText(\"abc\") != 3) { return -4; }\n"
            "if (stream.position != 3 || stream.length != 3) { return -5; }\n"
            "if (tellFd(stream) != 3) { return -22; }\n"
            "if (stream.seek(1, \"begin\") != 1) { return -7; }\n"
            "if (stream.writeText(\"Z\") != 1) { return -9; }\n"
            "if (stream.seek(0, \"begin\") != 0) { return -10; }\n"
            "if (takeReader(stream) != \"a\") { return -11; }\n"
            "if (stream.seek(0, \"begin\") != 0) { return -12; }\n"
            "if (stream.readText(-1) != \"aZc\") { return -13; }\n"
            "stream.setLength(2);\n"
            "if (stream.length != 2) { return -14; }\n"
            "stream.close();\n"
            "stream.close();\n"
            "if (!stream.closed) { return -15; }\n"
            "var append = file.open(\"a+\");\n"
            "if (takeWriter(append) != 1) { return -16; }\n"
            "append.seek(0, \"begin\");\n"
            "if (append.readText(-1) != \"aZ!\") { return -17; }\n"
            "append.close();\n"
            "var firstOpen = exclusiveFile.open(\"x+\");\n"
            "firstOpen.writeBytes([1, 2, 3]);\n"
            "firstOpen.seek(0, \"begin\");\n"
            "var roundtrip = firstOpen.readBytes(-1);\n"
            "if (roundtrip.length != 3 || roundtrip[0] != 1 || roundtrip[2] != 3) { return -18; }\n"
            "firstOpen.close();\n"
            "var duplicateFailed = 0;\n"
            "try {\n"
            "  exclusiveFile.open(\"x\");\n"
            "} catch (e: exception.IOException) {\n"
            "  duplicateFailed = 1;\n"
            "}\n"
            "if (duplicateFailed != 1) { return -19; }\n"
            /* TODO: 此判据只观察using读取结果，未观察作用域退出后的closed/句柄状态；需在现有生命周期验收中确认close钩子的可观测检查入口。 */
            "if (readWithUsing(file) != \"aZ!\") { return -20; }\n"
            "return 1;\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    TZrChar exclusivePath[ZR_TESTS_PATH_MAX];
    char source[16384];
    char escapedFixture[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    char escapedExclusive[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs runtime supports stream modes and using");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("stream_modes", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/stream.txt", rootPath);
    snprintf(exclusivePath, sizeof(exclusivePath), "%s/exclusive.bin", rootPath);

    escape_for_zr_string_literal(escapedFixture, sizeof(escapedFixture), ZR_VM_FFI_FIXTURE_PATH);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    escape_for_zr_string_literal(escapedExclusive, sizeof(escapedExclusive), exclusivePath);

    snprintf(source, sizeof(source), kSourceTemplate, escapedFixture, escapedFile, escapedExclusive);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_stream_modes_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs runtime supports stream modes and using");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 要求缺失路径打开失败由 ZR 的 IOException 类型捕获，而不是正常返回。
 *
 * ## Summary
 * main经RUN_TEST注册本例；通过指定 IOException catch 块返回1，区分没有异常的返回0；不把任意执行失败当作预期异常被捕获。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 脚本catch返回1，未抛出返回0；C侧校验1，不检查异常消息、错误码或其他缺失路径边界。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的异常 RUN_TEST；compile_source 和 ZrTests_Function_ExecuteExpectInt64 将 ZR catch 的结果交给 Unity。
 */
static void test_system_fs_source_runtime_raises_io_exception_for_missing_file(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = import(\"zr.system.fs\");\n"
            "var exception = import(\"zr.system.exception\");\n"
            "try {\n"
            "  new fs.File(\"%s\").open(\"r\");\n"
            "  return 0;\n"
            "} catch (e: exception.IOException) {\n"
            "  return 1;\n"
            "}\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar missingPath[ZR_TESTS_PATH_MAX];
    char source[4096];
    char escapedMissing[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrInt64 result = 0;

    ZR_TEST_START("zr.system.fs runtime raises IOException for missing file");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("io_exception", rootPath, sizeof(rootPath)));
    snprintf(missingPath, sizeof(missingPath), "%s/missing.txt", rootPath);
    escape_for_zr_string_literal(escapedMissing, sizeof(escapedMissing), missingPath);
    snprintf(source, sizeof(source), kSourceTemplate, escapedMissing);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_io_exception_runtime.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_TRUE(ZrTests_Function_ExecuteExpectInt64(state, entryFunction, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs runtime raises IOException for missing file");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 让关闭后的 FileStream 进入 native整数实参边界，检查执行被拒绝。
 *
 * ## Summary
 * main经RUN_TEST注册本例；先要求源码成功编译，再检查关闭流后的 native 实参执行失败；具体失败归属仍须补诊断核查。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 只有执行失败判据，没有异常种类/诊断断言；尚不能排除其他执行失败满足该负例。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的关闭流 RUN_TEST；ZrTests_Runtime_Function_ExecuteCaptureFailure 交付失败状态，FFI 的 handle_id 分支读取 closed。
 */
static void test_system_fs_handle_id_lowering_rejects_closed_stream(void) {
    static const TZrChar *kSourceTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_tell_fd\")# fn tellFd(fd:i32): i32;\n"
            "}\n"
            "var fs = import(\"zr.system.fs\");\n"
            "var file = new fs.File(\"%s\");\n"
            "file.parent.create(true);\n"
            "var stream = file.open(\"w+\");\n"
            "stream.close();\n"
            "return tellFd(stream);\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    char source[8192];
    char escapedFixture[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    SZrTypeValue result;

    ZR_TEST_START("zr.system.fs handle_id lowering rejects closed stream");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("closed_stream", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/closed.txt", rootPath);
    escape_for_zr_string_literal(escapedFixture, sizeof(escapedFixture), ZR_VM_FFI_FIXTURE_PATH);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    snprintf(source, sizeof(source), kSourceTemplate, escapedFixture, escapedFile);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_closed_stream_ffi_lowering.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    /* TODO: 当前只要求执行失败；核对捕获的异常种类/诊断或native调用观察，区分关闭句柄拒绝与不相关的执行错误。 */
    TEST_ASSERT_FALSE(ZrTests_Runtime_Function_ExecuteCaptureFailure(state, entryFunction, &result));

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs handle_id lowering rejects closed stream");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 要求普通 ZR 的i32形参调用不接受 FileStream 作为native句柄隐式降级。
 *
 * ## Summary
 * main经RUN_TEST注册本例；只要求普通 i32 形参调用的源码编译返回NULL，不启动执行；该范围意在限制 native 边界特例泄入普通类型检查。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 本例只编译并要求NULL，不执行源码；没有具体诊断检查，其他编译错误也可能满足返回空判据。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的普通调用负例 RUN_TEST；compile_source 负责返回拒绝结果，未调用执行支持。
 */
static void test_system_fs_handle_id_lowering_does_not_apply_to_ordinary_calls(void) {
    static const TZrChar *kSourceTemplate =
            "var fs = import(\"zr.system.fs\");\n"
            "fn acceptFd(fd:i32): i32 {\n"
            "  return fd;\n"
            "}\n"
            "var file = new fs.File(\"%s\");\n"
            "var stream = file.open(\"w+\");\n"
            "return acceptFd(stream);\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    char source[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;

    ZR_TEST_START("zr.system.fs handle_id lowering does not apply to ordinary calls");
    timer.startTime = clock();

    TEST_ASSERT_TRUE(make_unique_test_root("ordinary_call", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/ordinary.txt", rootPath);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    snprintf(source, sizeof(source), kSourceTemplate, escapedFile);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_ordinary_call_no_lowering.zr");
    /* TODO: 当前NULL判据没有核对普通调用类型诊断；沿编译失败信息确认是禁止handle_id降级的规则，而不是其他语法/导入错误。 */
    TEST_ASSERT_NULL(entryFunction);

    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "zr.system.fs handle_id lowering does not apply to ordinary calls");
    ZR_TEST_DIVIDER();
}

/*
 * 专项：对照 stream_modes 失败栈中 system_fs_stream_modes_runtime.zr:22 / ip≈112，
 * 打印入口函数在源码第 22 行附近的调用类指令（FUNCTION_CALL / META_CALL / DYN_CALL 等）及操作数，
 * 并在 FFI 注册完成后检查 SymbolHandle 原型链上 ZR_META_CALL 是否可解析。
 *
 * 栈槽说明：本探测使用在首处 tellFd 截断的模板，入口函数槽位分配可与「完整 stream_modes 源」不同；
 * 在 WSL 上完整模板运行时，line 22 处 tellFd 的 PreCall 帧内槽约为 21（与截断模板 bytecode 不必一致）。
 */
/**
 * @brief 检查tellFd源码行的调用指令与一元@call契约，并展示SymbolHandle原型查询结果。
 *
 * ## Summary
 * main经RUN_TEST注册本例；编译完整 stream 模板，按源码第22行核对调用家族、一元 @call 和无数字转换；没有执行模板，原型查找输出不等于断言。
 *
 * ## Returns
 * 无返回值；由Unity硬断言或支持层失败标志交付验收，PASS日志只展示当前成功路径。
 *
 * ## Constraints and lifecycle
 * 本例只编译，不执行该脚本；line22检查依赖源码定位元数据。历史ip112仅作诊断参考，原型/元方法查询只打印。 成功取得State后，正常尾部销毁并结束计时；后续硬断言失败会跳过释放，空tearDown不回收。
 *
 * ## References
 * main 的 probe RUN_TEST；ZrCore_Exception_FindSourceLine 选择指令行，ZrLib_Type_FindPrototype/ZrCore_Prototype_GetMetaRecursively 提供诊断。
 */
static void test_tellfd_stream_call_bytecode_and_symbol_handle_meta_probe(void) {
    static const TZrChar *kProbeTemplate =
            "native extern(\"%s\") {\n"
            "  #zr.ffi.entry(\"zr_ffi_tell_fd\")# fn tellFd(fd:i32): i32;\n"
            "}\n"
            "var fs = import(\"zr.system.fs\");\n"
            "var exception = import(\"zr.system.exception\");\n"
            "fn takeReader(reader: fs.IStreamReader): string {\n"
            "  return reader.readText(1);\n"
            "}\n"
            "fn takeWriter(writer: fs.IStreamWriter): int {\n"
            "  return writer.writeText(\"!\");\n"
            "}\n"
            "fn readWithUsing(target: fs.File): string {\n"
            "  var usingStream = target.open(\"r\");\n"
            "  using usingStream;\n"
            "  return usingStream.readText(-1);\n"
            "}\n"
            "var file = new fs.File(\"%s\");\n"
            "var exclusiveFile = new fs.File(\"%s\");\n"
            "file.parent.create(true);\n"
            "var stream = file.open(\"w+\");\n"
            "if (stream.closed || stream.length != 0 || stream.position != 0) { return -1; }\n"
            "if (tellFd(stream) != 0) { return -21; }\n"
            "if (stream.mode != \"w+\") { return -2; }\n"
            "if (stream.path != file.fullPath) { return -3; }\n"
            "if (stream.writeText(\"abc\") != 3) { return -4; }\n"
            "if (stream.position != 3 || stream.length != 3) { return -5; }\n"
            "if (tellFd(stream) != 3) { return -22; }\n"
            "if (stream.seek(1, \"begin\") != 1) { return -7; }\n"
            "if (stream.writeText(\"Z\") != 1) { return -9; }\n"
            "if (stream.seek(0, \"begin\") != 0) { return -10; }\n"
            "if (takeReader(stream) != \"a\") { return -11; }\n"
            "if (stream.seek(0, \"begin\") != 0) { return -12; }\n"
            "if (stream.readText(-1) != \"aZc\") { return -13; }\n"
            "stream.setLength(2);\n"
            "if (stream.length != 2) { return -14; }\n"
            "stream.close();\n"
            "stream.close();\n"
            "if (!stream.closed) { return -15; }\n"
            "var append = file.open(\"a+\");\n"
            "if (takeWriter(append) != 1) { return -16; }\n"
            "append.seek(0, \"begin\");\n"
            "if (append.readText(-1) != \"aZ!\") { return -17; }\n"
            "append.close();\n"
            "var firstOpen = exclusiveFile.open(\"x+\");\n"
            "firstOpen.writeBytes([1, 2, 3]);\n"
            "firstOpen.seek(0, \"begin\");\n"
            "var roundtrip = firstOpen.readBytes(-1);\n"
            "if (roundtrip.length != 3 || roundtrip[0] != 1 || roundtrip[2] != 3) { return -18; }\n"
            "firstOpen.close();\n"
            "var duplicateFailed = 0;\n"
            "try {\n"
            "  exclusiveFile.open(\"x\");\n"
            "} catch (e: exception.IOException) {\n"
            "  duplicateFailed = 1;\n"
            "}\n"
            "if (duplicateFailed != 1) { return -19; }\n"
            "if (readWithUsing(file) != \"aZ!\") { return -20; }\n"
            "return 1;\n";
    SZrTestTimer timer = {0};
    TZrChar rootPath[ZR_TESTS_PATH_MAX];
    TZrChar filePath[ZR_TESTS_PATH_MAX];
    TZrChar exclusivePath[ZR_TESTS_PATH_MAX];
    char source[16384];
    char escapedFixture[4096];
    char escapedFile[ZR_TESTS_PATH_MAX * 2];
    char escapedExclusive[ZR_TESTS_PATH_MAX * 2];
    SZrState *state;
    SZrFunction *entryFunction;
    TZrUInt32 index;
    TZrUInt32 probeLine = 22;
    TZrUInt32 firstCallFamilyIpOnProbeLine = 0;
    TZrBool haveFirstCallFamilyIpOnProbeLine = ZR_FALSE;
    TZrBool sawCallFamilyOnProbeLine = ZR_FALSE;
    TZrBool sawMetaCallFamilyOnProbeLine = ZR_FALSE;
    TZrBool sawNonMetaCallFamilyOnProbeLine = ZR_FALSE;
    TZrBool sawExpectedUnaryMetaContract = ZR_FALSE;
    TZrBool sawNumericConversionOnProbeLine = ZR_FALSE;
    static const TZrUInt32 kZrTellfdProbeReportedFailureInstructionOffset = 112u;

    ZR_TEST_START("tellFd(stream) bytecode + SymbolHandle @call probe (stream_modes line 22)");
    timer.startTime = clock();

    ZR_TEST_INFO("bytecode mapping",
                 "Expect first tellFd(stream) site at source line 22 (same as failing stream_modes template).");
    /* BUG: 此处“在第一次tellFd后截断”的诊断与当前模板不符；kProbeTemplate与stream_modes模板解码后逐字相同，都是58行，会误导源码/槽位比对。本例没有执行这段模板。 */
    printf("  Note: probe template is truncated after first tellFd; A1 slot in bytecode may differ from full "
           "stream_modes entry (WSL full template: PreCall slot for tellFd at line 22 is often ~21).\n");

    TEST_ASSERT_TRUE(make_unique_test_root("tellfd_probe", rootPath, sizeof(rootPath)));
    snprintf(filePath, sizeof(filePath), "%s/stream.txt", rootPath);
    snprintf(exclusivePath, sizeof(exclusivePath), "%s/exclusive.bin", rootPath);
    escape_for_zr_string_literal(escapedFixture, sizeof(escapedFixture), ZR_VM_FFI_FIXTURE_PATH);
    escape_for_zr_string_literal(escapedFile, sizeof(escapedFile), filePath);
    escape_for_zr_string_literal(escapedExclusive, sizeof(escapedExclusive), exclusivePath);
    snprintf(source, sizeof(source), kProbeTemplate, escapedFixture, escapedFile, escapedExclusive);

    state = create_test_state();
    TEST_ASSERT_NOT_NULL(state);

    entryFunction = compile_source(state, source, "system_fs_stream_modes_runtime_probe.zr");
    TEST_ASSERT_NOT_NULL(entryFunction);
    TEST_ASSERT_NOT_NULL(entryFunction->instructionsList);
    TEST_ASSERT_TRUE(entryFunction->instructionsLength > 0);

    if (entryFunction->executionLocationInfoList == ZR_NULL || entryFunction->executionLocationInfoLength == 0) {
        printf("  executionLocationInfoList missing; line mapping unavailable (compiler metadata).\n");
    } else if (entryFunction->instructionsLength > kZrTellfdProbeReportedFailureInstructionOffset) {
        TZrUInt32 lineAtReportedIp =
                ZrCore_Exception_FindSourceLine(entryFunction,
                                                (TZrMemoryOffset)kZrTellfdProbeReportedFailureInstructionOffset);
        printf("  ZrCore_Exception_FindSourceLine(entry, instructionOffset=%u) -> line %u\n",
               (unsigned)kZrTellfdProbeReportedFailureInstructionOffset, (unsigned)lineAtReportedIp);
    }

    for (index = 0; index < entryFunction->instructionsLength; index++) {
        const TZrInstruction *inst = &entryFunction->instructionsList[index];
        EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
        TZrUInt32 line = ZrCore_Exception_FindSourceLine(entryFunction, (TZrMemoryOffset)index);

        if (line != probeLine) {
            continue;
        }

        if (opcode == ZR_INSTRUCTION_OP_TO_INT || opcode == ZR_INSTRUCTION_OP_TO_UINT) {
            sawNumericConversionOnProbeLine = ZR_TRUE;
            printf("  ip=%u line=%u opcode=%s slot=%u\n",
                   (unsigned)index,
                   (unsigned)line,
                   opcode == ZR_INSTRUCTION_OP_TO_INT ? "TO_INT" : "TO_UINT",
                   (unsigned)inst->instruction.operand.operand1[0]);
        }

        if (opcode == ZR_INSTRUCTION_OP_FUNCTION_CALL || opcode == ZR_INSTRUCTION_OP_FUNCTION_TAIL_CALL ||
            opcode == ZR_INSTRUCTION_OP_META_CALL || opcode == ZR_INSTRUCTION_OP_META_TAIL_CALL ||
            opcode == ZR_INSTRUCTION_OP_DYN_CALL || opcode == ZR_INSTRUCTION_OP_DYN_TAIL_CALL ||
            opcode == ZR_INSTRUCTION_OP_KNOWN_VM_CALL || opcode == ZR_INSTRUCTION_OP_KNOWN_VM_TAIL_CALL ||
            opcode == ZR_INSTRUCTION_OP_KNOWN_NATIVE_CALL || opcode == ZR_INSTRUCTION_OP_KNOWN_NATIVE_TAIL_CALL ||
            opcode == ZR_INSTRUCTION_OP_SUPER_META_CALL_NO_ARGS ||
            opcode == ZR_INSTRUCTION_OP_SUPER_META_TAIL_CALL_NO_ARGS ||
            opcode == ZR_INSTRUCTION_OP_SUPER_META_CALL_CACHED) {
            TZrUInt16 operandExtra = inst->instruction.operandExtra;
            TZrUInt16 a1 = inst->instruction.operand.operand1[0];
            TZrUInt16 b1 = inst->instruction.operand.operand1[1];
            const TZrChar *opcodeName = "CALL_FAMILY";

            if (!haveFirstCallFamilyIpOnProbeLine) {
                firstCallFamilyIpOnProbeLine = index;
                haveFirstCallFamilyIpOnProbeLine = ZR_TRUE;
            }
            sawCallFamilyOnProbeLine = ZR_TRUE;
            switch (opcode) {
                case ZR_INSTRUCTION_OP_FUNCTION_CALL:
                    opcodeName = "FUNCTION_CALL";
                    break;
                case ZR_INSTRUCTION_OP_FUNCTION_TAIL_CALL:
                    opcodeName = "FUNCTION_TAIL_CALL";
                    break;
                case ZR_INSTRUCTION_OP_META_CALL:
                    opcodeName = "META_CALL";
                    break;
                case ZR_INSTRUCTION_OP_META_TAIL_CALL:
                    opcodeName = "META_TAIL_CALL";
                    break;
                case ZR_INSTRUCTION_OP_DYN_CALL:
                    opcodeName = "DYN_CALL";
                    break;
                case ZR_INSTRUCTION_OP_DYN_TAIL_CALL:
                    opcodeName = "DYN_TAIL_CALL";
                    break;
                case ZR_INSTRUCTION_OP_KNOWN_VM_CALL:
                    opcodeName = "KNOWN_VM_CALL";
                    break;
                case ZR_INSTRUCTION_OP_KNOWN_VM_TAIL_CALL:
                    opcodeName = "KNOWN_VM_TAIL_CALL";
                    break;
                case ZR_INSTRUCTION_OP_KNOWN_NATIVE_CALL:
                    opcodeName = "KNOWN_NATIVE_CALL";
                    break;
                case ZR_INSTRUCTION_OP_KNOWN_NATIVE_TAIL_CALL:
                    opcodeName = "KNOWN_NATIVE_TAIL_CALL";
                    break;
                case ZR_INSTRUCTION_OP_SUPER_META_CALL_NO_ARGS:
                    opcodeName = "SUPER_META_CALL_NO_ARGS";
                    sawMetaCallFamilyOnProbeLine = ZR_TRUE;
                    break;
                case ZR_INSTRUCTION_OP_SUPER_META_TAIL_CALL_NO_ARGS:
                    opcodeName = "SUPER_META_TAIL_CALL_NO_ARGS";
                    sawMetaCallFamilyOnProbeLine = ZR_TRUE;
                    break;
                case ZR_INSTRUCTION_OP_SUPER_META_CALL_CACHED:
                    opcodeName = "SUPER_META_CALL_CACHED";
                    sawMetaCallFamilyOnProbeLine = ZR_TRUE;
                    if (entryFunction->callSiteCaches != ZR_NULL && b1 < entryFunction->callSiteCacheLength) {
                        const SZrFunctionCallSiteCacheEntry *cache = &entryFunction->callSiteCaches[b1];
                        if (cache->kind == ZR_FUNCTION_CALLSITE_CACHE_KIND_META_CALL && cache->argumentCount == 1) {
                            sawExpectedUnaryMetaContract = ZR_TRUE;
                        }
                    }
                    break;
                default:
                    break;
            }

            if (opcode == ZR_INSTRUCTION_OP_META_CALL || opcode == ZR_INSTRUCTION_OP_META_TAIL_CALL) {
                sawMetaCallFamilyOnProbeLine = ZR_TRUE;
                if (b1 == 1) {
                    sawExpectedUnaryMetaContract = ZR_TRUE;
                }
            } else if (opcode != ZR_INSTRUCTION_OP_SUPER_META_CALL_NO_ARGS &&
                       opcode != ZR_INSTRUCTION_OP_SUPER_META_TAIL_CALL_NO_ARGS &&
                       opcode != ZR_INSTRUCTION_OP_SUPER_META_CALL_CACHED) {
                sawNonMetaCallFamilyOnProbeLine = ZR_TRUE;
            }

            printf("  ip=%u line=%u opcode=%s E=%u A1(functionSlot)=%u B1(argOrAux)=%u\n", (unsigned)index,
                   (unsigned)line, opcodeName, (unsigned)operandExtra, (unsigned)a1, (unsigned)b1);
        }
    }

    if (haveFirstCallFamilyIpOnProbeLine) {
        TZrUInt32 lineAtFirstCallIp =
                ZrCore_Exception_FindSourceLine(entryFunction, (TZrMemoryOffset)firstCallFamilyIpOnProbeLine);
        printf("  ZrCore_Exception_FindSourceLine(entry, first call-family ip=%u) -> line %u\n",
               (unsigned)firstCallFamilyIpOnProbeLine, (unsigned)lineAtFirstCallIp);
    }

    TEST_ASSERT_TRUE_MESSAGE(sawCallFamilyOnProbeLine,
                             "expected at least one call-family opcode on probe source line (tellFd site)");
    TEST_ASSERT_TRUE_MESSAGE(sawMetaCallFamilyOnProbeLine,
                             "tellFd(stream) should lower through META_CALL/@call semantics on line 22");
    TEST_ASSERT_FALSE_MESSAGE(sawNonMetaCallFamilyOnProbeLine,
                              "tellFd(stream) should not remain on FUNCTION/DYN/KNOWN call families");
    TEST_ASSERT_TRUE_MESSAGE(sawExpectedUnaryMetaContract,
                             "tellFd(stream) should preserve a unary @call contract at the probe site");
    TEST_ASSERT_FALSE_MESSAGE(sawNumericConversionOnProbeLine,
                              "tellFd(stream) should preserve the stream wrapper for ffi handle_id lowering");

    printf("Testing SymbolHandle prototype ZR_META_CALL:\n");
    {
        /* TODO: 两个SymbolHandle原型及ZR_META_CALL查询只打印，没有非空/一致性断言；按专项意图核查是否应把原型可调用性纳入本用例验收。 */
        SZrObjectPrototype *prototypeByFqn = ZrLib_Type_FindPrototype(state, "zr.ffi.SymbolHandle");
        SZrObjectPrototype *prototypeShort = ZrLib_Type_FindPrototype(state, "SymbolHandle");
        SZrMeta *metaFqn =
                prototypeByFqn != ZR_NULL
                        ? ZrCore_Prototype_GetMetaRecursively(state->global, prototypeByFqn, ZR_META_CALL)
                        : ZR_NULL;
        SZrMeta *metaShort =
                prototypeShort != ZR_NULL
                        ? ZrCore_Prototype_GetMetaRecursively(state->global, prototypeShort, ZR_META_CALL)
                        : ZR_NULL;

        printf("  ZrLib_Type_FindPrototype(\"zr.ffi.SymbolHandle\") -> %p\n", (void *)prototypeByFqn);
        printf("  ZrLib_Type_FindPrototype(\"SymbolHandle\") -> %p\n", (void *)prototypeShort);
        printf("  ZrCore_Prototype_GetMetaRecursively(..., ZR_META_CALL) fqn -> %p short -> %p\n", (void *)metaFqn,
               (void *)metaShort);
    }

    ZrCore_Function_Free(state, entryFunction);
    destroy_test_state(state);
    timer.endTime = clock();
    ZR_TEST_PASS(timer, "tellFd(stream) bytecode + SymbolHandle @call probe");
    ZR_TEST_DIVIDER();
}

/**
 * @brief 组织system_fs八个用例并把Unity失败计数交给进程调用方。
 *
 * ## Summary
 * 注册顺序先查元数据、路径/拷贝，再探测调用字节码与流/异常/降级边界；日志分隔线不负责注册。
 *
 * ## Returns
 * @return UNITY_END的失败计数；零值仅表示没有被Unity计为失败，忽略用例仍独立统计。
 *
 * ## Constraints and lifecycle
 * 直接启动可执行目标或由CTest system_fs runner派发；文件/FFI用例有实际副作用，生命周期和清理由每个用例负责。
 *
 * ## References
 * zr_vm_system_fs_test目标；CTest system_fs调用run_executable_suite并消费退出码。
 */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_system_fs_module_metadata_exposes_object_surface_and_wrapper_fields);
    RUN_TEST(test_system_fs_source_runtime_supports_path_objects_and_directory_operations);
    RUN_TEST(test_system_fs_copy_result_supports_exists_and_read_text_separately);
    RUN_TEST(test_tellfd_stream_call_bytecode_and_symbol_handle_meta_probe);
    RUN_TEST(test_system_fs_source_runtime_supports_stream_modes_and_using);
    RUN_TEST(test_system_fs_source_runtime_raises_io_exception_for_missing_file);
    RUN_TEST(test_system_fs_handle_id_lowering_rejects_closed_stream);
    RUN_TEST(test_system_fs_handle_id_lowering_does_not_apply_to_ordinary_calls);

    return UNITY_END();
}
