const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const textmate = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');

// 复用真正的 TextMate/Oniguruma 解析器，检查跨行状态；静态 JSON 正则测试无法替代这一层。
let registry;
const grammarPromise = (async () => {
    const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
    await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
    const grammarPath = path.join(__dirname, '..', 'syntaxes', 'zr.tmLanguage.json');
    registry = new textmate.Registry({
        onigLib: Promise.resolve({
            createOnigScanner: (patterns) => new oniguruma.OnigScanner(patterns),
            createOnigString: (value) => new oniguruma.OnigString(value),
        }),
        loadGrammar: async (scope) => scope === 'source.zr'
            ? textmate.parseRawGrammar(fs.readFileSync(grammarPath, 'utf8'), grammarPath)
            : null,
    });
    return registry.loadGrammar('source.zr');
})();

// 注册表只由本测试文件创建，测试结束时清理已加载的语法对象。
test.after(() => registry?.dispose());

// scope 断言统一取覆盖目标文本起点的 token，避免检查到相邻操作符的作用域。
function tokenFor(result, line, text, from = 0) {
    const offset = line.indexOf(text, from);
    assert.notEqual(offset, -1, `Missing fixture text: ${text}`);
    const token = result.tokens.find((item) => item.startIndex <= offset && item.endIndex > offset);
    assert.ok(token, `Missing token at ${offset}`);
    return token;
}

// 同一文本在不同词法上下文可有不同 scope；各场景指定其预期上下文。
function assertScope(result, line, text, scope, from = 0) {
    const token = tokenFor(result, line, text, from);
    assert.ok(token.scopes.includes(scope), `${text}: ${token.scopes.join(', ')}`);
    return token;
}

// 多字符操作符必须整体归类，避免一部分被较短的比较或赋值规则抢先消耗。
test('TextMate consumes each operator completely with its correct scope', async () => {
    const grammar = await grammarPromise;
    const cases = {
        comparison: ['==', '!=', '<=', '>=', '<', '>'],
        bitwise: ['<<', '>>', '&', '|', '^', '~'],
        assignment: ['=', '+=', '-=', '*=', '/=', '%=', '<<=', '>>='],
        logical: ['&&', '||', '!'],
        'function-arrow': ['->', '=>'],
    };
    for (const [kind, operators] of Object.entries(cases)) {
        for (const operator of operators) {
            const line = `left ${operator} right`;
            const result = grammar.tokenizeLine(line, textmate.INITIAL);
            const token = assertScope(result, line, operator, `keyword.operator.${kind}.zr`);
            assert.equal(token.startIndex, 5, operator);
            assert.equal(token.endIndex, 5 + operator.length, operator);
        }
    }
});

// 所有权内建调用和对象同名成员必须使用不同 scope，供编辑器主题及导航正确区分。
test('TextMate distinguishes ownership intrinsics from ordinary member calls', async () => {
    const grammar = await grammarPromise;
    const line = 'using (owner) { share(owner); degrade(shared); wake(weak); intoGc(owner); drop(owner); }';
    const result = grammar.tokenizeLine(line, textmate.INITIAL);
    assertScope(result, line, 'using', 'keyword.control.zr');
    for (const name of ['share', 'degrade', 'wake', 'intoGc', 'drop']) {
        assertScope(result, line, `${name}(`, 'support.function.intrinsic.zr');
        for (const prefix of ['object.', 'object?.', 'object. ']) {
            const memberLine = `${prefix}${name}(value);`;
            const member = tokenFor(grammar.tokenizeLine(memberLine, textmate.INITIAL), memberLine, name);
            assert.ok(member.scopes.includes('variable.other.member.zr'), `${memberLine}: ${member.scopes}`);
            assert.ok(!member.scopes.includes('support.function.intrinsic.zr'), memberLine);
        }
    }
});

// 插值中的字符串、注释和嵌套括号不能提前结束外层模板；随后代码应恢复普通词法状态。
test('TextMate tracks template text, escapes and nested interpolation braces', async () => {
    const grammar = await grammarPromise;
    const first = 'let message = `hello \\` \\n ${fn() {';
    const firstResult = grammar.tokenizeLine(first, textmate.INITIAL);
    assertScope(firstResult, first, 'hello', 'string.quoted.template.zr');
    assertScope(firstResult, first, '\\`', 'constant.character.escape.zr');
    assertScope(firstResult, first, '\\n', 'constant.character.escape.zr');
    assertScope(firstResult, first, '${', 'punctuation.section.interpolation.begin.zr');
    assertScope(firstResult, first, 'fn', 'storage.type.function.zr');

    const second = 'return "}"; /* } */ }()} world`;';
    const secondResult = grammar.tokenizeLine(second, firstResult.ruleStack);
    assertScope(secondResult, second, 'return', 'keyword.control.zr');
    assertScope(secondResult, second, '"}"', 'string.quoted.double.zr');
    assertScope(secondResult, second, '/* } */', 'comment.block.zr');
    assertScope(secondResult, second, 'world', 'string.quoted.template.zr');

    const after = 'let value = 42;';
    const afterResult = grammar.tokenizeLine(after, secondResult.ruleStack);
    assertScope(afterResult, after, 'let', 'storage.modifier.binding.zr');
    assert.ok(afterResult.tokens.every((token) => !token.scopes.includes('string.quoted.template.zr')));
});

// 成员访问可以跨注释和换行；解析器仍须区分其后的方法名与独立内建调用。
test('TextMate preserves ordinary member names across comments and line breaks', async () => {
    const grammar = await grammarPromise;
    for (const access of ['.', '?.']) {
        for (const trivia of ['\n', '/* comment */', ' // comment\n', ' /* multiline\ncomment */ ']) {
            const lines = `object${access}${trivia}wake(value);`.split('\n');
            let state = textmate.INITIAL;
            let result;
            for (const line of lines) {
                result = grammar.tokenizeLine(line, state);
                state = result.ruleStack;
            }
            const member = assertScope(result, lines.at(-1), 'wake', 'variable.other.member.zr');
            assert.ok(!member.scopes.includes('support.function.intrinsic.zr'));
            const next = 'wake(value);';
            assertScope(grammar.tokenizeLine(next, state), next, 'wake', 'support.function.intrinsic.zr');
        }
    }
    const variant = 'Shape.Some';
    assertScope(grammar.tokenizeLine(variant, textmate.INITIAL), variant, 'Some', 'variable.other.member.variant.zr');
});

// 编辑器修复未闭合模板后会重新词法分析，旧行状态不能继续污染后面的源码。
test('TextMate recovers the following lines when an unfinished template is repaired', async () => {
    const grammar = await grammarPromise;
    const unfinished = grammar.tokenizeLine('let label = `value ${item', textmate.INITIAL);
    const suffix = 'let after = 1;';
    const stale = grammar.tokenizeLine(suffix, unfinished.ruleStack);
    assert.ok(stale.tokens.some((token) => token.scopes.includes('string.quoted.template.zr')));

    const repaired = grammar.tokenizeLine('let label = `value ${item}`;', textmate.INITIAL);
    const retokenized = grammar.tokenizeLine(suffix, repaired.ruleStack);
    const fresh = grammar.tokenizeLine(suffix, textmate.INITIAL);
    assert.deepEqual(retokenized.tokens, fresh.tokens);
    assert.ok(retokenized.ruleStack.equals(fresh.ruleStack));
});

// 转义反斜杠层级不同的嵌套模板都应在正确位置闭合，下一行不得沿用插值 scope。
test('TextMate closes escaped nested templates without leaking into following code', async () => {
    const grammar = await grammarPromise;
    const delimiter = '\\`';
    const deeperDelimiter = '\\\\\\`';
    const expressions = [
        `${delimiter}inner${delimiter}`,
        `${delimiter}inner ${'${'}${deeperDelimiter}deep${deeperDelimiter}}${delimiter}`,
        `${delimiter}inner ${deeperDelimiter} literal backtick${delimiter}`,
    ];
    for (const literalBackslashes of [1, 2]) {
        const innerClose = '\\'.repeat(1 + literalBackslashes * 4) + '`';
        const deepClose = '\\'.repeat(3 + literalBackslashes * 8) + '`';
        expressions.push(`${delimiter}inner${innerClose}`);
        expressions.push(`${delimiter}inner ${'${'}${deeperDelimiter}deep${deepClose}}${delimiter}`);
    }
    for (const expression of expressions) {
        const line = 'let text = `outer ${' + expression + '} tail`;';
        const result = grammar.tokenizeLine(line, textmate.INITIAL);
        const tail = assertScope(result, line, 'tail', 'string.quoted.template.zr');
        assert.ok(!tail.scopes.includes('meta.interpolation.zr'), tail.scopes.join(', '));
        const after = 'let after = 1;';
        const continued = grammar.tokenizeLine(after, result.ruleStack);
        const fresh = grammar.tokenizeLine(after, textmate.INITIAL);
        assert.deepEqual(continued.tokens, fresh.tokens, line);
        assert.ok(continued.ruleStack.equals(fresh.ruleStack), line);
    }
});

// 除语法着色外，编辑器的反引号自动配对和包围也必须由语言配置声明。
test('language configuration pairs and surrounds template backticks', () => {
    const configuration = JSON.parse(fs.readFileSync(
        path.join(__dirname, '..', 'language-configuration.json'), 'utf8'));
    const isBacktickPair = (pair) => Array.isArray(pair)
        ? pair[0] === '`' && pair[1] === '`'
        : pair.open === '`' && pair.close === '`';
    assert.ok(configuration.autoClosingPairs.some(isBacktickPair));
    assert.ok(configuration.surroundingPairs.some(isBacktickPair));
});
