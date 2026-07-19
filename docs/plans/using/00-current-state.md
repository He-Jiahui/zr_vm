# Using 00：当前状态与痛点

## 已有基线

runtime已有ownership对象、release/detach、metadata token、union与plugin guard相关能力；semantic query/dataflow也已有ownership诊断基础。完成证据见各阶段记录目录。

## 痛点

1. 旧`%using`同时承担owner构造、lexical cleanup、union guard与plugin guard，无法给出单一类型规则。
2. `%unique/%shared/%weak/%borrow/%loan`混合了类型能力、操作与runtime representation。
3. 旧计划依赖变量置null或名义wrapper模拟move，不能覆盖Place projection、branch join与ref escape。
4. using cleanup与Drop/finalizer边界不清，异常路径容易重复或漏释放。
5. plugin加载是动态I/O与版本解析问题，不应借用resource syntax。
6. 大量完成日志记录了旧表层实现，不等于目标syntax contract完成。

## 新基线

- `resource class`定义确定性资源类别。
- `Unique/Shared/Weak`是规范owner类型；`ref/ref readonly`是借用。
- `using`只绑定实现Close/Dispose protocol的值并建立lexical cleanup。
- `if let/switch`处理union；`loadPlugin/loadModule`返回显式result union。
- GC bridge只通过`Gc<T>/GcBox<T>/intoGc()`等注册contract。
