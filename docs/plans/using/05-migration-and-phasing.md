# Using 05：旧表层迁移

## 迁移映射

| 旧语义角色 | 目标 |
|---|---|
| `%owned` type | `resource class` |
| `%unique` construct/type | `own T(...)` / `Unique<T>` |
| `%shared/%weak` | `Shared<T>/Weak<T>`与`share(owner)`/`degrade(shared)` |
| `%borrow/%loan` | `ref readonly/ref`与普通borrow expression |
| `%release` | `drop(owner)` |
| `%detach` | `intoGc(owner)`，必须人工确认bridge |
| `%using` owner scope | owner binding + scope Drop；不保留using构造 |
| `%using` Close scope | 迁移到最终UsingStatementSyntax，grammar冻结前requiresReview |
| `%using` union guard | `if let`/`switch` |
| `%using` plugin guard | `loadPlugin` result + pattern + 可选Close scope |

迁移按AST role执行，不能全文替换同名token。工具必须输出semantic-change warning、可定位edit和requiresReview类别，并保证二次运行幂等。

## Phasing

1. inventory旧AST role与fixture。
2. 先实现Canonical owner/ref/Place/CFG和目标negative diagnostics。
3. 开放resource/owner新surface。
4. 开放union pattern和loader API。
5. using grammar冻结后迁移Close scope。
6. formatter/LSP只输出目标表层，legacy parser只给期限诊断。
7. 删除runtime旧执行路径与artifact旧writer。

## 完成记录

[2026-06-18 legacy/plugin guard baseline](./05-migration/2026-06-18-legacy-plugin-guard-baseline.md) 是旧能力证据，不是目标迁移完成声明。

## 迁移报告与门槛

迁移输出必须为结构化记录：source range、旧AST role、目标feature id、edits、applicability、semantic warning和required follow-up。machineApplicable只用于语义等价且binding facts充分的case；owner/Close/pattern/plugin角色不明、control-flow变化、Shared detach或local dynamic import一律requiresReview/blocked。

失败边界包括无法绑定旧AST role、目标feature仍未冻结、edit重叠、目标parser/rebind失败、跨文件版本过期和二次迁移仍产生edit；这些情况不得静默跳过或标成成功。

阶段验收：

1. inventory按AST role计数，不按`%using`文本命中计数。
2. 每个映射先有正/负fixture和expected edits，再实现rewriter。
3. edit应用后使用目标parser/semantic query重新验证，不只比较文本。
4. 二次迁移零edit，formatter不重新引入旧拼写。
5. source、tests、docs、golden和`.zrp`同一切换；旧artifact reader有版本期限。
6. 仓库扫描剩余旧拼写只允许legacy/negative/history allowlist。

验证继承`tests/acceptance/2026-06-17-using-import-guard.md`的旧输入证据，但目标验收必须独立覆盖owner、Close、pattern、plugin四类输出，不能以其中一类通过宣称全部迁移。

退出条件：四类旧角色均有明确目标或requiresReview报告；应用后的项目只使用目标语义；二次运行幂等；current源码/fixture/artifact writer扫描满足allowlist。
