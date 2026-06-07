const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_expression_fact_smoke.js <zr_vm_cli>');
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

    child.stdin.write(':type 42\n');
    child.stdin.write(':type 1 + 2\n');
    child.stdin.write(':type "zr"\n');
    child.stdin.write(':type "a\\\"b\\\\c\\n\\t"\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    assert(output.includes('Expression: literal exact'),
        `:type should print expression kind/exactness for literal facts\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print expression kind/exactness for binary facts\n${output}`);
    assert(output.includes('Constant: 42'),
        `:type should print integer constants from shared expression facts\n${output}`);
    assert(output.includes('Constant: 3'),
        `:type should print folded binary constants from shared expression facts\n${output}`);
    assert(output.includes('Constant: "zr"'),
        `:type should print string constants from shared expression facts\n${output}`);
    assert(output.includes('Constant: "a\\"b\\\\c\\n\\t"'),
        `:type should escape quotes, backslashes, and control characters in string constants\n${output}`);
    assert(!output.includes('Constant: "a"b'),
        `:type should not print raw embedded quotes in string constants\n${output}`);
    assert(!output.includes('\n42\n') &&
        !output.includes('\n3\n') &&
        !output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not execute the expression or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
