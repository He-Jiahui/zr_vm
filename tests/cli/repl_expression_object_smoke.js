const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_expression_object_smoke.js <zr_vm_cli>');
    process.exit(1);
}

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

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

    child.stdin.write('{a: 1 + 2}.a\n');
    child.stdin.write('\n');
    child.stdin.write('{[1 + 2]: 4}[3]\n');
    child.stdin.write('\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    const normalizedOutput = output.replace(/\r\n/g, '\n');
    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(normalizedOutput.includes('\n3\n'),
        `bare object member expression should execute and print 3\n${output}`);
    assert(normalizedOutput.includes('\n4\n'),
        `bare computed-key object expression should execute and print 4\n${output}`);
    assert(!output.includes("Expected ';'") && !output.includes("期望 ';'"),
        `bare object expressions should not be parsed as unterminated statements\n${output}`);
    assert(!output.includes('failed to infer expression type'),
        `bare object expression execution should not take the :type analysis path\n${output}`);
    assert(!output.includes('SET_BY_INDEX') && !output.includes('SET_MEMBER'),
        `bare object expression execution should construct the object literal successfully\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
