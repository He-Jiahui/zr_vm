---
related_code:
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/type.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/ownership_transfer.h
  - zr_vm_parser/src/zr_vm_parser/compiler
implementation_files:
  - zr_vm_core/src/zr_vm_core/type.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_core/src/zr_vm_core/gc.c
  - zr_vm_parser/src/zr_vm_parser/compiler
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
  - docs/plans/syntax/README.md
tests:
  - tests/core/test_type_layout.c
  - tests/core/test_ownership.c
  - tests/core/test_gc.c
  - tests/parser/test_syntax_reference_v1.c
doc_type: language-reference
---

# 类型、布局与所有权

ZR 的类型系统同时服务解释执行、AOT、GC 和 native ABI。编译器不会把所有值都降成一个
“通用对象”：值类型的布局、引用类型的 header、扫描方式和 Drop hook 都进入 `TypeId` /
`TypeLayout` identity。写泛型库或 native provider 时，必须把这些事实当作契约的一部分。

## 值分类

| 分类 | 例子 | 典型存储 | 复制/清理 |
| --- | --- | --- | --- |
| primitive | `bool`、`i32`、`u64`、`float`、`char` | inline slot | 按值复制，无 GC scan |
| string | `string` | managed string reference | 引用计数/GC 语义由 core 管理 |
| struct/union | `Vector3`、用户 `struct` | inline 或 boxed | 由字段 layout 决定 scan/copy/drop |
| class | `File`、用户 `class` | object reference | GC trace；可带 finalizer/native payload |
| array | `T[]` | managed array object | 元素按 T 的 layout 扫描 |
| callable | `fn(...) -> T` | closure/native binding | 捕获环境和 binding metadata 受 root 保护 |
| resource | `resource class`、`FileStream` | managed wrapper + native handle | 必须显式或 cleanup close |

`typeof(expr)` 读取运行时值的类型 descriptor，`typeid(T)` 产生可传给反射/FFI 的 TypeId
对象。TypeId 不等于显示名称；模块 identity、泛型实参、ownership、metadata generation
和 layout hash 可能都会影响比较结果。

## ownership qualifier

| qualifier | 目的 | 允许的典型操作 | 逃逸边界 |
| --- | --- | --- | --- |
| `own T` | 唯一拥有一个资源/对象 | move、显式 `drop`、转交构造参数 | 不能复制；离开 scope 触发 Drop |
| `share T` | 共享强引用 | clone/share、跨任务传递（需 Send/Sync） | 最后一个强引用释放时回收 |
| `weak T` | 不保持对象存活 | `wake`/upgrade 成强引用 | 不能直接解引用；升级可能得到 null |
| `ref T` | 当前调用内的可写借用 | 读写 place、写回 `out` | 不能跨越 owner 销毁、await 或线程迁移 |
| `readonly T` | 只读借用/视图 | 读取字段和调用 const 方法 | 不能写回，通常可共享 |
| `scoped T` | 词法 guard | `using` 自动关闭、pool/lock view | 必须在声明 scope 内 close |
| `in T` | 按只读值传参 | 免复制地读取值布局 | callback 返回前有效 |
| `out T` | 调用方 place 输出 | native/函数写入结果 | 必须在正常返回路径初始化 |

`share`、`degrade`、`wake`、`intoGc`、`drop` 是 ownership intrinsic。它们不是普通函数，
编译器会在 CFG 上插入 retain/release、loan end、cleanup edge 和异常路径处理。`drop x;`
之后，`x` 的 place 进入 moved 状态；再次读取会得到 use-after-move 诊断。

## Place、Value 和 Loan

语义 IR 把表达式分成三类事实：

1. **Place** 是可定位的存储位置，例如局部变量、字段或数组元素；它可以被 `ref/out` 借用。
2. **Value** 是一次求值结果；把 place 读成 value 可能触发 copy、move 或 retain。
3. **Loan** 是带 region 的借用记录，记录读/写模式、来源 place 和结束点。

```zr
fn normalize(ref v: Vector3): void {
    v = v.normalized();
}

fn consume(stream: own FileStream): void {
    using (stream) {
        stream.flush();
    }
} // pending cleanup 即使 flush 抛异常也执行
```

上例中的 `ref v` 要求调用方传入可写 place，而 `own stream` 把 close 责任转移到函数。
`using` 会生成资源 guard；`return`、`throw`、`break`、`continue`、`yield` 都先结束活动
loan，再按嵌套逆序执行 cleanup。

## TypeLayout 和 GC

每个具体类型的 `SZrTypeLayout` 至少描述 size、alignment、copy、drop、scan kind 和 layout
id。常见扫描类别为：

- `GcFree`：没有 managed 引用，不扫描；
- `GcMapped`：按字段 bitmap 扫描 managed 值；
- `GcBarriered`：写入需 dirty-card/write barrier；
- pinned/large/permanent region：由 GC domain 单独管理，不能把地址当普通 movable pointer。

native callback 在创建临时对象后若要跨越可能触发 GC 的调用，必须建立
`ZrLibTempValueRoot` 或使用 state 的正式 root。直接缓存 `SZrObject *`、字符串 buffer 或
inline span 直到下一个 safepoint 都是不安全的。

## nullable 和默认值

运行时的 null 是独立的 `ZR_VALUE_TYPE_NULL`，不是任意数值的特殊 bit pattern。函数可以
通过默认参数表达“可省略”：

```zr
fn readLineOr(defaultValue: string = ""): string {
    let line = import("zr.system.console").readLine();
    if (line == null) { return defaultValue; }
    return line;
}
```

descriptor 文档中的 `string?` 只表示“返回 string 或 null”，不能直接复制为 `T?` 类型后缀。
对 nullable value 进行成员访问前使用 `if (x != null)` 或 `x?.member`；semantic phase 会
检查 guard 是否覆盖所有控制流路径。

## 泛型实例和协议

泛型实例的 identity 包含 base type、每个实参的 canonical TypeId、ownership qualifier 和
约束满足结果。`Array<T>`、`Span<T>` 等值布局可能随 T 改变；不能用一个 `void *` 视图替代
所有实例。协议/接口关系会进入 protocol mask 和 dispatch table，调用点保存 contract hash
以便 AOT/quickening 失配时回退到 checked path。

## async、线程和所有权

`await` 是挂起点。跨越它的局部必须可安全保存在 task frame 中；活动的 `ref`/`scoped`
loan、未完成的 `out` 写入和 thread-affine lock 不能跨越 await。`async fn` 返回
`zr.task.Task<T>`；`zr.thread` 的 `Send`/`Sync` 协议再决定值能否进入 worker 或 isolated
domain。不可传递的值应在编译期拒绝，而不是运行时猜测深拷贝。

## C 侧对应关系

宿主通过 `SZrTypeValue` 传递值，通过 `SZrTypeLayout` 解释 inline storage；不要把
`SZrTypeValue.value.object` 强转成任意结构体。创建/写入 managed 值使用
`ZrLib_Value_Set*`、`ZrLib_Object_*` 和 `ZrLib_Array_*` helper；跨调用保存使用
`ZrLib_TempValueRoot` 或 core handle。反射 token 携带 metadata generation，reload 后旧
token 必须重新解析。

## 常见诊断

| 场景 | 阶段 | 典型结果 |
| --- | --- | --- |
| 把 `let` 作为左值写入 | flow/type | immutable place |
| move 后再次读取 | ownership | use-after-move |
| `ref` 借用逃出 owner scope | loan | borrowed value escapes |
| `out` 路径未写入 | flow | uninitialized out |
| resource 未 close 且不能自动清理 | ownership | missing cleanup |
| 非 Send 值进入线程 scheduler | protocol | thread transfer rejected |
| stale TypeId/token/layout | reflection | generation/layout mismatch |

更多执行细节见[语义与执行模型](semantics-implementation.md)，布局和 GC C API 见[类型、
布局与内存](../04-types-and-memory.md)。
