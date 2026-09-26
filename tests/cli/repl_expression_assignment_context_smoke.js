const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_expression_assignment_context_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景在同一 REPL 会话中依次修改局部变量、对象成员和数组元素，再查询结果与静态数值事实；防止查询继续读取旧值。
// TODO: 这里等待 exit 后立即检查已收集输出；子进程 stdio 可能尚未关闭，需用慢速或大量尾部输出的假 CLI 验证是否漏读，再决定是否等待 close。
async function main() {
    const child = spawn(cliPath, [], {
        stdio: ['pipe', 'pipe', 'pipe'],
        windowsHide: true,
    });
    let output = '';
    child.stdout.on('data', (chunk) => {
        output += chunk.toString();
    });
    child.stderr.on('data', (chunk) => {
        output += chunk.toString();
    });

    child.stdin.write('var seed = 2;\n');
    child.stdin.write('\n');
    child.stdin.write('seed = 5;\n');
    child.stdin.write('\n');
    child.stdin.write('seed + 1\n');
    child.stdin.write('\n');
    child.stdin.write(':type seed + 1\n');
    // BUG: 当前 MSVC CLI 运行此会话时对象声明报 Failed to register variable in pre-execution Semantic IR，obj 后续读写均找不到绑定，40 的结果断言失败。
    child.stdin.write('var obj = {a: 10};\n');
    child.stdin.write('\n');
    child.stdin.write('obj.a = 40;\n');
    child.stdin.write('\n');
    child.stdin.write('obj.a\n');
    child.stdin.write('\n');
    child.stdin.write('var values = [10];\n');
    child.stdin.write('\n');
    child.stdin.write('values[0] = 40;\n');
    child.stdin.write('\n');
    child.stdin.write('values[0] + 2\n');
    child.stdin.write('\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    const normalizedOutput = output.replace(/\r\n/g, '\n');
    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(normalizedOutput.includes('\n6\n'),
        `bare expression should execute against the latest persisted assignment\n${output}`);
    assert(output.includes('Numeric range: 6..6'),
        `:type should infer against the latest persisted assignment\n${output}`);
    assert(!output.includes('Numeric range: 3..3'),
        `:type should not keep using the stale declaration initializer after assignment\n${output}`);
    assert(normalizedOutput.includes('\n40\n'),
        `member assignment should persist for later REPL member reads\n${output}`);
    assert(!normalizedOutput.includes('\n10\n'),
        `member assignment should not fall back to the stale object literal value\n${output}`);
    assert(normalizedOutput.includes('\n42\n'),
        `index assignment should persist for later REPL expression execution\n${output}`);
    assert(!normalizedOutput.includes('\n12\n'),
        `index assignment should not fall back to the stale array literal value\n${output}`);
    assert(!output.includes('undefined variable') &&
        !output.includes('Unknown identifier') &&
        !output.includes('failed to infer expression type'),
        `assignment context should not break later expression inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
