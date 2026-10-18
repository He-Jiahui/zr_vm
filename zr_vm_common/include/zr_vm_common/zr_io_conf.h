//
// Created by HeJiahui on 2025/7/6.
//

#ifndef ZR_IO_CONF_H
#define ZR_IO_CONF_H

/** @brief 字符读取器结束或无法补充数据时返回的 stdio 哨兵，不是有效输入字节。 */
#define ZR_IO_EOF EOF
#include "zr_vm_common/zr_common_conf.h"

/* 写端与读端使用同一端序标识；该静态对象只描述本宿主字节序，不表示文件自身可随意交换端序。 */
static const union {
    TZrUInt64 dummy;
    TZrBool littleEndian;
} CZrIoEndian = {1};

#define ZR_IO_IS_LITTLE_ENDIAN (CZrIoEndian.littleEndian)

/* .zro 头部格式由 parser writer 与 core reader 共同消费；字段宽度和 patch 门槛属于持久化协议。 */
// IO文件格式相关常量
/** @brief .zro 文件头宣告的原生整数、长度和指令宽度。
 * @note 当前 writer 仍用本宿主 sizeof 写入原生长度与数据；这些宣告不能单独证明跨位宽、
 * 跨端序可读，也不能代替读取端的实际平台布局约束。
 */
#define ZR_IO_NATIVE_INT_SIZE 8
#define ZR_IO_SIZE_T_SIZE 8
#define ZR_IO_INSTRUCTION_SIZE 8
#define ZR_IO_VERSION_FORMAT_SHIFT_BITS 32
#define ZR_IO_SOURCE_SIGNATURE_LENGTH 4U
#define ZR_IO_SOURCE_HEADER_OPT_BYTES 3U

/* 历史布局速记 .SOURCE；非现行格式规范；当前规则见 EOF 同名单元。
 *  0 SIGNATURE 4
 *  4 VERSION_MAJOR 4
 *  8 VERSION_MINOR 4
 *  12 VERSION_PATCH 4
 *  16 FORMAT 8
 *  24 NATIVE_INT_SIZE 1
 *  25 SIZE_T_SIZE 1
 *  26 INSTRUCTION_SIZE 1
 *  27 ENDIAN 1
 *  28 DEBUG 1
 *  29 OPT 3
 *  32 MODULES_LENGTH 8
 *  40 MODULES [.MODULE]
 */
#define ZR_IO_SOURCE_SIGNATURE "\x1ZR\x2"
/* writer 写入当前 patch，ReadSourceNew 拒绝未来 patch；这些门槛还决定历史字段的读取布局，
 * 因此新增字段时须同步写端和读端，不能只提高版本号。 */
#define ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA 2U
#define ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB 3U
#define ZR_IO_SOURCE_PATCH_HAS_SEMIR_METADATA 4U
#define ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES 5U
#define ZR_IO_SOURCE_PATCH_HAS_CALLSITE_CACHE 7U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_DECORATOR_METADATA 10U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA 11U
#define ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS 12U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_IDENTITY 13U
#define ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS 14U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES 15U
#define ZR_IO_SOURCE_PATCH_HAS_MODULE_INIT_METADATA 16U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA 17U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_CALLABLE_RETURN_TYPE 18U
#define ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT 19U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_MODEL 20U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_SIGNATURE_HASH 21U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_OWNER_CHAIN 22U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_HASH 23U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_TARGET_SIGNATURE_TOKENS 24U
#define ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_REF_TABLE 25U
#define ZR_IO_SOURCE_PATCH_HAS_MODULE_SIGNATURE_HASH 26U
#define ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_TARGET_MODULE_HASH 27U
#define ZR_IO_SOURCE_PATCH_HAS_MODULE_METADATA_BINDINGS 28U
#define ZR_IO_SOURCE_PATCH_HAS_ASSEMBLY_REF_VERSION_RANGE 29U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_STRING_HEAP 30U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_TOKEN_LAYOUT_IDENTITY 31U
#define ZR_IO_SOURCE_PATCH_HAS_METADATA_BINDING_LAYOUT_IDENTITY 32U
#define ZR_IO_SOURCE_PATCH_HAS_MODULE_EFFECT_ASSEMBLY_NAME 33U
#define ZR_IO_SOURCE_PATCH_HAS_SEMIR_STATIC_C_TYPES 34U
#define ZR_IO_SOURCE_PATCH_HAS_BORROWED_FRAME_ALIAS 35U
#define ZR_IO_SOURCE_PATCH_HAS_VM_ENTRY_CLEAR_STACK_SIZE 36U
#define ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_CANONICAL_BINDINGS 37U
#define ZR_IO_SOURCE_PATCH_HAS_TYPED_LOCAL_ROLE_FLAGS 38U
#define ZR_IO_SOURCE_PATCH_HAS_NATIVE_IMPORT_CONTRACTS 39U
#define ZR_IO_SOURCE_PATCH_HAS_TEST_MANIFEST 40U
#define ZR_IO_SOURCE_PATCH_HAS_TYPED_CLOSURE_CAPTURE_BINDINGS 41U
#define ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_GENERIC_PARAMETERS 42U
#define ZR_IO_SOURCE_PATCH_HAS_CALL_BINDING 43U
#define ZR_IO_SOURCE_PATCH_HAS_MARK_CLOSE_PROXY 44U
/* 当前 writer 输出的 patch；reader 拒绝更高版本，并按历史门槛解析可选字段。 */
/* CREATE_ARRAY operand2[0] is the initial length for checked literal stores. */
#define ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH 45U
/* Frame coordinates are (physical stack-slot index, intra-slot byte offset).
 * Earlier artifacts contain ABI-dependent raw offsets; rebuild from source. */
#define ZR_IO_SOURCE_PATCH_HAS_SLOT_RELATIVE_FRAME_LAYOUT 46U
#define ZR_IO_SOURCE_PATCH_MIN_SUPPORTED ZR_IO_SOURCE_PATCH_HAS_SLOT_RELATIVE_FRAME_LAYOUT
#define ZR_IO_SOURCE_PATCH_CURRENT ZR_IO_SOURCE_PATCH_HAS_SLOT_RELATIVE_FRAME_LAYOUT

/* 历史布局速记 .MODULE；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * MD5 [string]
 * IMPORTS_LENGTH [8]
 * IMPORTS [.IMPORT]
 * DECLARES_LENGTH [8]
 * DECLARES [.DECLARE]
 * ENTRY [.FUNCTION]
 */


/* 历史布局速记 .IMPORT；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * MD5 [string]
 *
 */

/* 历史布局速记 .META；非现行格式规范；当前规则见 EOF 同名单元。
 * META_TYPE [4]
 * FUNCTIONS_LENGTH [8]
 * FUNCTIONS [.FUNCTION]
 */

/* 历史布局速记 .METHOD；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * FUNCTIONS_LENGTH [8]
 * FUNCTIONS [.FUNCTION]
 */

/* 历史布局速记 .PROPERTY；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * PROPERTY_TYPE [4] [DECLARE|GETTER|SETTER]
 * GETTER_FUNCTION [.FUNCTION]
 * SETTER_FUNCTION [.FUNCTION]
 */

/* 历史布局速记 .FIELD；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 *
 */

/* 历史布局速记 .INSTRUCTION；非现行格式规范；当前规则见 EOF 同名单元。
 * RAW [8]
 */

/* 历史布局速记 .LOCAL；非现行格式规范；当前规则见 EOF 同名单元。
 * // TYPE [4]
 * // DATA [string|uint|int|float|...]
 * INSTRUCTION_START [8]
 * INSTRUCTION_END [8]
 * START_LINE [8] (DEBUG)
 * END_LINE [8] (DEBUG)
 */

/* 历史布局速记 .CONSTANT；非现行格式规范；当前规则见 EOF 同名单元。
 * TYPE [4]
 * DATA [string|uint|int|float|...]
 * START_LINE [8] (DEBUG)
 * END_LINE [8] (DEBUG)
 */

/* 历史布局速记 .CLOSURE；非现行格式规范；当前规则见 EOF 同名单元。
 * SUB_FUNCTION [.FUNCTION]
 * todo
 */

/* 历史布局速记 .DEBUG_INFO；非现行格式规范；当前规则见 EOF 同名单元。
 * SOURCE_FILE [string]
 * SOURCE_HASH [string]
 * INSTRUCTIONS_LENGTH [8]
 * INSTRUCTION_LINES [8 * INSTRUCTIONS_LENGTH]
 * if VERSION_PATCH >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES:
 *   INSTRUCTION_RANGES [startLine[4], startColumn[4], endLine[4], endColumn[4]] * INSTRUCTIONS_LENGTH
 */


/* TODO: 下列 .FUNCTION 布局速记未列出现行 writer 写入的 closure/exception 元数据等字段；
 * 应以 parser writer 与 core reader 的实际字段顺序和 patch 门槛校订线格式说明。 */
/* 历史布局速记 .FUNCTION；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * START_LINE [8]
 * END_LINE [8]
 * PARAMETERS_LENGTH [8]
 * HAS_VAR_ARGS [8]
 * STACK_SIZE [4]
 * if VERSION_PATCH >= ZR_IO_SOURCE_PATCH_HAS_VM_ENTRY_CLEAR_STACK_SIZE:
 *   VM_ENTRY_CLEAR_STACK_SIZE_PLUS_ONE [4]
 * if VERSION_PATCH >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_FRAME_LAYOUT:
 *   FRAME_BYTE_SIZE [4]
 *   FRAME_BYTE_ALIGN [4]
 *   FRAME_SLOT_LAYOUTS_LENGTH [8]
 *   FRAME_SLOT_LAYOUTS [stackSlot[4], byteOffset[4], byteSize[4], byteAlign[4], typeLayoutId[4],
 *                       slotKind[1], isParameter[1], reserved0[2]] * FRAME_SLOT_LAYOUTS_LENGTH
 * INSTRUCTIONS_LENGTH [8]
 * INSTRUCTIONS [.INSTRUCTION]
 * LOCAL_LENGTH [8]
 * LOCALS [.LOCAL]
 * CONSTANTS_LENGTH [8]
 * CONSTANTS [.CONSTANT]
 * PROTOTYPES_LENGTH [8]  (prototype 总数)
 * CLASS_COUNT [8]  (CLASS prototype 数量)
 * CLASSES [.CLASS]  (CLASS prototype 数组)
 * STRUCT_COUNT [8]  (STRUCT prototype 数量)
 * STRUCTS [.STRUCT]  (STRUCT prototype 数组)
 * CLOSURES_LENGTH [8]
 * CLOSURES [.CLOSURE]
 * DEBUG_INFO_LENGTH [8]
 * DEBUG_INFO [.DEBUG_INFO] (DEBUG)
 */


/* 历史布局速记 .DECLARE；非现行格式规范；当前规则见 EOF 同名单元。
 * TYPE [4]
 * BODY [.CLASS|.STRUCT|.INTERFACE|.FUNCTION|.ENUM|.FIELD]
 */

/* 历史布局速记 .CLASS；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * SUPER_CLASS_LENGTH[8]
 * SUPER_CLASSES [.REFERENCE]
 * GENERIC_LENGTH [8]
 * DECLARES_LENGTH [8]
 * DECLARES [.CLASS_DECLARE] [.FIELD|.PROPERTY|.METHOD|.META]
 */


/* 历史布局速记 .STRUCT；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * SUPER_STRUCT_LENGTH [8]
 * SUPER_STRUCTS [.REFERENCE]
 * GENERIC_LENGTH [8]
 * DECLARES_LENGTH [8]
 * DECLARES [.STRUCT_DECLARE] [.FIELD|.METHOD|.META]
 */

/* 历史布局速记 .INTERFACE；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * SUPER_INTERFACE_LENGTH [8]
 * SUPER_INTERFACES [.REFERENCE]
 * GENERIC_LENGTH [8]
 * DECLARES_LENGTH [8]
 * DECLARES [.INTERFACE_DECLARE] [.FIELD|.PROPERTY|.METHOD|.META]
 */

/* 历史布局速记 .ENUM_FIELD；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * DATA [string|uint|int|float|...]
 */

/* 历史布局速记 .ENUM；非现行格式规范；当前规则见 EOF 同名单元。
 * NAME [string]
 * FLAG_TYPE [4]
 * ENUM_LENGTH [8]
 * ENUMS [.ENUM_DECLARE] [.ENUM_FIELD]
 */

/** @brief 二进制模块声明的线格式类别，读写端须保持同序。 */
/** @note 当前二进制 writer 的顶层 declaresLength 为零；这些类别仍供 reader
 * 的结构化声明分支及对应释放器判别子树，不表示 writer 已输出所有类别。
 */
enum EZrIoModuleDeclareType {
    ZR_IO_MODULE_DECLARE_TYPE_CLASS,
    ZR_IO_MODULE_DECLARE_TYPE_STRUCT,
    ZR_IO_MODULE_DECLARE_TYPE_INTERFACE,
    ZR_IO_MODULE_DECLARE_TYPE_FUNCTION,
    ZR_IO_MODULE_DECLARE_TYPE_ENUM,
    ZR_IO_MODULE_DECLARE_TYPE_FIELD,

    ZR_IO_MODULE_DECLARE_TYPE_ENUM_MAX
};

typedef enum EZrIoModuleDeclareType EZrIoModuleDeclareType;

/** @brief 二进制成员声明类别，供 core 重建字段、属性、方法与元方法。 */
/** @note ENUM 在此表示 enumField 子树；模块级 ENUM 则对应完整枚举，两层标记不能混用。 */
enum EZrIoMemberDeclareType {
    ZR_IO_MEMBER_DECLARE_TYPE_FIELD,
    ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY,
    ZR_IO_MEMBER_DECLARE_TYPE_METHOD,
    ZR_IO_MEMBER_DECLARE_TYPE_META,
    ZR_IO_MEMBER_DECLARE_TYPE_ENUM,

    ZR_IO_MEMBER_DECLARE_TYPE_ENUM_MAX
};

typedef enum EZrIoMemberDeclareType EZrIoMemberDeclareType;

/* TODO: 声明状态预留 DECLARED/DEFINED/STATIC 位值，但当前仅见 io.h 字段持有，
 * 尚未找到 parser writer 或 core loader 对这些位的实际编码；需确认是否仍属线格式。 */
/** @brief 声明状态位；STATIC 可与 DECLARED/DEFINED 组合，不能按互斥枚举解释。 */
enum EZrIoMemberDeclareStatus {
    ZR_IO_MEMBER_DECLARE_STATUS_NONE = 0,
    ZR_IO_MEMBER_DECLARE_STATUS_DECLARED = 1,
    ZR_IO_MEMBER_DECLARE_STATUS_DEFINED = 2,
    ZR_IO_MEMBER_DECLARE_STATUS_STATIC = 4,

    ZR_IO_MEMBER_DECLARE_STATUS_ENUM_MAX
};

typedef enum EZrIoMemberDeclareStatus EZrIoMemberDeclareStatus;


#endif // ZR_IO_CONF_H

/**
 * @def ZR_IO_EOF
 * @brief ZR_IO_EOF：在 IO 字符读取路径区分无法取得下一个字节与有效字节值。
 *
 * ## Summary
 * 在 IO 字符读取路径区分无法取得下一个字节与有效字节值。
 *
 * ## Value model
 * 展开为 stdio 的 EOF 哨兵，不是合法输入字节。
 *
 * ## Invariants and lifecycle
 * 本头不自行包含 stdio.h，展开处须已引入 EOF 定义。当前 io_read_char 在 Read 不能取得一个字节时返回该值，不能据此区分截断、回调失败及真正文件末尾。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:9：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:68：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:70：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_IS_LITTLE_ENDIAN
 * @brief ZR_IO_IS_LITTLE_ENDIAN：将本宿主首字节端序观察用于写出头标记及读入门禁。
 *
 * ## Summary
 * 将本宿主首字节端序观察用于写出头标记及读入门禁。
 *
 * ## Value model
 * 读取每 TU 私有 CZrIoEndian 的 unsigned-char 视图。
 *
 * ## Invariants and lifecycle
 * 不是字节序转换器；writer 写宿主标记，reader 不匹配即拒绝，不会按该宏自动调整后续数据。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:18：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:43：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2365：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/native_call/native_call_contract.c:1149：定义、配套消费或失败边界证据。
 * - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_contract.c:415：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_ffi_contract.c:398：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_NATIVE_INT_SIZE
 * @brief ZR_IO_NATIVE_INT_SIZE：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 8。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:26：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2363：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:40：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_SIZE_T_SIZE
 * @brief ZR_IO_SIZE_T_SIZE：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 8。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:27：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2363：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:41：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_INSTRUCTION_SIZE
 * @brief ZR_IO_INSTRUCTION_SIZE：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 8。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:28：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2364：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:42：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_VERSION_FORMAT_SHIFT_BITS
 * @brief ZR_IO_VERSION_FORMAT_SHIFT_BITS：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 32。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:29：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:39：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2362：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_SOURCE_SIGNATURE_LENGTH
 * @brief ZR_IO_SOURCE_SIGNATURE_LENGTH：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 4（不含字符串字面量的隐式末尾 NUL）。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:30：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2344：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - scripts/audit_magic_constants.py:122：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_SOURCE_HEADER_OPT_BYTES
 * @brief ZR_IO_SOURCE_HEADER_OPT_BYTES：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 3 个保留字节。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:31：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2366：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:45：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:79：定义、配套消费或失败边界证据。
 * - scripts/audit_magic_constants.py:122：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_SOURCE_SIGNATURE
 * @brief ZR_IO_SOURCE_SIGNATURE：为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Summary
 * 为 .zro 文件头的写入和有效性门禁提供协议常量。
 *
 * ## Value model
 * 四字节 0x01、Z、R、0x02。
 *
 * ## Invariants and lifecycle
 * 固定宣告不代替实际 sizeof 与原生读写布局；当前 reader 会核对相应宽度、版本、端序和保留字节。修改须同步配套 writer、reader 与既有损坏头测试。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:48：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2358：定义、配套消费或失败边界证据。
 * - scripts/audit_magic_constants.py:121：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_SOURCE_PATCH_CURRENT
 * @brief ZR_IO_SOURCE_PATCH_CURRENT：指定当前 writer 输出的语义/布局版本，并限制 reader 接受的最高版本。
 *
 * ## Summary
 * 指定当前 writer 输出的语义/布局版本，并限制 reader 接受的最高版本。
 *
 * ## Value model
 * 当前别名为 ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH，即 45。
 *
 * ## Invariants and lifecycle
 * reader 还要求 patch >= MODULE_INIT_METADATA(16)，不能把保留的低版本阈值当作仍可加载的文件版本。阈值变化必须同步实际字段/指令验证，不是仅改头数字。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:94：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:38：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2360：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2361：定义、配套消费或失败边界证据。
 * - tests/artifact/test_manifest_roundtrip.c:117：定义、配套消费或失败边界证据。
 * - tests/core/test_io_validation.c:58：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrIoModuleDeclareType
 * @brief EZrIoModuleDeclareType：为模块级声明的持久化子树选择 class、struct、interface、function、enum 或 field 分支。
 *
 * ## Summary
 * 为模块级声明的持久化子树选择 class、struct、interface、function、enum 或 field 分支。
 *
 * ## Value model
 * 各成员按声明顺序编号，ENUM_MAX 是界限，不是有效声明。
 *
 * ## Invariants and lifecycle
 * 模块层 ENUM 代表完整枚举，与成员层 ENUM 的单 enumField 不同。loader 必须按标签分配并释放正确子树；当前 binary writer 的简单模块壳不生成所有模块声明分支。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:256：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2222：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2225：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:79：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2230：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2235：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2240：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrIoMemberDeclareType
 * @brief EZrIoMemberDeclareType：为类型内部成员的 IO 子树选择 field/property/method/meta/enumField 分支。
 *
 * ## Summary
 * 为类型内部成员的 IO 子树选择 field/property/method/meta/enumField 分支。
 *
 * ## Value model
 * 互斥编号标签，ENUM_MAX 不是可解码成员。
 *
 * ## Invariants and lifecycle
 * 成员 ENUM 是一个字段子树，不能替代模块完整枚举。当前类/结构 writer 先写稳定类型 stub，成员载荷由 prototypeData 恢复；不能据 enum 存在断言 writer 已输出所有分支。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:271：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2122：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2140：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2124：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2130：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2135：定义、配套消费或失败边界证据。
 */

/**
 * @enum EZrIoMemberDeclareStatus
 * @brief EZrIoMemberDeclareStatus：为声明状态保留 DECLARED、DEFINED 与 STATIC 位标志。
 *
 * ## Summary
 * 为声明状态保留 DECLARED、DEFINED 与 STATIC 位标志。
 *
 * ## Value model
 * NONE=0、DECLARED=1、DEFINED=2、STATIC=4；末尾自动编号 ENUM_MAX 为 5，并不是全部位的合法上界掩码。
 *
 * ## Invariants and lifecycle
 * STATIC 可与声明/定义位组合，不能当作互斥枚举。TODO: 当前仅见 io.h 的 status 字段，未找到 writer/reader 实际编码；在 io_read_member_declares 与类型 stub writer 核实后再决定是否仍属线格式。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:286：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/io.h:606：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2119：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:295：定义、配套消费或失败边界证据。
 */

/**
 * @typedef EZrIoModuleDeclareType
 * @brief EZrIoModuleDeclareType.typedef_alias：为 EZrIoModuleDeclareType 标签提供免写 enum 前缀的 C 类型别名。
 *
 * ## Summary
 * 为 EZrIoModuleDeclareType 标签提供免写 enum 前缀的 C 类型别名。
 *
 * ## Value model
 * typedef enum EZrIoModuleDeclareType EZrIoModuleDeclareType；共享枚举的数值域和实现选择的底层表示。
 *
 * ## Invariants and lifecycle
 * 模块层 ENUM 代表完整枚举，与成员层 ENUM 的单 enumField 不同。loader 必须按标签分配并释放正确子树；当前 binary writer 的简单模块壳不生成所有模块声明分支。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:267：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2222：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2225：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:79：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:256：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/io.h:642：定义、配套消费或失败边界证据。
 */

/**
 * @typedef EZrIoMemberDeclareType
 * @brief EZrIoMemberDeclareType.typedef_alias：为 EZrIoMemberDeclareType 标签提供免写 enum 前缀的 C 类型别名。
 *
 * ## Summary
 * 为 EZrIoMemberDeclareType 标签提供免写 enum 前缀的 C 类型别名。
 *
 * ## Value model
 * typedef enum EZrIoMemberDeclareType EZrIoMemberDeclareType；共享枚举的数值域和实现选择的底层表示。
 *
 * ## Invariants and lifecycle
 * 成员 ENUM 是一个字段子树，不能替代模块完整枚举。当前类/结构 writer 先写稳定类型 stub，成员载荷由 prototypeData 恢复；不能据 enum 存在断言 writer 已输出所有分支。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:281：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2122：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2140：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2222：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:271：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/io.h:605：定义、配套消费或失败边界证据。
 */

/**
 * @typedef EZrIoMemberDeclareStatus
 * @brief EZrIoMemberDeclareStatus.typedef_alias：为 EZrIoMemberDeclareStatus 标签提供免写 enum 前缀的 C 类型别名。
 *
 * ## Summary
 * 为 EZrIoMemberDeclareStatus 标签提供免写 enum 前缀的 C 类型别名。
 *
 * ## Value model
 * typedef enum EZrIoMemberDeclareStatus EZrIoMemberDeclareStatus；共享枚举的数值域和实现选择的底层表示。
 *
 * ## Invariants and lifecycle
 * STATIC 可与声明/定义位组合，不能当作互斥枚举。TODO: 当前仅见 io.h 的 status 字段，未找到 writer/reader 实际编码；在 io_read_member_declares 与类型 stub writer 核实后再决定是否仍属线格式。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:295：定义、配套消费或失败边界证据。
 * - zr_vm_core/include/zr_vm_core/io.h:606：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2119：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:286：定义、配套消费或失败边界证据。
 */

/**
 * @var CZrIoEndian
 * @brief CZrIoEndian：以只读联合体观察本宿主整数 1 的第一个字节。
 *
 * ## Summary
 * 以只读联合体观察本宿主整数 1 的第一个字节。
 *
 * ## Fields
 * static const 匿名 union 每个包含本头的 TU 各有一份；dummy 为 TZrUInt64，littleEndian 为 TZrBool，与 dummy 共用存储。
 *
 * ## Invariants and lifecycle
 * dummy 初始化为 1 后只读 littleEndian 视图。字段组并非两个独立状态；不携带文件端序、线程状态或所有权，不能变成运行期文件换序开关。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:13：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:18：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:43：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_CZrIoEndian_type CZrIoEndian.type
 * @brief CZrIoEndian.type：以只读联合体观察本宿主整数 1 的第一个字节。
 *
 * ## Summary
 * 以只读联合体观察本宿主整数 1 的第一个字节。
 *
 * ## Fields
 * static const 匿名 union 每个包含本头的 TU 各有一份；dummy 为 TZrUInt64，littleEndian 为 TZrBool，与 dummy 共用存储。
 *
 * ## Invariants and lifecycle
 * dummy 初始化为 1 后只读 littleEndian 视图。字段组并非两个独立状态；不携带文件端序、线程状态或所有权，不能变成运行期文件换序开关。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:13：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:18：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:43：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_CZrIoEndian_views CZrIoEndian.views
 * @brief CZrIoEndian.views：以只读联合体观察本宿主整数 1 的第一个字节。
 *
 * ## Summary
 * 以只读联合体观察本宿主整数 1 的第一个字节。
 *
 * ## Fields
 * static const 匿名 union 每个包含本头的 TU 各有一份；dummy 为 TZrUInt64，littleEndian 为 TZrBool，与 dummy 共用存储。
 *
 * ## Invariants and lifecycle
 * dummy 初始化为 1 后只读 littleEndian 视图。字段组并非两个独立状态；不携带文件端序、线程状态或所有权，不能变成运行期文件换序开关。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:14：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:13：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:18：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:43：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:16：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_FUNCTION wire_layout_note.FUNCTION
 * @brief wire_layout_note.FUNCTION：核对 .FUNCTION 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .FUNCTION 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 函数从 name、起止行、参数数、varargs、32位 stack size 开始，版本化 entry-clear/frame-layout 后接 instructions、locals、captures、exception tables、constants、exports、typed bindings、metadata/effects、native imports、prototypes/blob、child functions 与 debug。
 *
 * ## Invariants and lifecycle
 * writer 当前 patch45输出固定当前顺序，reader 按每个阈值补缺省值或读取。TODO: 旧速记遗漏/错写多段，不能作为生成器或解码器规范；维护此附录须逐项核对 write_io_function_internal 与 io_read_functions，不以一次成功读写推断所有分支完整。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:174：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:1758：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:1675：定义、配套消费或失败边界证据。
 */

/**
 * @def ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH
 * @brief ZR_IO_SOURCE_PATCH_HAS_ARRAY_INITIAL_LENGTH：为 ARRAY_INITIAL_LENGTH 的读取布局或指令语义选择版本门槛。
 *
 * ## Summary
 * 为 ARRAY_INITIAL_LENGTH 的读取布局或指令语义选择版本门槛。
 *
 * ## Value model
 * 阈值常量 45U；不是独立文件格式或可加载版本全集。
 *
 * ## Invariants and lifecycle
 * 当前 writer 写 patch45，reader 只接受16..45；低版本阈值仍参与条件代码但不扩大顶层接受范围。字段或 opcode 含义以该符号实际消费者为准。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:93：定义、配套消费或失败边界证据。
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:94：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io_bytecode_verify.h:359：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_SOURCE wire_layout_note.SOURCE
 * @brief wire_layout_note.SOURCE：核对 .SOURCE 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .SOURCE 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 文件壳头以签名、major/minor/patch、打包 format、三种宽度、宿主端序、debug 与三保留字节开始，再写模块数量和模块。
 *
 * ## Invariants and lifecycle
 * writer_binary 使用原生固定标量与 TZrSize 写出；当前 reader 拒绝版本、位宽、端序或保留字段不符。旧偏移图仅适用于其已满足的宿主布局，不承诺跨宿主可读。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:33：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:67：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2343：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_MODULE wire_layout_note.MODULE
 * @brief wire_layout_note.MODULE：核对 .MODULE 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .MODULE 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 模块子树依次读 name、md5、imports count/list、declares count/list、entry function。
 *
 * ## Invariants and lifecycle
 * 当前 WriteBinaryFile 只构造一个模块并写零 imports/declares，再写入口函数；loader 支持的广义声明树不代表当前 writer 会生成它们。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:96：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:75：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2263：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_IMPORT wire_layout_note.IMPORT
 * @brief wire_layout_note.IMPORT：核对 .IMPORT 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .IMPORT 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 导入记录按顺序读取 name 与 md5 两个带长度字符串。
 *
 * ## Invariants and lifecycle
 * 字符串以原生 TZrSize 长度加原始字节编码；当前 binary writer 的模块壳将 imports count 置零，不能把此说明当作实际外部导入解析/加载保证。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:107：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:160：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer_binary.c:79：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_META wire_layout_note.META
 * @brief wire_layout_note.META：核对 .META 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .META 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * meta 成员读 uint32 metaType、原生函数数量与函数子树。
 *
 * ## Invariants and lifecycle
 * 这是 loader 的成员分支；当前 prototype class/struct writer 输出零成员 stub，非完整任意 meta writer。旧图中的4/8是速记，不是所有 C enum 和 size_t 的可移植布局证明。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:113：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2100：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_METHOD wire_layout_note.METHOD
 * @brief wire_layout_note.METHOD：核对 .METHOD 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .METHOD 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * method 成员依次读 name、原生函数数量与函数数组。
 *
 * ## Invariants and lifecycle
 * 成员判别式已由外层读取；函数子树递归遵守 patch 门禁。当前类型 stub 不写 method 载荷，不能拿 reader 分支存在当作 roundtrip 已覆盖。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:119：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2079：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_PROPERTY wire_layout_note.PROPERTY
 * @brief wire_layout_note.PROPERTY：核对 .PROPERTY 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .PROPERTY 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * property 成员依次读 name、32位 propertyType、一个 getter 与一个 setter 函数子树。
 *
 * ## Invariants and lifecycle
 * loader 固定读取两棵函数树；名称 GETTER/SETTER 不代表省略分支的可选编码。宽度约定仍受 reader 现有 TODO 核查，当前 stub writer 不输出该成员载荷。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:125：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2088：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:110：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_FIELD wire_layout_note.FIELD
 * @brief wire_layout_note.FIELD：核对 .FIELD 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .FIELD 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * field 成员载荷只读取带长度的 name。
 *
 * ## Invariants and lifecycle
 * 字段的类型判别式由 member/module 外层决定；这一载荷速记不能被扩展成对象字段值、偏移或所有权完整格式。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:132：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:371：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2145：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_INSTRUCTION wire_layout_note.INSTRUCTION
 * @brief wire_layout_note.INSTRUCTION：核对 .INSTRUCTION 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .INSTRUCTION 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 函数指令序列以实际 TZrInstruction 原始值写入并按已宣告宽度读回。
 *
 * ## Invariants and lifecycle
 * 数据不是独立自描述指令；patch 44/45 等语义门禁由 opcode 校验处理。宣告8字节不使不同 opcode 表或布局自动兼容。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:137：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:1788：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:1712：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_LOCAL wire_layout_note.LOCAL
 * @brief wire_layout_note.LOCAL：核对 .LOCAL 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .LOCAL 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * local 记录实际含 name、uint32 stackSlot、uint64 起止指令偏移及起止行；patch>=17 另含 scopeDepth/escapeFlags。
 *
 * ## Invariants and lifecycle
 * 旧速记遗漏 name/slot/escape 字段；行号并非简单随 debug 标志就全部省略。以 write_function_local_variables/io_read_function_local_variables 的实际序列为准。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:141：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:346：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:383：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_CONSTANT wire_layout_note.CONSTANT
 * @brief wire_layout_note.CONSTANT：核对 .CONSTANT 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .CONSTANT 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 常量记录以 uint32 type 选择 payload，再写两个 uint64 尾字段；原生 helper 用首尾字段中的 helper 标识约定。
 *
 * ## Invariants and lifecycle
 * 函数/闭包载荷含 hasFunctionValue 和可递归函数，native pointer 走可序列化 helper 映射；不能把 DATA 视为任意指针或对象内存，也不能把旧 START_LINE 释为所有类型的真实源行。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:150：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:807：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:441：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_CLOSURE wire_layout_note.CLOSURE
 * @brief wire_layout_note.CLOSURE：核对 .CLOSURE 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .CLOSURE 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 子函数 closure 记录当前递归读取一棵 function 子树。
 *
 * ## Invariants and lifecycle
 * 不要与函数前部的 closure variable 捕获表混淆；后者有名称、inStack、index、valueType 与版本化 scope/escape 字段。旧 todo 仍需在完整函数布局维护时一并核实。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:157：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:1566：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:1856：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_DEBUG_INFO wire_layout_note.DEBUG_INFO
 * @brief wire_layout_note.DEBUG_INFO：核对 .DEBUG_INFO 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .DEBUG_INFO 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 调试记录按 patch>=13 读源文件/摘要，随后读 instructionsLength 及 uint64 行表；patch>=15 且非空时另读逐指令范围。
 *
 * ## Invariants and lifecycle
 * writer 的 debug options 可决定写出内容，但 loader 依据数量/版本解码。旧图的区间不是独立于 patch 的通用结构布局。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:162：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:1577：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:241：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_DECLARE wire_layout_note.DECLARE
 * @brief wire_layout_note.DECLARE：核对 .DECLARE 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .DECLARE 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 声明先消费外层类型判别式，再解码与该标签一致的子树。
 *
 * ## Invariants and lifecycle
 * 模块层与成员层枚举不能互换；当前 loader 的模块读入语句使用 EZrIoMemberDeclareType 载体而 switch 用模块标签，应按实际编号和编译器 enum 布局理解，不能由旧4字节图推导通用 ABI。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:207：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2219：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2119：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_CLASS wire_layout_note.CLASS
 * @brief wire_layout_note.CLASS：核对 .CLASS 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .CLASS 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * class 子树依次读 name、父引用数量/记录、generic 数量、member 数量/记录。
 *
 * ## Invariants and lifecycle
 * 父引用实际是 module name/md5/referenceIndex，不是旧图所写逐项内嵌 class；当前 writer 使用字符串索引引用且 generic/member 数量写零，实际成员从 prototypeData 重建。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:212：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:89：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2158：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_STRUCT wire_layout_note.STRUCT
 * @brief wire_layout_note.STRUCT：核对 .STRUCT 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .STRUCT 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * struct 子树依次读 name、父引用数量/记录、generic 数量、member 数量/记录。
 *
 * ## Invariants and lifecycle
 * 父引用不是逐项内嵌 struct；当前 writer 使用稳定 type stub，generic/member 数量写零，不能以 loader 分支代替所有成员 roundtrip 覆盖。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:222：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:121：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2174：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_INTERFACE wire_layout_note.INTERFACE
 * @brief wire_layout_note.INTERFACE：核对 .INTERFACE 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .INTERFACE 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * interface 子树由 loader 读取 name、父接口引用数量/记录、generic 数量与成员记录。
 *
 * ## Invariants and lifecycle
 * 该 reader 格式不能从历史速记推导当前 writer 的接口产出能力；当前二进制原型适配只明确 class/struct stub。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:231：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2190：定义、配套消费或失败边界证据。
 * - zr_vm_parser/src/zr_vm_parser/writer/writer.c:1633：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_ENUM_FIELD wire_layout_note.ENUM_FIELD
 * @brief wire_layout_note.ENUM_FIELD：核对 .ENUM_FIELD 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .ENUM_FIELD 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * enum field 先读 name，再按外层传入的 valueType 解码值。
 *
 * ## Invariants and lifecycle
 * 类型来自所属完整枚举；成员层 enumField 分支当前显式按 UINT64 读取，不能把两种场合混为每字段自带 TYPE。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:240：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2109：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2143：定义、配套消费或失败边界证据。
 */

/**
 * @page common3_wire_layout_note_ENUM wire_layout_note.ENUM
 * @brief wire_layout_note.ENUM：核对 .ENUM 的历史布局速记与当前实际读写边界。
 *
 * ## Summary
 * 核对 .ENUM 的历史布局速记与当前实际读写边界。
 *
 * ## Value model
 * 完整枚举先读 name、valueType、fieldsLength，再以同一个 valueType 读取各 field。
 *
 * ## Invariants and lifecycle
 * 值类型是枚举级共享标签；旧字段速记不能证明 writer 当前输出完整模块枚举或其所有可选值类型。
 *
 * ## References
 * - zr_vm_common/include/zr_vm_common/zr_io_conf.h:245：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2207：定义、配套消费或失败边界证据。
 * - zr_vm_core/src/zr_vm_core/io/io.c:2109：定义、配套消费或失败边界证据。
 */
