#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
/**
 * ## Summary
 * 让 Windows 测试共享库的入口可被按符号名查询。
 *
 * ## Constraints and lifecycle
 * 只为本测试库的声明与定义提供 DLL 导出；实际签名、线程策略和动态库句柄生命周期仍由调用链负责。
 *
 * ## References
 * tests/CMakeLists.txt:6483
 * tests/ffi/ffi_fixture.c:229
 * tests/ffi/test_ffi_module.c:668
 */
#define ZR_FFI_FIXTURE_EXPORT __declspec(dllexport)
/**
 * ## Summary
 * 给 stdcall 见证的原生声明和定义选择 Windows 调用约定。
 *
 * ## Constraints and lifecycle
 * 调用方必须选宿主支持的 ABI；该宏不改变参数生命周期，也不能把不同签名变成兼容入口。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:436
 * tests/ffi/ffi_fixture.c:753
 * tests/ffi/test_ffi_module.c:1648
 */
#define ZR_FFI_FIXTURE_STDCALL __stdcall
#else
#include <pthread.h>
#include <unistd.h>
/**
 * ## Summary
 * 在 POSIX 共享库中保留同一组测试入口声明。
 *
 * ## Constraints and lifecycle
 * 本分支宏为空，依赖实际共享目标和宿主符号可见性；符号能否查询需由动态加载消费者确认。
 *
 * ## References
 * tests/CMakeLists.txt:6483
 * tests/ffi/ffi_fixture.c:229
 * tests/ffi/test_ffi_module.c:668
 */
#define ZR_FFI_FIXTURE_EXPORT
/**
 * ## Summary
 * 让相同见证源码能在非 Windows 宿主编译。
 *
 * ## Constraints and lifecycle
 * 空宏不表示 stdcall ABI 可用；消费者的非 Windows 分支核对该签名的拒绝，受支持 ABI 由上游准入决定。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:436
 * tests/ffi/ffi_fixture.c:753
 * tests/ffi/test_ffi_module.c:1648
 */
#define ZR_FFI_FIXTURE_STDCALL
#endif

/* 与 ZR 脚本中的 Point 声明共同充当按值结构体 ABI 的布局见证。 */
/**
 * ## Summary
 * 与脚本 Point 配对，见证按值参数、按值返回及 out 写回的布局。
 *
 * ## Fields
 * x、y 是按此顺序放置的两个 i32；值由调用方或 make/fill 提供。
 *
 * ## Invariants and lifecycle
 * 调用方描述真实宿主字段宽度、顺序与对齐。按值对象没有堆句柄所有权；sum 使用时字段和须可表示。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:806
 * tests/ffi/test_ffi_module.c:810
 * tests/ffi/test_ffi_module.c:1194
 */
typedef struct ZrFfiFixturePoint {
    /** 第一个按值 i32 字段；顺序需与脚本 Point 的 x 对应。 */
    int32_t x;
    /** 第二个按值 i32 字段；sum 的字段和须可表示。 */
    int32_t y;
} ZrFfiFixturePoint;

/**
 * ## Summary
 * 为 wrapper handleId 到原生 i32 的转换提供 OFF/ON 两个哨兵。
 *
 * ## Value model
 * OFF=0、ON=1；边界使用 i32 而不是传递宿主 C enum 对象。
 *
 * ## Constraints and lifecycle
 * flip_mode 只特别识别 ON，其他 i32 值均返回 ON；这个样本不提供任意模式值的合法性校验。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:953
 * tests/ffi/test_ffi_module.c:2015
 */
typedef enum ZrFfiFixtureMode {
    /** wrapper 样本的关闭哨兵；传到 native 边界时仍是 i32。 */
    ZR_FFI_FIXTURE_MODE_OFF = 0,
    /** wrapper 样本的开启哨兵；其他值不由本 enum 校验。 */
    ZR_FFI_FIXTURE_MODE_ON = 1
} ZrFfiFixtureMode;

/**
 * ## Summary
 * 描述同步、两次派发、保存槽及 foreign-thread 见证共用的裸回调形状。
 *
 * ## Parameters
 * @param[in] value 由原生派发者提供的 double 输入；没有跨调用保存此实参的合同。
 *
 * ## Returns
 * @return 由回调提供者解释的 double；VM 回入或异常默认值取决于 trampoline 的实际策略。
 *
 * ## Constraints and lifecycle
 * 这是代码地址，不是 VM 句柄或 GC 根。调用者使代码和关联句柄在派发时有效；NULL 是否允许由各接收接口规定。线程附着和 call 生命周期限制由 trampoline 检查。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1115
 * tests/ffi/test_native_extern_contract.c:1137
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:697
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:684
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:683
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:693
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:703
 */
typedef double (*ZrFfiFixtureUnaryCallback)(double value);
/* 两个 union 形状分别覆盖整数/浮点混合和不同宽度浮点的 ABI 分类。 */
/**
 * ## Summary
 * 用整数与 double 共用存储的形状检验 union 按值 ABI 分类。
 *
 * ## Fields
 * scalar 与 integer 都起于偏移0；脚本布局描述须对应宿主。
 *
 * ## Invariants and lifecycle
 * 调用方明确当前活跃成员。read_mixed_union 读取 integer，scalar 入口读取 scalar；fixture 没有标签，也不代替上游消除活跃成员歧义。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1357
 * tests/ffi/test_native_extern_contract.c:1362
 * tests/ffi/test_native_extern_contract.c:1499
 */
typedef union ZrFfiFixtureMixedUnion {
    /** 浮点活跃成员入口读取此值；与 integer 共用存储。 */
    double scalar;
    /** 整数活跃成员入口读取此值；不得用另一活跃成员替代。 */
    int32_t integer;
} ZrFfiFixtureMixedUnion;
/**
 * ## Summary
 * 以 float/double 共用存储见证不同浮点宽度的 union 参数 ABI。
 *
 * ## Fields
 * narrow 与 wide 共享偏移0；字段宽度及整体对齐来自宿主。
 *
 * ## Invariants and lifecycle
 * 现有消费者选 wide=42.5，再按值传递；不是两个浮点字段的普通结构体，fixture 不跟踪活跃成员标签。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1402
 * tests/ffi/test_native_extern_contract.c:1407
 */
typedef union ZrFfiFixtureHeterogeneousFloatUnion {
    /** 较窄的浮点成员；当前 wide 见证不将此成员当作独立字段。 */
    float narrow;
    /** 当前42.5哨兵由此成员交付，调用者保持明确活跃成员。 */
    double wide;
} ZrFfiFixtureHeterogeneousFloatUnion;
/* 故意跨一次 native 调用保留地址，用于验证 call 生命周期回调在返回后不可再执行。 */
/**
 * ## Summary
 * 跨 native 调用保留裸 trampoline 地址，供晚到 callback 的负例消费。
 *
 * ## Invariants and lifecycle
 * C 层仍会调用保存的地址；closure 存储有效时，call 生命周期结束可在 trampoline 内拒绝 VM 回入。已释放 closure 地址不可再调用；保存槽本身不取得句柄或 GC 根。
 * TODO: 沿 store/invoke、callback finalizer 及库卸载顺序核实清空与并发策略；没有同步或自动清空合同。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:790
 * tests/ffi/ffi_fixture.c:794
 * tests/ffi/test_native_extern_contract.c:1274
 * tests/ffi/test_native_extern_contract.c:1275
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:614
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:684
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:683
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:693
 */
static ZrFfiFixtureUnaryCallback g_zr_ffi_stored_callback = NULL;

/**
 * ## Summary
 * 提供 LibraryHandle.getVersion 用例的稳定版本文本。
 *
 * ## Invariants and lifecycle
 * 返回的是借用的库内静态 NUL 文本，调用者不 free；动态库卸载后地址失效。逻辑关闭的准入缺口见 version 导出的调用契约。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:687
 * tests/ffi/test_ffi_module.c:1291
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:165
 */
static const char *kZrFfiFixtureVersion = "1.2.3-fixture";

/* 这些预声明与后面的定义是同一组外部符号；完整 ABI 与调用约束写在各声明处，定义保留测试哨兵行为。 */
/**
 * ## Summary
 * 给 LibraryHandle.getVersion 的直接原生调用提供稳定版本文本哨兵。
 *
 * ## Returns
 * @return 借用的库内静态 NUL 结尾字符串，文本用于版本断言。
 *
 * ## Constraints and lifecycle
 * 调用方保持动态库已装载；不 free 此地址，卸载后不得访问。 BUG: getVersion 在 closeRequested 后若尚有 SymbolHandle 保持物理库，只检查非空物理句柄仍可调用此导出；LibraryHandle.getVersion 的逻辑关闭检查与 getSymbol/call 不一致。此 fixture 不承诺调用者已正确检查关闭状态。
 *
 * ## References
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:134
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:165
 * tests/ffi/test_ffi_module.c:1291
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c:149
 */
ZR_FFI_FIXTURE_EXPORT const char *zr_ffi_version_string(void);
/**
 * ## Summary
 * 以有界 i32 相加同时见证动态符号、source native extern 和工程 AOT 的实际执行。
 *
 * ## Parameters
 * @param[in] lhs 原生 i32 左操作数；与 rhs 的和须可表示。
 * @param[in] rhs 原生 i32 右操作数；调用者选择相同宽度及符号的 ABI。
 *
 * ## Returns
 * @return 两个实参的 i32 和，现有 Add(20,22) 验证为42。
 *
 * ## Constraints and lifecycle
 * 输入和须可由 int32_t 表示；此小值见证不提供溢出防护。metadata/编译-only路径不等于执行本函数。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:668
 * tests/ffi/test_native_extern_contract.c:1091
 * tests/ffi/test_native_extern_contract.c:2148
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_add_i32(int32_t lhs, int32_t rhs);
/**
 * ## Summary
 * 验证 u64 参数与返回通过原生 ABI 时保留超过2^53的整数精度。
 *
 * ## Parameters
 * @param[in] value 显式 u64 整数，调用链保持整数表示。
 *
 * ## Returns
 * @return 按原生整数宽度返回 value，不经 double。
 *
 * ## Constraints and lifecycle
 * 调用方不得把该测试值先转为浮点；签名宽度与符号属性须一致。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:740
 */
ZR_FFI_FIXTURE_EXPORT uint64_t zr_ffi_identity_u64(uint64_t value);
/**
 * ## Summary
 * 验证负 i64 的原生有符号宽度和符号可按 i64 签名往返。
 *
 * ## Parameters
 * @param[in] value 显式 i64 整数，调用链保持有符号整数表示。
 *
 * ## Returns
 * @return value 的原生有符号整数表示。
 *
 * ## Constraints and lifecycle
 * 调用链须保留整数表示；当前负样本为 -(2^53-1)，仍可由 binary64 精确表示，不作为超出双精度精度的见证；u64 的 2^53+1 样本承担另一边界。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:744
 * tests/ffi/test_ffi_module.c:749
 * tests/ffi/test_ffi_module.c:750
 */
ZR_FFI_FIXTURE_EXPORT int64_t zr_ffi_identity_i64(int64_t value);
/**
 * ## Summary
 * 给 primitive symbol 用例提供 double 参数与返回值的标量 ABI 见证。
 *
 * ## Parameters
 * @param[in] lhs 宿主 double 左操作数。
 * @param[in] rhs 宿主 double 右操作数；动态签名为 f64。
 *
 * ## Returns
 * @return 按宿主 double 算术得到的乘积。
 *
 * ## Constraints and lifecycle
 * 测试的固定小值不证明所有浮点舍入、溢出或 NaN 策略；动态签名使用f64。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:672
 * tests/ffi/debug_ffi_close_probe.c:205
 * tests/ffi/debug_ffi_close_probe.c:225
 */
ZR_FFI_FIXTURE_EXPORT double zr_ffi_mul_f64(double lhs, double rhs);
/**
 * ## Summary
 * 检验字符串编码成 NUL 结尾原生指针后能被 C 层读取，结果是字节长度。
 *
 * ## Parameters
 * @param[in] text 本次同步调用借用的 NUL 结尾字节串；可为 NULL，否则须一直可读至 NUL。
 *
 * ## Returns
 * @return NUL之前的字节数，NULL为0，返回类型是宿主 size_t。
 *
 * ## Constraints and lifecycle
 * 名称不代表编码校验或字符计数；调用方保证可读至NUL。TODO: 现有签名把size_t写为u64，沿实际宿主指针宽度核对32位目标的返回ABI，入口为primitive-symbol用例。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:676
 * tests/ffi/debug_ffi_close_probe.c:209
 * tests/ffi/debug_ffi_close_probe.c:233
 */
ZR_FFI_FIXTURE_EXPORT size_t zr_ffi_strlen_utf8(const char *text);
/**
 * ## Summary
 * 返回按值 Point 以验证两个 i32 字段的返回值 ABI。
 *
 * ## Parameters
 * @param[in] x 返回 Point 的第一个原生 i32 坐标。
 * @param[in] y 返回 Point 的第二个原生 i32 坐标。
 *
 * ## Returns
 * @return 与调用方 Point 声明布局对应的按值结构体。
 *
 * ## Constraints and lifecycle
 * 调用方描述字段顺序、宽度及宿主对齐；不是堆对象或需释放的句柄。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:810
 * tests/ffi/test_ffi_module.c:1186
 */
ZR_FFI_FIXTURE_EXPORT ZrFfiFixturePoint zr_ffi_make_point(int32_t x, int32_t y);
/**
 * ## Summary
 * 消费按值 Point，把传参 ABI 的字段恢复结果交付给 roundtrip 用例。
 *
 * ## Parameters
 * @param[in] point 按宿主 Point 布局传值；两个字段的和须可表示。
 *
 * ## Returns
 * @return point.x与point.y的i32和。
 *
 * ## Constraints and lifecycle
 * 字段和须可表示；脚本布局与按值调用约定一致，调用不转移对象所有权。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:814
 * tests/ffi/test_ffi_module.c:1190
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_point(ZrFfiFixturePoint point);
/**
 * ## Summary
 * 作为结构体 out 指针见证，在本次调用内向调用方缓冲写回 Point。
 *
 * ## Parameters
 * @param[out] outPoint 本次调用借用的可写、正确对齐且至少 Point 大小的存储；NULL 表示不写回。
 * @param[in] x 写回的第一个原生 i32 坐标。
 * @param[in] y 写回的第二个原生 i32 坐标。
 *
 * ## Returns
 * 无返回值；NULL不写入。
 *
 * ## Constraints and lifecycle
 * 缓冲由调用方持有且在写回期间有效，对齐须满足Point；本函数不分配或保存地址。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1194
 */
ZR_FFI_FIXTURE_EXPORT void zr_ffi_fill_point(ZrFfiFixturePoint *outPoint, int32_t x, int32_t y);
/**
 * ## Summary
 * 通过一次原生写回来区分 ref 保留初值与 out 由调用链提供零初值的契约。
 *
 * ## Parameters
 * @param[in,out] value 本次调用借用的一格可写 int32_t；可为 NULL，非空时原值不得为 INT32_MAX。
 *
 * ## Returns
 * @return 更新后的值；NULL返回-1。
 *
 * ## Constraints and lifecycle
 * 输入不为INT32_MAX；out零初始化由上游marshal负责，本函数不识别ref/out方向。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1206
 * tests/ffi/test_native_extern_contract.c:1745
 * tests/ffi/test_native_extern_contract.c:1784
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_increment_i32(int32_t *value);
/**
 * ## Summary
 * 让显式varargs签名驱动真实C可变参数调用，并用总和验证尾参数展开。
 *
 * ## Parameters
 * @param[in] count 真实尾参数个数；不得大于实际提供的数量，非正值不消费尾参数。
 * @param[in] ... 遵守 C 默认提升及本宿主 int32_t 取值 ABI 的整数尾参数；总和须可表示。
 *
 * ## Returns
 * @return 依次取出的i32总和；count<=0当前不取尾参数并返回0。
 *
 * ## Constraints and lifecycle
 * count不得超过实际参数数目且总和可表示；C层不主动检查参数个数。负向签名用例由FFI准入阻止进入C。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1525
 * tests/ffi/test_ffi_module.c:1591
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_varargs_i32(int32_t count, ...);
/**
 * ## Summary
 * 提供平台调用约定见证，以比较Windows显式stdcall成功与其他平台拒绝该ABI。
 *
 * ## Parameters
 * @param[in] lhs 原生 i32 左操作数。
 * @param[in] rhs 原生 i32 右操作数；和须可表示，调用约定由平台签名选择。
 *
 * ## Returns
 * @return 有界i32和。
 *
 * ## Constraints and lifecycle
 * Windows导出带ZR_FFI_FIXTURE_STDCALL；其他平台宏为空。需由调用者选择受支持签名，和必须可表示。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1648
 */
ZR_FFI_FIXTURE_EXPORT int32_t ZR_FFI_FIXTURE_STDCALL zr_ffi_stdcall_add_i32(int32_t lhs, int32_t rhs);
/**
 * ## Summary
 * 验证buffer handle或source extern裸指针降级后，C层可在调用方存储中写回确定字节序列。
 *
 * ## Parameters
 * @param[out] buffer 借用至少 length 个可写字节；可为 NULL，不保存或释放地址。
 * @param[in] length 宿主 size_t 的容量约束，调用方保证真实存储足够。
 * @param[in] seed u8 序列起始值；后续字节按 u8 循环。
 *
 * ## Returns
 * @return NULL返回-1；非NULL返回窄化到int32_t的length。
 *
 * ## Constraints and lifecycle
 * 不保存、释放或检查缓冲真实容量；序列按u8循环。TODO: 核对length>INT32_MAX时返回窄化与宿主size_t/u64签名边界，入口为buffer和source extern用例。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:931
 * tests/ffi/test_ffi_module.c:999
 * tests/ffi/test_ffi_module.c:1898
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_fill_bytes(uint8_t *buffer, size_t length, uint8_t seed);
/**
 * ## Summary
 * 在本次原生调用的同一线程同步回调，用于验证callback参数、异常默认值和VM回入策略。
 *
 * ## Parameters
 * @param[in] value 传入回调的 double 输入。
 * @param[in] callback 同步借用的 double(double) 入口；可为 NULL，代码及句柄在调用期间有效。
 *
 * ## Returns
 * @return NULL为-1；否则回调结果加0.5。
 *
 * ## Constraints and lifecycle
 * 回调代码和句柄在执行期间有效；是否允许VM回入、错误如何记录由trampoline和调用方策略决定。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1115
 * tests/ffi/test_native_extern_contract.c:1137
 * tests/ffi/test_native_extern_contract.c:1181
 */
ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback(double value, ZrFfiFixtureUnaryCallback callback);
/**
 * ## Summary
 * 同一次native调用中两次回调，让嵌套Apply场景验证外层callback策略恢复。
 *
 * ## Parameters
 * @param[in] value 第一次回调输入；第二次回调使用 value+1。
 * @param[in] callback 两次同步调用期间借用的 double(double) 入口；可为 NULL。
 *
 * ## Returns
 * @return 两次回调结果之和；NULL为-1。
 *
 * ## Constraints and lifecycle
 * 两次调用期间保持trampoline和VM句柄有效；本函数不延长回调生命周期或捕获VM异常。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1311
 * tests/ffi/test_native_extern_contract.c:1316
 */
ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback_twice(double value, ZrFfiFixtureUnaryCallback callback);
/**
 * ## Summary
 * 由新的 OS 线程调用借用的 callback，为未附着线程进入 VM 的策略提供负例入口。
 *
 * ## Parameters
 * @param[in] value 写入 C 调用线程栈上局部 data，供 OS 子线程读取的 double 输入。
 * @param[in] callback 写入同一局部 data 的借用入口；可为 NULL。传给子线程的 &data 仅在成功等待完成后才可结束生存期。
 *
 * ## Returns
 * @return 创建失败为-998；NULL callback不写结果时保留-999；其余返回trampoline交付的double结果。
 *
 * ## Constraints and lifecycle
 * 成功等待后才可离开 C 局部 data 的作用域；VM是否拒绝发生在trampoline内部。TODO: 核对未检查的WaitForSingleObject/pthread_join失败，沿等待返回值与data栈寿命证明线程已完成。 当前用例只观察 Function_Execute 为 FALSE，没有核对 callback thread 错误类别或消息。TODO: 按用例现有诊断 TODO 排除其他执行失败满足负例判据。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:1720
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:697
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:701
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:705
 * tests/ffi/test_ffi_module.c:1748
 */
ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback_foreign_thread(double value,
                                                                   ZrFfiFixtureUnaryCallback callback);
/**
 * ## Summary
 * 跨一次native调用存储裸trampoline地址，制造call-lifetime晚到回调的负向路径。
 *
 * ## Parameters
 * @param[in] callback 借用 trampoline 地址，NULL 清空库内全局槽；不取得句柄或 GC 根。
 *
 * ## Returns
 * 无返回值；覆盖测试库内全局槽。
 *
 * ## Constraints and lifecycle
 * 不取得句柄所有权或GC根；调用方维持trampoline存储仍存在。TODO: 核对全局槽清空、并发与句柄释放/库卸载顺序，入口为store/invoke用例及callback finalizer。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1260
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:614
 */
ZR_FFI_FIXTURE_EXPORT void zr_ffi_store_callback(ZrFfiFixtureUnaryCallback callback);
/**
 * ## Summary
 * 实际从C层调用已存地址，检验call-lifetime结束后trampoline阻止VM回入。
 *
 * ## Parameters
 * @param[in] value 交给保存的 trampoline 的 double 输入；其代码存储必须仍有效。
 *
 * ## Returns
 * @return 槽为NULL时-1；否则为trampoline返回值。
 *
 * ## Constraints and lifecycle
 * C层确实执行地址，拒绝不是发生在C调用前；句柄仍在时trampoline可拒绝进入VM。已释放closure地址不能安全调用；维护所有权与并发限制见store TODO。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1265
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:683
 */
ZR_FFI_FIXTURE_EXPORT double zr_ffi_invoke_stored_callback(double value);
/**
 * ## Summary
 * 以integer成员返回值验证整数/浮点混合union按值ABI分类。
 *
 * ## Parameters
 * @param[in] value 按宿主 MixedUnion 布局传值，调用方选择 integer 为活跃成员。
 *
 * ## Returns
 * @return 当前integer字段。
 *
 * ## Constraints and lifecycle
 * 字段重叠于偏移0，不能把另一个活跃成员当成此例的等价输入；ABI布局描述须对应宿主。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1362
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_read_mixed_union(ZrFfiFixtureMixedUnion value);
/**
 * ## Summary
 * 通过scalar的42.5哨兵检查union首成员没有被覆盖；歧义活跃成员应在native准入阶段拒绝。
 *
 * ## Parameters
 * @param[in] value 按宿主 MixedUnion 布局传值，调用方明确选择 scalar 为活跃成员。
 *
 * ## Returns
 * @return scalar等于42.5返回42，否则-1。
 *
 * ## Constraints and lifecycle
 * 本函数不判断活跃成员状态；负向样本的拒绝由上游承担。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1451
 * tests/ffi/test_native_extern_contract.c:1499
 * tests/ffi/test_native_extern_contract.c:1510
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_read_mixed_union_scalar(ZrFfiFixtureMixedUnion value);
/**
 * ## Summary
 * 以wide哨兵验证不同宽度浮点union的按值ABI及整数分类路径。
 *
 * ## Parameters
 * @param[in] value 按宿主 union 布局传值，调用方明确选择 wide 为活跃成员。
 *
 * ## Returns
 * @return wide等于42.5返回42，否则-1。
 *
 * ## Constraints and lifecycle
 * narrow/wide共用存储；仅按当前活跃wide解释，不能以普通float结构体ABI替代union。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1407
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_read_heterogeneous_float_union(
        ZrFfiFixtureHeterogeneousFloatUnion value);
/**
 * ## Summary
 * 用两个哨兵模式检验source wrapper handleId解包为原生i32参数的调用链。
 *
 * ## Parameters
 * @param[in] modeValue wrapper 解包后传递的 i32 值，不是宿主 C enum 对象。
 *
 * ## Returns
 * @return 精确ON=1返回OFF=0，其他值都返回ON=1。
 *
 * ## Constraints and lifecycle
 * 这个见证不验证或拒绝任意枚举值；wrapper解包政策由上游决定。
 *
 * ## References
 * tests/ffi/test_ffi_module.c:2015
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_flip_mode(int32_t modeValue);
/**
 * ## Summary
 * 读取同进程文件描述符当前位置，为FileStream包装参数传到native extern提供见证。
 *
 * ## Parameters
 * @param[in] fd 同进程有效宿主文件描述符；Windows 须属于实际使用的 CRT 命名空间。
 *
 * ## Returns
 * @return 宿主seek当前位置窄化为int32_t；宿主错误值仍由调用方错误策略解释。
 *
 * ## Constraints and lifecycle
 * 不取得或关闭fd；现有小偏移0/3见证不覆盖大文件。TODO: 核对_lseeki64/lseek偏移窄化及跨CRT fd传递，入口为FS真实执行样本。 bytecode shape probe仅编译/检查call lowering，不作为脚本执行见证。
 *
 * ## References
 * tests/system/test_system_fs_module.c:947
 * tests/system/test_system_fs_module.c:967
 * tests/system/test_system_fs_module.c:972
 * tests/system/test_system_fs_module.c:1120
 * tests/system/test_system_fs_module.c:1127
 * tests/system/test_system_fs_module.c:1241
 * tests/system/test_system_fs_module.c:1261
 * tests/system/test_system_fs_module.c:1266
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_tell_fd(int32_t fd);
/**
 * ## Summary
 * 设置库侧errno并返回失败哨兵，让native extern用例检验errno错误策略。
 *
 * ## Parameters
 * @param[in] code 写入当前线程、当前 CRT errno 的值。
 *
 * ## Returns
 * @return 固定-1；不直接抛VM异常。
 *
 * ## Constraints and lifecycle
 * 消费errno须同线程、同CRT；Windows静态CRT跨DLL用例会跳过，因此不能推断宿主必然读到此errno。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1659
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_set_errno_i32(int32_t code);
/**
 * ## Summary
 * 把给定状态码交给returnCode策略，以正负路径见证错误解释及callee清理链。
 *
 * ## Parameters
 * @param[in] code 由调用方的 returnCode 策略解释的状态；当前成功哨兵为 0。
 *
 * ## Returns
 * @return 原样返回code。
 *
 * ## Constraints and lifecycle
 * fixture不判断成功或设置VM错误；returnCode策略由native extern调用方解释。
 *
 * ## References
 * tests/ffi/test_native_extern_contract.c:1689
 * tests/ffi/test_native_extern_contract.c:1697
 */
ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_return_status_i32(int32_t code);

ZR_FFI_FIXTURE_EXPORT const char *zr_ffi_version_string(void) {
    return kZrFfiFixtureVersion;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_add_i32(int32_t lhs, int32_t rhs) {
    return lhs + rhs;
}

ZR_FFI_FIXTURE_EXPORT uint64_t zr_ffi_identity_u64(uint64_t value) {
    return value;
}

ZR_FFI_FIXTURE_EXPORT int64_t zr_ffi_identity_i64(int64_t value) {
    return value;
}

ZR_FFI_FIXTURE_EXPORT double zr_ffi_mul_f64(double lhs, double rhs) {
    return lhs * rhs;
}

ZR_FFI_FIXTURE_EXPORT size_t zr_ffi_strlen_utf8(const char *text) {
    return text != NULL ? strlen(text) : 0u;
}

ZR_FFI_FIXTURE_EXPORT ZrFfiFixturePoint zr_ffi_make_point(int32_t x, int32_t y) {
    ZrFfiFixturePoint point;
    point.x = x;
    point.y = y;
    return point;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_point(ZrFfiFixturePoint point) {
    return point.x + point.y;
}

ZR_FFI_FIXTURE_EXPORT void zr_ffi_fill_point(ZrFfiFixturePoint *outPoint, int32_t x, int32_t y) {
    if (outPoint == NULL) {
        return;
    }
    outPoint->x = x;
    outPoint->y = y;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_increment_i32(int32_t *value) {
    if (value == NULL) {
        return -1;
    }
    *value += 1;
    return *value;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_sum_varargs_i32(int32_t count, ...) {
    va_list arguments;
    int32_t total = 0;
    int32_t index = 0;

/* count 与真实尾参数 ABI 由调用者保证；这段消费范围用于检验展开，不提供动态数量校验或整数溢出保护。 */
    va_start(arguments, count);
    for (index = 0; index < count; index++) {
        total += va_arg(arguments, int32_t);
    }
    va_end(arguments);

    return total;
}

ZR_FFI_FIXTURE_EXPORT int32_t ZR_FFI_FIXTURE_STDCALL zr_ffi_stdcall_add_i32(int32_t lhs, int32_t rhs) {
    return lhs + rhs;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_fill_bytes(uint8_t *buffer, size_t length, uint8_t seed) {
    size_t index;

    if (buffer == NULL) {
        return -1;
    }

    for (index = 0; index < length; index++) {
        buffer[index] = (uint8_t)(seed + (uint8_t)index);
    }

    return (int32_t)length;
}

ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback(double value, ZrFfiFixtureUnaryCallback callback) {
    if (callback == NULL) {
        return -1.0;
    }
    return callback(value) + 0.5;
}

ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback_twice(double value, ZrFfiFixtureUnaryCallback callback) {
    double first;
    double second;

    if (callback == NULL) {
        return -1.0;
    }
    first = callback(value);
    second = callback(value + 1.0);
    return first + second;
}

ZR_FFI_FIXTURE_EXPORT void zr_ffi_store_callback(ZrFfiFixtureUnaryCallback callback) {
    g_zr_ffi_stored_callback = callback;
}

ZR_FFI_FIXTURE_EXPORT double zr_ffi_invoke_stored_callback(double value) {
    return g_zr_ffi_stored_callback != NULL
            ? g_zr_ffi_stored_callback(value)
            : -1.0;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_read_mixed_union(ZrFfiFixtureMixedUnion value) {
    return value.integer;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_read_mixed_union_scalar(ZrFfiFixtureMixedUnion value) {
    return value.scalar == 42.5 ? 42 : -1;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_read_heterogeneous_float_union(
        ZrFfiFixtureHeterogeneousFloatUnion value) {
    return value.wide == 42.5 ? 42 : -1;
}

#if defined(_WIN32)
/**
 * ## Summary
 * 把一次 foreign-thread 调用的输入和结果借给新 OS 线程。
 *
 * ## Fields
 * callback 是借用的裸入口，value 是发起者的输入副本，result 是线程完成后交付给发起者的值。
 *
 * ## Invariants and lifecycle
 * 实例在 C 发起线程的局部栈上，子线程不释放它。创建前完成初始化，成功等待线程完成后才读取 result 并结束作用域。
 * TODO: 当前等待结果未检查，沿平台 wait/join 返回值核实 data 和 callback 的实际有效期。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:921
 * tests/ffi/ffi_fixture.c:932
 * tests/ffi/ffi_fixture.c:936
 * tests/ffi/ffi_fixture.c:950
 */
typedef struct ZrFfiFixtureThreadData {
    /** 创建前填入的借用入口；代码/句柄有效到子线程完成。 */
    ZrFfiFixtureUnaryCallback callback;
    /** 子线程读取的输入副本，创建前初始化。 */
    double value;
    /** 先置-999哨兵；成功等待线程完成后父线程才读取。 */
    double result;
} ZrFfiFixtureThreadData;

/**
 * ## Summary
 * 由 CreateThread 派发，制造非创建 VM 的宿主线程回调路径。
 *
 * ## Parameters
 * @param[in,out] argument 借用的 ThreadData 地址；callback/value 创建前已初始化，result 仅在线程完成后供父线程读取。NULL 或 NULL callback 不派发。
 *
 * ## Returns
 * @return 固定0作为 Windows 线程退出值；callback 的 double 结果写入 data->result，不通过线程退出值交付。
 *
 * ## Constraints and lifecycle
 * 不附着 VM、不保活 callback，也不释放 data。VM 是否拒绝此线程由 trampoline 策略决定；父线程须成功等待完成，才能读取结果或结束局部对象寿命。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:932
 * tests/ffi/ffi_fixture.c:936
 * tests/ffi/test_ffi_module.c:1720
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:697
 */
static DWORD WINAPI zr_ffi_fixture_thread_proc(LPVOID argument) {
    ZrFfiFixtureThreadData *data = (ZrFfiFixtureThreadData *)argument;
    if (data != NULL && data->callback != NULL) {
        data->result = data->callback(data->value);
    }
    return 0;
}
#else
/**
 * ## Summary
 * 把一次 foreign-thread 调用的输入和结果借给新 OS 线程。
 *
 * ## Fields
 * callback 是借用的裸入口，value 是发起者的输入副本，result 是线程完成后交付给发起者的值。
 *
 * ## Invariants and lifecycle
 * 实例在 C 发起线程的局部栈上，子线程不释放它。创建前完成初始化，成功等待线程完成后才读取 result 并结束作用域。
 * TODO: 当前等待结果未检查，沿平台 wait/join 返回值核实 data 和 callback 的实际有效期。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:921
 * tests/ffi/ffi_fixture.c:943
 * tests/ffi/ffi_fixture.c:946
 * tests/ffi/ffi_fixture.c:950
 */
typedef struct ZrFfiFixtureThreadData {
    /** 创建前填入的借用入口；代码/句柄有效到子线程完成。 */
    ZrFfiFixtureUnaryCallback callback;
    /** 子线程读取的输入副本，创建前初始化。 */
    double value;
    /** 先置-999哨兵；成功等待线程完成后父线程才读取。 */
    double result;
} ZrFfiFixtureThreadData;

/**
 * ## Summary
 * 由 pthread_create 派发，制造非创建 VM 的宿主线程回调路径。
 *
 * ## Parameters
 * @param[in,out] argument 借用的 ThreadData 地址；callback/value 创建前已初始化，result 仅在线程完成后供父线程读取。NULL 或 NULL callback 不派发。
 *
 * ## Returns
 * @return 固定NULL作为 POSIX线程退出值；callback 的 double 结果写入 data->result，不通过线程退出值交付。
 *
 * ## Constraints and lifecycle
 * 不附着 VM、不保活 callback，也不释放 data。VM 是否拒绝此线程由 trampoline 策略决定；父线程须成功等待完成，才能读取结果或结束局部对象寿命。
 *
 * ## References
 * tests/ffi/ffi_fixture.c:943
 * tests/ffi/ffi_fixture.c:946
 * tests/ffi/test_ffi_module.c:1720
 * zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c:703
 */
static void *zr_ffi_fixture_thread_proc(void *argument) {
    ZrFfiFixtureThreadData *data = (ZrFfiFixtureThreadData *)argument;
    if (data != NULL && data->callback != NULL) {
        data->result = data->callback(data->value);
    }
    return NULL;
}
#endif

ZR_FFI_FIXTURE_EXPORT double zr_ffi_apply_callback_foreign_thread(double value, ZrFfiFixtureUnaryCallback callback) {
    ZrFfiFixtureThreadData data;

/* 在创建 OS 线程前发布全部借用输入和默认结果；成功等待后才可读取结果并结束 C 局部 data 的作用域。 */
    data.callback = callback;
    data.value = value;
    data.result = -999.0;

#if defined(_WIN32)
    {
/* TODO: WaitForSingleObject 的返回未检查；等待成功才可读取结果并结束借用的 &data 寿命；按等待返回状态处理并关闭句柄，关闭句柄本身不证明线程结束。 */
        HANDLE threadHandle = CreateThread(NULL, 0, zr_ffi_fixture_thread_proc, &data, 0, NULL);
        if (threadHandle == NULL) {
            return -998.0;
        }
        WaitForSingleObject(threadHandle, INFINITE);
        CloseHandle(threadHandle);
    }
#else
    {
        pthread_t threadId;
/* TODO: pthread_join 的返回未检查；沿 join 结果证明线程已完成，不能把函数调用发生当作借用栈对象已可释放的证明。 */
        if (pthread_create(&threadId, NULL, zr_ffi_fixture_thread_proc, &data) != 0) {
            return -998.0;
        }
        pthread_join(threadId, NULL);
    }
#endif

    return data.result;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_flip_mode(int32_t modeValue) {
    return modeValue == (int32_t)ZR_FFI_FIXTURE_MODE_ON
                   ? (int32_t)ZR_FFI_FIXTURE_MODE_OFF
                   : (int32_t)ZR_FFI_FIXTURE_MODE_ON;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_tell_fd(int32_t fd) {
#if defined(_WIN32)
    return (int32_t)_lseeki64((int)fd, 0, SEEK_CUR);
#else
    return (int32_t)lseek((int)fd, 0, SEEK_CUR);
#endif
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_set_errno_i32(int32_t code) {
    errno = code;
    return -1;
}

ZR_FFI_FIXTURE_EXPORT int32_t zr_ffi_return_status_i32(int32_t code) {
    return code;
}
