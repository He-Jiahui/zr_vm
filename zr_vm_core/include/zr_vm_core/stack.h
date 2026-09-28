//
// Created by HeJiahui on 2025/6/18.
//

#ifndef ZR_VM_CORE_STACK_H
#define ZR_VM_CORE_STACK_H
#include "zr_vm_core/conf.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/value.h"

/**
 * @file stack.h
 * @brief VM 值栈的槽访问、容量管理和 frame byte place 接口。
 * @note 容量以 SZrTypeValueOnStack 槽计；扩容可能搬移整段数组，跨搬移保留的位置须保存为 offset 后重新加载。
 */
/**
 * @brief VM 栈上的值槽，同时携带待关闭值链的紧凑链接。
 * @note value 是运行时值与 GC 追踪所见的表示；栈搬移时整个槽随分配一起移动。
 */
struct ZR_STRUCT_ALIGN SZrTypeValueOnStack {
    /** tagged value 内容；读写应经 stack/value helper 保持所有权与 GC 规则。 */
    SZrTypeValue value;
    /** 到前一登记待关闭槽的反向槽距；零也用于长跨度链的桥接槽。 */
    TZrUInt32 toBeClosedValueOffset;
};

typedef struct SZrTypeValueOnStack SZrTypeValueOnStack;
typedef struct SZrTypeLayout SZrTypeLayout;

/** @brief 当前栈分配中的值槽指针；扩容搬移栈后必须重新取得。 */
typedef SZrTypeValueOnStack *TZrStackValuePointer;

/**
 * @brief 表示一段带布局信息的 frame place。
 * @note 栈内 place 的 address 是当前 stackBase 下的借用地址，栈扩容后失效；byteOffset 是相对 stackBase 的字节偏移。
 *       函数层解析别名时也可能把非栈地址放入 address，并用负 byteOffset 表示没有栈偏移。
 */
typedef struct SZrStackFramePlace {
    /** 当前可直接访问的位置地址；栈内地址不可跨栈扩容保存。 */
    TZrPtr address;
    /** 栈内位置相对 stackBase 的字节偏移；外部别名可使用负哨兵。 */
    TZrMemoryOffset byteOffset;
    /** place 声明覆盖的字节数。 */
    TZrUInt32 byteSize;
    /** place 声明的字节对齐；栈 helper 将零规范化为 1。 */
    TZrUInt32 byteAlign;
} SZrStackFramePlace;

/** 栈位置的临时双态表示；常态存指针，重分配时可暂存偏移，须按当前阶段读取对应成员。 */
union TZrStackPtr {
    TZrStackValuePointer valuePointer;
    TZrMemoryOffset reusableValueOffset;
};

/** 对外使用的栈指针/重定位偏移双态别名。 */
typedef union TZrStackPtr TZrStackPointer;

/**
 * @brief 检查当前调用帧从 functionBase 到 stackTop 的已用槽数是否足够。
 * @note 此检查不会扩栈；当前仓内调用均使用名为 state 的变量。TODO: 宏形参是 STATE，但 ZR_CHECK 首参引用自由标识符 state，需确认并统一该宏对实参的使用。
 */
#define ZR_STACK_CHECK_CALL_INFO_STACK_COUNT(STATE, COUNT)                                                             \
    ZR_CHECK(state,                                                                                                    \
             (TZrMemoryOffset) (COUNT) <=                                                                              \
                     ((STATE)->stackTop.valuePointer - (STATE)->callInfoList->functionBase.valuePointer),              \
             "not enough elements in the stack")

/**
 * @brief 为 state 分配 stackLength 个栈槽，并返回数组尾后一地址。
 * @pre state 与其 global 有效，stack 非空且 stackLength 大于零；调用方记录的长度须与分配长度一致。
 * @note BUG: 原生分配失败后仍计算尾地址；state_stack_init 忽略失败并继续访问空栈，启动 OOM 会触发未定义行为。
 */
ZR_CORE_API TZrPtr ZrCore_Stack_Construct(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength);

/** @brief 释放 Construct 分配的栈槽数组。
 * @pre state、stack 指向同一次成功分配的状态与栈指针，stackLength 与原分配长度一致。
 */
ZR_CORE_API void ZrCore_Stack_Deconstruct(struct SZrState *state, TZrStackPointer *stack, TZrSize stackLength);

/**
 * @brief 按当前调用帧的相对索引解析栈槽地址。
 * @pre state 有当前 callInfo；正偏移从 functionBase 起算，负偏移从 stackTop 起算。
 * @return 索引通过当前帧边界检查时返回栈槽；零及受保留区限制的负偏移会触发运行时检查。
 */
ZR_CORE_API TZrStackValuePointer ZrCore_Stack_GetAddressFromOffset(struct SZrState *state, TZrMemoryOffset offset);

/** @brief 将逻辑栈容量扩至至少 requiredSize 个槽。
 * @pre requiredSize 不小于当前逻辑容量，且不超过 ZR_VM_MAX_STACK（错误栈容量除外）。
 *      容量须覆盖所有活动栈槽及调用方所需空间。
 * @return 扩容成功返回 true；失败返回 false，canThrowError 为 true 时改为抛出内存错误。
 * @note 可能搬移整个槽数组；扩容前保存的裸栈指针随后失效，额外尾部缓冲由实现另行保留。
 */
ZR_CORE_API TZrBool ZrCore_Stack_GrowTo(struct SZrState *state, TZrSize requiredSize, TZrBool canThrowError);

/** @brief 至少为 stackTop 后的 space 个槽预留容量，并保留现有逻辑栈长度。
 * @pre state 的 stackTop 与 stackBase 属于同一有效栈数组，最终容量不超过 ZR_VM_MAX_STACK（错误栈容量除外）。
 * @return 扩容成功返回 true；失败返回 false，canThrowError 决定是否抛出内存错误。
 * @note 可能搬移栈数组，旧槽指针须通过保存的偏移重新加载。
 */
ZR_CORE_API TZrBool ZrCore_Stack_Grow(struct SZrState *state, TZrSize space, TZrBool canThrowError);

/** @brief 检查 stackTop 后是否有 space 个可用槽，不足时尝试扩栈。
 * @pre state/global/callInfo 有效；space 大于零且计算所得容量不超过 ZR_VM_MAX_STACK（错误栈容量除外）。
 * @return 可用或扩容成功时为 true；分配失败时解锁、记录可选错误信息并返回 false，不抛内存异常。
 */
ZR_CORE_API TZrBool ZrCore_Stack_CheckFullAndGrow(struct SZrState *state, TZrSize space, TZrNativeString errorMessage);


/** @brief 返回 [stackBase, stackTop) 中已使用的槽数，不是字节数。
 * @pre 两个指针位于同一栈数组，且 stackTop 不早于 stackBase。
 */
ZR_FORCE_INLINE TZrSize ZrCore_Stack_UsedSize(TZrStackPointer *stackBase, TZrStackPointer *stackTop) {
    ZR_ASSERT(stackTop->valuePointer >= stackBase->valuePointer);
    return (TZrSize) (stackTop->valuePointer - stackBase->valuePointer);
}

/** @brief 取得值槽中 tagged value 的地址，不记录 profile helper 计数。
 * @pre valueOnStack 指向当前有效的栈槽。
 */
static ZR_FORCE_INLINE SZrTypeValue *ZrCore_Stack_GetValueNoProfile(SZrTypeValueOnStack *valueOnStack) {
    return &valueOnStack->value;
}

/** @brief 取得值槽中的 tagged value，并记录一次 stack-get profile helper 计数。
 * @pre valueOnStack 指向当前有效的栈槽。
 */
static ZR_FORCE_INLINE SZrTypeValue *ZrCore_Stack_GetValue(SZrTypeValueOnStack *valueOnStack) {
    ZrCore_Profile_RecordHelperCurrent(ZR_PROFILE_HELPER_STACK_GET_VALUE);
    return ZrCore_Stack_GetValueNoProfile(valueOnStack);
}

/** @brief 用一个 GC 管理对象初始化目标栈槽，先按值 API 规则清理旧内容。
 * @pre destination 属于当前有效栈；object 是存活且可作为 tagged value 保存的 raw object。
 */
ZR_CORE_API void ZrCore_Stack_SetRawObjectValue(struct SZrState *state, SZrTypeValueOnStack *destination,
                                          SZrRawObject *object);

/** @brief 通过 value 层复制规则覆盖目标槽中的 tagged value。
 * @pre destination 是可写栈槽，source 指向有效的 tagged value。
 */
ZR_CORE_API void ZrCore_Stack_CopyValue(struct SZrState *state,
                                        SZrTypeValueOnStack *destination,
                                        const SZrTypeValue *source);

/** @brief 将栈槽指针编码为相对当前 stackBase 的字节偏移。
 * @pre stackPointer 属于 state 当前栈分配；保存结果可跨栈搬移，原指针不可跨搬移保留。
 */
ZR_CORE_API TZrMemoryOffset ZrCore_Stack_SavePointerAsOffset(struct SZrState *state, TZrStackValuePointer stackPointer);

/** @brief 用当前 stackBase 将已保存的栈槽字节偏移还原为指针。
 * @pre offset 来自同一 state 的有效栈槽指针，且当前栈分配仍覆盖该偏移。
 */
ZR_CORE_API TZrStackValuePointer ZrCore_Stack_LoadOffsetToPointer(struct SZrState *state, TZrMemoryOffset offset);

/** @brief 将栈内原始字节地址编码为相对 stackBase 的字节偏移。
 * @pre stackAddress 属于 state 当前栈分配；本函数只编码，不验证范围。
 */
ZR_CORE_API TZrMemoryOffset ZrCore_Stack_SaveByteAddressAsOffset(struct SZrState *state, TZrPtr stackAddress);

/** @brief 将非负字节偏移加到当前 stackBase 上。
 * @pre 调用方保证 offset 指向当前栈分配中的有效字节范围；本函数只检查非负性。
 */
ZR_CORE_API TZrPtr ZrCore_Stack_LoadByteOffsetToAddress(struct SZrState *state, TZrMemoryOffset offset);

/** @brief 把 frameBase 加上相对字节偏移解析为经过边界检查的栈内 place。
 * @pre frameBase 属于 state 当前栈分配，outPlace 可写。
 * @return 偏移未按规范化对齐、范围越界或参数无效时返回 false；byteAlign 为零按 1 处理。
 * @note 成功 place 的 address 会在栈搬移后失效，byteOffset 仍可用于新 stackBase 下重新解析。
 */
ZR_CORE_API TZrBool ZrCore_Stack_MakeFramePlace(struct SZrState *state,
                                                TZrStackValuePointer frameBase,
                                                TZrUInt32 frameByteOffset,
                                                TZrUInt32 byteSize,
                                                TZrUInt32 byteAlign,
                                                SZrStackFramePlace *outPlace);

/** @brief 按布局语义在两个 stackBase 相对字节偏移间复制 inline 值。
 * @pre layout 有效；两个偏移及 layout->byteSize 覆盖当前逻辑栈内的完整字节范围。
 * @return 范围无效或 TypeLayout 拒绝复制时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_Stack_CopyInline(struct SZrState *state,
                                            const SZrTypeLayout *layout,
                                            TZrMemoryOffset destinationOffset,
                                            TZrMemoryOffset sourceOffset);

/** @brief 在两个仍对应当前栈分配的 place 间按 TypeLayout 复制 inline 值。
 * @pre source 和 destination 是 stackBase 相对且 address 与 byteOffset 一致的栈内 place。
 * @return place 未覆盖完整布局或 TypeLayout 拒绝复制时返回 false。
 */
ZR_CORE_API TZrBool ZrCore_Stack_CopyInlinePlace(struct SZrState *state,
                                                 const SZrTypeLayout *layout,
                                                 const SZrStackFramePlace *destination,
                                                 const SZrStackFramePlace *source);

#endif // ZR_VM_CORE_STACK_H
