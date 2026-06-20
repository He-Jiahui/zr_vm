const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

function readGrammar() {
    return JSON.parse(fs.readFileSync(path.resolve(__dirname, '..', 'syntaxes', 'zr.tmLanguage.json'), 'utf8'));
}

function repositoryPatterns(grammar, key) {
    return grammar.repository?.[key]?.patterns ?? [];
}

test('ZR grammar highlights union declarations, lowercase primitives, and variant members', () => {
    const grammar = readGrammar();
    const topLevelIncludes = grammar.patterns?.map((rule) => rule.include).filter(Boolean) ?? [];
    const unionDeclarationRule = repositoryPatterns(grammar, 'union-declarations')
        .find((rule) => rule.name === 'meta.declaration.union.zr');
    const keywordRule = repositoryPatterns(grammar, 'keywords')
        .find((rule) => rule.name === 'keyword.other.zr');
    const primitiveRule = repositoryPatterns(grammar, 'types')
        .find((rule) => rule.name === 'storage.type.primitive.zr');
    const variantPatterns = repositoryPatterns(grammar, 'union-variants');
    const defaultVariantRule = variantPatterns
        .find((rule) => rule.name === 'meta.union.variant.default.zr');
    const variantDeclarationRule = variantPatterns
        .find((rule) => rule.name === 'meta.union.variant.declaration.zr');
    const payloadFieldRule = variantPatterns
        .find((rule) => rule.name === 'variable.parameter.union.payload.zr');
    const variantMemberRule = variantPatterns
        .find((rule) => rule.name === 'variable.other.member.variant.zr');

    assert(topLevelIncludes.includes('#union-declarations'));
    assert(topLevelIncludes.indexOf('#union-declarations') < topLevelIncludes.indexOf('#keywords'));

    assert(unionDeclarationRule, 'Expected union declaration rule');
    const unionDeclarationRegex = new RegExp(unionDeclarationRule.match);
    const declarationMatch = unionDeclarationRegex.exec('union Shape {');
    assert(declarationMatch, 'Expected union declaration rule to match union Shape');
    assert.equal(declarationMatch[1], 'union');
    assert.equal(declarationMatch[2], 'Shape');
    assert.equal(unionDeclarationRule.captures?.['1']?.name, 'storage.type.union.zr');
    assert.equal(unionDeclarationRule.captures?.['2']?.name, 'entity.name.type.union.zr');

    assert(keywordRule, 'Expected keyword.other.zr rule');
    assert(new RegExp(keywordRule.match).test('union'));
    assert(new RegExp(keywordRule.match).test('using'));

    assert(primitiveRule, 'Expected primitive type rule');
    assert(new RegExp(primitiveRule.match).test('int'));
    assert(new RegExp(primitiveRule.match).test('float'));
    assert(new RegExp(primitiveRule.match).test('string'));

    assert(defaultVariantRule, 'Expected default union variant rule');
    assert(new RegExp(defaultVariantRule.match).test('@Available'));
    assert.equal(defaultVariantRule.captures?.['2']?.name, 'entity.name.type.variant.zr');

    assert(variantDeclarationRule, 'Expected union variant declaration rule');
    assert(new RegExp(variantDeclarationRule.match, 'm').test('    Circle(radius: float);'));
    assert(new RegExp(variantDeclarationRule.match, 'm').test('    Rect { width: float; height: float; }'));
    assert(new RegExp(variantDeclarationRule.match, 'm').test('    Empty;'));
    assert.equal(variantDeclarationRule.captures?.['2']?.name, 'entity.name.type.variant.zr');

    assert(payloadFieldRule, 'Expected union payload field rule');
    assert(new RegExp(payloadFieldRule.match).test('radius: float'));
    assert(new RegExp(payloadFieldRule.match).test('width: float'));
    assert.equal(payloadFieldRule.captures?.['1']?.name, 'variable.parameter.union.payload.zr');

    assert(variantMemberRule, 'Expected variant member rule');
    assert(new RegExp(variantMemberRule.match).test('.Some'));
    assert(new RegExp(variantMemberRule.match).test('.Rect'));
});
