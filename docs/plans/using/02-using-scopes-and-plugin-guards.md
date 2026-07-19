# Using 02：Close Scope 与动态加载边界

## `using`唯一职责

`using`表示“绑定一个实现Close/Dispose protocol的值，并在lexical scope所有退出边执行一次Close”。它不构造owner、不匹配union、不加载plugin。

最终表层语法暂记为：

```text
UsingStatementSyntax = surfacePending
```

冻结grammar前必须比较binding declaration、expression resource、多个resource、async close与已有statement grammar，不能用旧`%using`形状作为默认答案。

## 已锁定语义

- resource expression只求值一次。
- binding lifetime覆盖body，cleanup顺序与声明/嵌套顺序确定。
- normal exit、return、throw、break、continue都进入cleanup CFG。
- body异常与Close异常按明确suppression/aggregation policy处理；不得静默覆盖原异常。
- Close是ordinary callable contract，可有sync/async类别；第一版未冻结async using时应拒绝而非阻塞等待。
- using不代替resource class的Drop。若同一类型同时有Drop与Close，owner storage执行Drop，using只执行其绑定的Close protocol，必须通过类型设计避免重复释放同一native handle。

## Plugin/module加载

```zr
let result = loadPlugin("render.vulkan");
if let PluginLoadResult.Ok(plugin) = result {
    plugin.run();
}
```

loader负责specifier/version/capability/I/O；result union负责失败；plugin handle若需要Close可以随后进入using。静态`import`仍是module-scope literal binding。

## 冻结门

先完成cleanup CFG、Close protocol metadata、异常组合与borrow escape，再选择surface grammar。所有候选语法必须doc-test且minified后仍靠分号/block无歧义。

## Cleanup Contract 实施顺序

交付物是CloseProtocol、UsingResourceBinding、cleanup CFG、exception aggregation contract和loader-result边界；最终surface syntax不在此阶段输出。

1. **C1 protocol binding**：以Canonical CallableContract识别sync Close/Dispose，记录receiver effect、mayThrow和accessibility；不按方法名临时猜测。
2. **C2 single evaluation/binding**：resource expression求值一次，binding拥有明确Place、scope和borrow region；初始化失败不执行未获得的Close。
3. **C3 cleanup CFG**：normal、return、throw、break、continue、nested scope和partial body统一连接cleanup edge，多个resource逆序关闭。
4. **C4 exception policy**：定义body异常、Close异常和多个Close异常的primary/cause/suppressed结构；同一handle不得因Drop+Close重复释放。
5. **C5 async boundary**：在async Close protocol未设计前，任何需要跨suspension的using候选稳定拒绝，不隐式阻塞。
6. **C6 surface freeze**：只在C1-C5通过后比较候选grammar，更新syntax 07 fixture与migration edits。

现有`tests/fixtures/projects/using_feature_matrix`、`using_edge_matrix`和`using_real_world_checkout`只能作为旧语义baseline。目标fixture必须新增每个退出边、初始化/关闭失败、nested order、borrow escape与minified semicolon版本。

Plugin验证另行覆盖unknown package/export、version/capability、load failure、module generation和返回handle Close；通过plugin guard旧测试不能替代cleanup contract。

## 完成记录

[Legacy plugin guard baseline](./05-migration/2026-06-18-legacy-plugin-guard-baseline.md)只证明旧loader/guard行为可作为迁移输入；不证明目标Close scope或ModuleIdentity loader已经完成。
