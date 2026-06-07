const { spawn } = require('child_process');

const cliPath = process.argv[2];
if (!cliPath) {
    console.error('usage: node repl_type_lambda_body_fact_smoke.js <zr_vm_cli>');
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

    child.stdin.write('var seed = 2;\n');
    child.stdin.write('\n');
    child.stdin.write(':type () => { return seed + 3; }\n');
    child.stdin.write(':type () => { return true || false; }\n');
    child.stdin.write(':type () => { while (true) { return 1 + 2; } }\n');
    child.stdin.write(':quit\n');
    child.stdin.end();

    const exitCode = await new Promise((resolve) => {
        child.on('exit', resolve);
    });

    assert(exitCode === 0, `REPL exited with code ${exitCode}\n${output}`);
    const intLambdaTypeCount = (output.match(/Type: %func\(\)->int/g) || []).length;
    assert(intLambdaTypeCount >= 2,
        `:type should infer numeric lambda function types through direct and control-flow returns\n${output}`);
    assert(output.includes('Type: %func()->bool'),
        `:type should infer the logical lambda function type\n${output}`);
    assert(output.includes('Expression: lambda exact'),
        `:type should print the lambda expression fact\n${output}`);
    assert(output.includes('Expression: binary exact'),
        `:type should print expression facts from the lambda return body\n${output}`);
    assert(output.includes('Numeric range: 5..5'),
        `:type should print numeric facts from the lambda return body\n${output}`);
    assert(output.includes('Numeric range: 3..3'),
        `:type should print numeric facts from lambda control-flow return bodies\n${output}`);
    assert(output.includes('Reference: read seed'),
        `:type should print reference facts from the lambda return body\n${output}`);
    assert(output.includes('Declared at: 1:5'),
        `:type should print nested reference declaration locations from the lambda body\n${output}`);
    assert(output.includes('Logical flow: short-circuits right operand'),
        `:type should print logical facts from the lambda return body\n${output}`);
    assert(output.includes('Reachability: unreachable because short-circuit skips evaluation'),
        `:type should print reachability facts from the lambda return body\n${output}`);
    assert(!output.includes('\n3\n') &&
        !output.includes('failed to infer expression type') &&
        !output.includes('Compiler Error'),
        `:type should not execute the lambda body or fail inference\n${output}`);
}

main().catch((error) => {
    console.error(error && error.stack ? error.stack : String(error));
    process.exit(1);
});
