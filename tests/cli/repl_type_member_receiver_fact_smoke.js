const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_member_receiver_fact_smoke.js <zr_vm_cli>');
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

    child.stdin.write(':type {a: 1 + 2}.a\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Type: object'),
        `:type should keep the existing member expression type result\n${output}`);
    assert(output.includes('Expression: member exact'),
        `:type should print the outer member expression fact\n${output}`);
    assert(output.includes('Member: a'),
        `:type should print the outer member payload\n${output}`);
    assert(output.includes('Reference: member a'),
        `:type should print the outer member reference fact\n${output}`);
    assert(output.includes('Expression: object exact'),
        `:type should print the aggregate receiver expression fact\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print the receiver object's nested value expression fact\n${output}`);
    assert(output.includes('Constant: 3'),
        `:type should print folded constants from the receiver object expression facts\n${output}`);
    assert(output.includes('Numeric range: 3..3'),
        `:type should preserve receiver nested numeric facts\n${output}`);
    assert(!output.includes('Constant: 1') &&
        !output.includes('Constant: 2') &&
        !output.includes('\n3\n') &&
        !output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not dump leaf constants, execute the member expression, or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
