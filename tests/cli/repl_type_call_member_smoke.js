const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_call_member_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

// CTest 传入 CLI 可执行路径；此场景用未声明的调用目标和成员接收者查询语法事实，验证 :type 可报告调用、成员及写引用而不会执行表达式。
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

    child.stdin.write(':type pick(42)\n');
    child.stdin.write(':type seed.value\n');
    child.stdin.write(':type seed[index]\n');
    child.stdin.write(':type seed.value = 3\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Call: pick args=1'),
        `:type should print the shared call expression fact\n${output}`);
    assert(output.includes('Member: value'),
        `:type should print the shared member expression fact\n${output}`);
    assert(output.includes('Member: index'),
        `:type should print the shared computed member expression fact\n${output}`);
    assert(output.includes('Reference: member value'),
        `:type should print the shared member-access reference fact\n${output}`);
    assert(output.includes('Reference: member index'),
        `:type should print the shared computed member-access reference fact\n${output}`);
    const computedMemberReferenceCount = (output.match(/Reference: member index/g) || []).length;
    assert(computedMemberReferenceCount === 1,
        `:type should not duplicate the same computed member-access reference fact\n${output}`);
    assert(output.includes('Reference: member write value'),
        `:type should print the shared member-write reference fact\n${output}`);
    assert(!output.includes('Declared at:'),
        `unresolved member-write facts should not print a declaration location\n${output}`);
    assert(!output.includes('\n42\n') &&
        !output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not execute the call or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
