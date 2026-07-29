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
    const controlRule = repositoryPatterns(grammar, 'keywords')
        .find((rule) => rule.name === 'keyword.control.zr');
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
    assert.equal(topLevelIncludes.filter((include) => include === '#decorators').length, 1);

    assert(unionDeclarationRule, 'Expected union declaration rule');
    const unionDeclarationRegex = new RegExp(unionDeclarationRule.match);
    const declarationMatch = unionDeclarationRegex.exec('union Shape {');
    assert(declarationMatch, 'Expected union declaration rule to match union Shape');
    assert.equal(declarationMatch[1], 'union');
    assert.equal(declarationMatch[2], 'Shape');
    assert.equal(unionDeclarationRule.captures?.['1']?.name, 'storage.type.union.zr');
    assert.equal(unionDeclarationRule.captures?.['2']?.name, 'entity.name.type.union.zr');

    assert(keywordRule, 'Expected keyword.other.zr rule');
    const keywordRegex = new RegExp(keywordRule.match);
    assert(keywordRegex.test('union'));
    assert(keywordRegex.test('using'));
    assert(keywordRegex.test('module'));
    assert(keywordRegex.test('delegate'));
    assert(!keywordRegex.test('func'));
    assert(!keywordRegex.test('test'));
    assert(!keywordRegex.test('new'));

    assert(controlRule, 'Expected keyword.control.zr rule');
    assert(new RegExp(controlRule.match).test('require(value >= 0)'));

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

test('ZR grammar highlights redesigned declarations, references, construction, and attributes', () => {
    const grammar = readGrammar();
    const declarationPatterns = repositoryPatterns(grammar, 'declarations');
    const modifierPatterns = repositoryPatterns(grammar, 'modifiers');
    const keywordPatterns = repositoryPatterns(grammar, 'keywords');
    const decoratorPatterns = repositoryPatterns(grammar, 'decorators');
    const functionDeclarationRule = declarationPatterns
        .find((rule) => rule.name === 'meta.declaration.function.zr');
    const typeDeclarationRule = declarationPatterns
        .find((rule) => rule.name === 'meta.declaration.type.zr');
    const bindingRule = modifierPatterns
        .find((rule) => rule.name === 'storage.modifier.binding.zr');
    const referenceRule = modifierPatterns
        .find((rule) => rule.name === 'storage.modifier.reference.zr');
    const capabilityRule = modifierPatterns
        .find((rule) => rule.name === 'storage.modifier.capability.zr');
    const constructionRule = keywordPatterns
        .find((rule) => rule.name === 'keyword.operator.construct.zr');
    const intrinsicRule = keywordPatterns
        .find((rule) => rule.name === 'support.function.intrinsic.zr');
    const legacyPatterns = repositoryPatterns(grammar, 'legacy-percent-syntax');
    const legacyPercentRule = legacyPatterns
        .find((rule) => rule.name === 'invalid.deprecated.percent-syntax.zr');
    const legacyConstructionPatterns = repositoryPatterns(grammar, 'legacy-dollar-construction-syntax');
    const legacyConstructionRule = legacyConstructionPatterns
        .find((rule) => rule.name === 'invalid.deprecated.dollar-construction-syntax.zr');
    const decoratorRule = decoratorPatterns
        .find((rule) => rule.name === 'meta.attribute.zr');

    assert(functionDeclarationRule, 'Expected fn declaration rule');
    assert(new RegExp(functionDeclarationRule.match).test('pub async fn transform(value: ref readonly Data): Result {'));
    assert.equal(functionDeclarationRule.captures?.['1']?.name, 'storage.type.function.zr');
    assert.equal(functionDeclarationRule.captures?.['2']?.name, 'entity.name.function.zr');

    assert(typeDeclarationRule, 'Expected redesigned type declaration rule');
    assert(new RegExp(typeDeclarationRule.match).test('readonly ref struct Span<T> {'));
    assert(new RegExp(typeDeclarationRule.match).test('resource class Texture {'));
    assert.equal(typeDeclarationRule.captures?.['2']?.name, 'storage.type.declaration.zr');
    assert.equal(typeDeclarationRule.captures?.['3']?.name, 'entity.name.type.zr');

    assert(bindingRule, 'Expected let/var binding modifier rule');
    assert(new RegExp(bindingRule.match).test('let point = init Point(1, 2);'));
    assert(referenceRule, 'Expected ref/in/out/scoped/readonly modifier rule');
    assert(new RegExp(referenceRule.match).test('scoped ref readonly Data'));
    assert(new RegExp(referenceRule.match).test('result: out Data'));
    assert(capabilityRule, 'Expected modern declaration modifier rule');
    assert(new RegExp(capabilityRule.match).test('native extern("zr.math") {'));
    assert(new RegExp(capabilityRule.match).test('comptime fn build(): void {'));

    assert(constructionRule, 'Expected init/new/own construction rule');
    assert(new RegExp(constructionRule.match).test('init Point(1, 2)'));
    assert(new RegExp(constructionRule.match).test('new Document()'));
    assert(new RegExp(constructionRule.match).test('own Texture()'));
    assert(intrinsicRule, 'Expected import/typeid/typeof intrinsic rule');
    assert(new RegExp(intrinsicRule.match).test('import("zr.math")'));
    assert(new RegExp(intrinsicRule.match).test('typeof(value)'));
    assert(new RegExp(intrinsicRule.match).test('typeid(Point)'));

    assert(legacyPercentRule, 'Expected deprecated percent syntax rule');
    const legacyPercentRegex = new RegExp(legacyPercentRule.match);
    assert(legacyPercentRegex.test('%import("zr.math")'));
    assert(legacyPercentRegex.test('%compileTime fn build(): void {}'));
    assert(legacyPercentRegex.test('%unknownLegacyDirective'));
    assert(!legacyPercentRegex.test('rate % divisor'));

    assert(legacyConstructionRule, 'Expected deprecated dollar construction syntax rule');
    const legacyConstructionRegex = new RegExp(legacyConstructionRule.match);
    assert(legacyConstructionRegex.test('$Point(1, 2)'));
    assert(legacyConstructionRegex.test('$(factory)(1, 2)'));
    assert(!legacyConstructionRegex.test('new Point(1, 2)'));
    assert(!legacyConstructionRegex.test('value $ divisor'));

    assert(decoratorRule, 'Expected #zr.*# attribute rule');
    assert.equal(decoratorRule.begin, '(#)(?=[A-Za-z_])');
    assert.equal(decoratorRule.end, '(#)');
    const decoratorNameRule = decoratorRule.patterns
        ?.find((rule) => rule.name === 'entity.name.function.attribute.zr');
    assert(decoratorNameRule, 'Expected dotted attribute name rule');
    assert(new RegExp(decoratorNameRule.match).test('zr.testing.case'));
});
