const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_logical_comparison_smoke.js <zr_vm_cli>');
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

    child.stdin.write(':type 1 < 2\n');
    child.stdin.write(':type !(1 < 2)\n');
    child.stdin.write(':type (1 < 2) && (3 < 4)\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: bool'),
        `:type should infer the comparison expression type\n${output}`);
    assert(output.includes('Logical value: true'),
        `:type should print the shared constant comparison logical fact\n${output}`);
    assert(output.includes('Logical value: false'),
        `:type should print the shared unary comparison logical fact\n${output}`);
    const trueFactCount = (output.match(/Logical value: true/g) || []).length;
    assert(trueFactCount >= 2,
        `:type should print logical facts for both direct and composed comparisons\n${output}`);
    assert(!output.includes('\ntrue\n') && !output.includes('failed to infer expression type'),
        `:type should not execute the comparison expression or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
