const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_expression_aggregate_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景验证 REPL 将数组索引表达式作为裸表达式执行，并输出聚合元素的求值结果；避免只生成赋值指令或误报语句分号。
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

    child.stdin.write('[1 + 2][0]\n');
    child.stdin.write('\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    const normalizedOutput = output.replace(/\r\n/g, '\n');
    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(normalizedOutput.includes('\n3\n'),
        `bare aggregate-index expression should execute and print 3\n${output}`);
    assert(!output.includes("Expected ';'") && !output.includes("期望 ';'"),
        `bare aggregate expression should not be parsed as an unterminated statement\n${output}`);
    assert(!output.includes('failed to infer expression type'),
        `bare aggregate execution should not take the :type analysis path\n${output}`);
    assert(!output.includes('SET_BY_INDEX'),
        `bare aggregate execution should construct the array literal successfully\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
