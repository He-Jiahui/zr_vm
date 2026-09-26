const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_expression_array_display_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景验证 REPL 裸数组表达式的显示结果来自已折叠的元素值，覆盖表达式编译、执行和格式化的整条路径。
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

    child.stdin.write('[1 + 2]\n');
    child.stdin.write('\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    const normalizedOutput = output.replace(/\r\n/g, '\n');
    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(normalizedOutput.includes('\n[3]\n'),
        `bare array expression should execute and print [3]\n${output}`);
    assert(!output.includes('Assertion') && !output.includes('assert'),
        `bare array expression display should not trip a value string assertion\n${output}`);
    assert(!output.includes('failed to infer expression type'),
        `bare array expression execution should not take the :type analysis path\n${output}`);
    assert(!output.includes('SET_BY_INDEX'),
        `bare array expression display should not fail while initializing elements\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
