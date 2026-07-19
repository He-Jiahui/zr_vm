# Syntax 02 M5 reference escape, closure and suspension acceptance

对应计划：`docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md`
的 `M5 Escape、closure 与 suspension`。

## Scope

- `ref` / `ref readonly` / `scoped ref` TypeRef 与 canonical Ref projection。
- local/function/caller/heap-static escape lattice 与 conservative unknown。
- ref return、conditional bound、module/member/container store。
- lambda/local named function capture 与 writable capture last-use。
- await/generator suspension epoch。
- native call-scoped ref 默认契约与 target async/native declaration surface。

## RED evidence

专项测试最初无法编译，因为 Syntax AST 和 inferred type 没有独立 reference access，
Semantic IR 没有 escape fact/query，compiler 也没有 reference escape pre-pass。补齐基础类型后，
首轮运行暴露 scoped capture 诊断文本/range 不稳定、closure call result 错继承 callee identity，
以及 `async fn` / `native extern` 被恢复 AST 假绿。

父回归进一步暴露两个初始化/上下文问题：`ZrParser_InferredType_Init` 漏初始化
`referenceAccess`，导致 type equality 与 generic method inference 失败；`scoped.Array<T>` 被
误判为 `scoped ref`。两者均先由父层测试定位，再分别补初始化不变量和 `ref` lookahead。

MSVC 首次链接专项测试时暴露 private validator 未导出 DLL 符号；内部声明按既有 parser
测试入口加 `ZR_PARSER_API` 后关闭，不增加安装头文件 API。

## GREEN implementation

- AST/inferred type 独立保存 writable/readonly reference access；canonical adapter 生成 Ref
  node，`scoped` 只约束 region，不进入 TypeId identity。
- Semantic IR escape fact 保存 source Region/Place、origin/target range、kind 与两端 lattice；
  flow query 按 fact id 返回精确 violation，unknown source 保守拒绝。
- compiler 两个入口在 task effects/bytecode publication 前运行 source escape pre-pass。
- ref return 采用所有可能来源的最严格 bound；in/out/scoped ref、local ref、global/member/
  container store 均按目标宽度验证。
- container/member store 只提升已知 ref provenance；普通值进入 field、array、object 的
  初始化与赋值保持 value semantics，避免 false-positive escape。
- lambda 与局部命名 `fn` 共用 capture rule；escaping closure 验证 capture bound，可写
  capture 的 mutable loan 延续到 closure binding 最后一次使用。
- await 与 generator suspension 使用 binding epoch/last-use；owned observation 不携带 ref
  provenance进入 frame。
- target `async fn` 不隐藏包装返回 TypeRef；legacy `%async` 保留兼容 lowering。
- target `native extern` 的函数声明要求 `fn`；legacy `%extern` 保持兼容，`native` 仍为
  contextual identifier。native ref 默认 call-scoped，不声明长期 capture。
- 未新增 VM/AOT runtime borrow fallback。

## Verification

- GCC 11.4：九套晋级矩阵 196/196；额外 parser 75/75、compiler integration 127/127。
- Clang 14.0：同九套晋级矩阵 196/196。
- MSVC 19.44.35228 `/W4`：同九套晋级矩阵 196/196。
- 九套构成：escape/closure/suspension 12、receiver 19、loan/NLL 15、pre-Semantic-IR 6、
  Place/CFG 4、Place/out 5、syntax contract 7、type inference 118、named arguments 10。
- fresh build directories：`/home/hejiahui/zr_vm-syntax-02-m5-gcc`、
  `/home/hejiahui/zr_vm-syntax-02-m5-clang`、`build-syntax-02-m5-msvc`。

## Promotion gate

- caller/function/heap-static lattice 与 conditional shorter bound：PASS。
- in/out/scoped ref return/capture/store/suspension rejection：PASS。
- lambda 与 local named closure escape、writable capture NLL：PASS。
- await/yield 前后最后使用边界：PASS。
- native ref call-scoped default 与 target/legacy syntax boundary：PASS。
- precise origin/escape structured ranges：PASS。
- unknown escape source conservative rejection：PASS。
- no runtime borrow table/fallback：PASS。

结论：M5 晋级门全部通过，可以进入 M6 Artifact/LSP consumers。
