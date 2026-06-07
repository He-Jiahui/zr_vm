const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_conditional_branch_smoke.js <zr_vm_cli>');
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

    child.stdin.write(':type true ? 1 : 2\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: int'),
        `:type should infer the conditional expression type\n${output}`);
    assert(output.includes('Numeric range: 1..1'),
        `:type should print the selected branch numeric range\n${output}`);
    assert(output.includes('Expression: conditional exact'),
        `:type should print the shared conditional expression fact\n${output}`);
    assert(output.includes('Logical value: true'),
        `:type should print the shared constant condition logical fact\n${output}`);
    assert(output.includes('Reachability: unreachable because a constant branch skips evaluation'),
        `:type should print the shared reachability fact for the skipped branch\n${output}`);
    assert(!output.includes('\n1\n') && !output.includes('failed to infer expression type'),
        `:type should not execute the conditional expression or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
