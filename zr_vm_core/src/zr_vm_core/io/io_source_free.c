#include "zr_vm_core/io.h"
#include "zr_vm_core/memory.h"

static void io_source_free_functions(SZrGlobalState *global, SZrIoFunction *functions, TZrSize count);
static void io_source_free_members(SZrGlobalState *global, SZrIoMemberDeclare *members, TZrSize count);

/* 与 io.c 的 IO 内存类别配对；长度必须仍与实际分配时一致。 */
static void io_source_free_storage(SZrGlobalState *global, TZrPtr storage, TZrSize bytes) {
    if (storage != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(global, storage, bytes, ZR_MEMORY_NATIVE_TYPE_IO);
    }
}

/* 源树每个数组均由所属节点的 count 定界，嵌套内容先递归回收。 */
#define IO_SOURCE_FREE_ARRAY(GLOBAL, POINTER, COUNT) \
    io_source_free_storage((GLOBAL), (POINTER), sizeof(*(POINTER)) * (COUNT))

/* 函数型常量拥有独立子函数树，普通值中的 GC 对象不由此释放。 */
static void io_source_free_constant(SZrGlobalState *global, SZrIoFunctionConstantVariable *constant) {
    if (constant->hasFunctionValue) {
        io_source_free_functions(global, constant->functionValue, 1u);
    }
}

/* 参数默认值和装饰器值可能各自包含函数常量，需在参数数组前释放。 */
static void io_source_free_parameters(SZrGlobalState *global,
                                      SZrIoFunctionMetadataParameter *parameters,
                                      TZrSize count) {
    if (parameters == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        SZrIoFunctionMetadataParameter *parameter = &parameters[index];
        if (parameter->hasDefaultValue) {
            io_source_free_constant(global, &parameter->defaultValue);
        }
        if (parameter->hasDecoratorMetadata) {
            io_source_free_constant(global, &parameter->decoratorMetadataValue);
        }
        IO_SOURCE_FREE_ARRAY(global, parameter->decoratorNames, parameter->decoratorNamesLength);
    }
    IO_SOURCE_FREE_ARRAY(global, parameters, count);
}

/* 类型化导出有参数类型和泛型约束两层数组，按拥有者反向释放。 */
static void io_source_free_typed_exports(SZrGlobalState *global,
                                        SZrIoFunctionTypedExportSymbol *symbols,
                                        TZrSize count) {
    if (symbols == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        SZrIoFunctionTypedExportSymbol *symbol = &symbols[index];
        if (symbol->genericParameters != ZR_NULL) {
            for (TZrSize genericIndex = 0u; genericIndex < symbol->genericParameterCount; ++genericIndex) {
                SZrIoFunctionTypedGenericParameter *parameter = &symbol->genericParameters[genericIndex];
                IO_SOURCE_FREE_ARRAY(global, parameter->constraintTypeNames, parameter->constraintTypeCount);
            }
        }
        IO_SOURCE_FREE_ARRAY(global, symbol->genericParameters, symbol->genericParameterCount);
        IO_SOURCE_FREE_ARRAY(global, symbol->parameterTypes, symbol->parameterCount);
    }
    IO_SOURCE_FREE_ARRAY(global, symbols, count);
}

/* 类原型持有继承引用和成员声明，成员的判别式由下层释放器处理。 */
static void io_source_free_classes(SZrGlobalState *global, SZrIoClass *classes, TZrSize count) {
    if (classes == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        IO_SOURCE_FREE_ARRAY(global, classes[index].superClasses, classes[index].superClassLength);
        io_source_free_members(global, classes[index].declares, classes[index].declaresLength);
    }
    IO_SOURCE_FREE_ARRAY(global, classes, count);
}

/* 结构体原型与类原型共享成员所有权规则。 */
static void io_source_free_structs(SZrGlobalState *global, SZrIoStruct *structs, TZrSize count) {
    if (structs == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        IO_SOURCE_FREE_ARRAY(global, structs[index].superStructs, structs[index].superStructLength);
        io_source_free_members(global, structs[index].declares, structs[index].declaresLength);
    }
    IO_SOURCE_FREE_ARRAY(global, structs, count);
}

/* 函数源树是释放的递归核心；仅回收 IO 原生数组，保留已转交 GC 管理的对象。 */
static void io_source_free_functions(SZrGlobalState *global, SZrIoFunction *functions, TZrSize count) {
    if (functions == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        SZrIoFunction *function = &functions[index];

        if (function->constantVariables != ZR_NULL) {
            for (TZrSize constantIndex = 0u; constantIndex < function->constantVariablesLength; ++constantIndex) {
                io_source_free_constant(global, &function->constantVariables[constantIndex]);
            }
        }
        if (function->hasDecoratorMetadata) {
            io_source_free_constant(global, &function->decoratorMetadataValue);
        }
        io_source_free_parameters(global, function->parameterMetadata, function->parameterMetadataLength);
        io_source_free_typed_exports(global, function->typedExportedSymbols, function->typedExportedSymbolsLength);
        if (function->exportedCallableSummaries != ZR_NULL) {
            for (TZrSize summaryIndex = 0u; summaryIndex < function->exportedCallableSummariesLength; ++summaryIndex) {
                SZrIoFunctionCallableSummary *summary = &function->exportedCallableSummaries[summaryIndex];
                IO_SOURCE_FREE_ARRAY(global, summary->effects, summary->effectCount);
            }
        }
        if (function->compileTimeVariableInfos != ZR_NULL) {
            for (TZrSize variableIndex = 0u; variableIndex < function->compileTimeVariableInfosLength; ++variableIndex) {
                SZrIoFunctionCompileTimeVariableInfo *variable = &function->compileTimeVariableInfos[variableIndex];
                IO_SOURCE_FREE_ARRAY(global, variable->pathBindings, variable->pathBindingsLength);
            }
        }
        if (function->compileTimeFunctionInfos != ZR_NULL) {
            for (TZrSize functionIndex = 0u; functionIndex < function->compileTimeFunctionInfosLength; ++functionIndex) {
                SZrIoFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[functionIndex];
                io_source_free_parameters(global, info->parameters, info->parameterCount);
            }
        }
        io_source_free_classes(global, function->classes, function->classesLength);
        io_source_free_structs(global, function->structs, function->structsLength);
        if (function->closures != ZR_NULL) {
            for (TZrSize closureIndex = 0u; closureIndex < function->closuresLength; ++closureIndex) {
                io_source_free_functions(global, function->closures[closureIndex].subFunction, 1u);
            }
        }
        if (function->debugInfos != ZR_NULL) {
            for (TZrSize debugIndex = 0u; debugIndex < function->debugInfosLength; ++debugIndex) {
                SZrIoFunctionDebugInfo *debug = &function->debugInfos[debugIndex];
                IO_SOURCE_FREE_ARRAY(global, debug->instructionsLine, debug->instructionsLength);
                IO_SOURCE_FREE_ARRAY(global, debug->instructionRanges, debug->instructionsLength);
            }
        }

        IO_SOURCE_FREE_ARRAY(global, function->frameSlotLayouts, function->frameSlotLayoutsLength);
        IO_SOURCE_FREE_ARRAY(global, function->instructions, function->instructionsLength);
        IO_SOURCE_FREE_ARRAY(global, function->localVariables, function->localVariablesLength);
        IO_SOURCE_FREE_ARRAY(global, function->closureVariables, function->closureVariablesLength);
        IO_SOURCE_FREE_ARRAY(global, function->catchClauses, function->catchClauseCount);
        IO_SOURCE_FREE_ARRAY(global, function->exceptionHandlers, function->exceptionHandlerCount);
        IO_SOURCE_FREE_ARRAY(global, function->constantVariables, function->constantVariablesLength);
        IO_SOURCE_FREE_ARRAY(global, function->exportedVariables, function->exportedVariablesLength);
        IO_SOURCE_FREE_ARRAY(global, function->typedLocalBindings, function->typedLocalBindingsLength);
        IO_SOURCE_FREE_ARRAY(global, function->typedClosureBindings, function->typedClosureBindingsLength);
        IO_SOURCE_FREE_ARRAY(global, function->metadataTokenRecords, function->metadataTokenRecordLength);
        IO_SOURCE_FREE_ARRAY(global, function->moduleMetadataTokenRecords, function->moduleMetadataTokenRecordLength);
        IO_SOURCE_FREE_ARRAY(global, function->signatureBlobHeap, function->signatureBlobHeapLength);
        IO_SOURCE_FREE_ARRAY(global, function->metadataStringHeap, function->metadataStringHeapLength);
        IO_SOURCE_FREE_ARRAY(global, function->moduleMetadataBindings, function->moduleMetadataBindingLength);
        IO_SOURCE_FREE_ARRAY(global, function->staticImports, function->staticImportsLength);
        IO_SOURCE_FREE_ARRAY(global, function->moduleEntryEffects, function->moduleEntryEffectsLength);
        IO_SOURCE_FREE_ARRAY(global, function->exportedCallableSummaries, function->exportedCallableSummariesLength);
        IO_SOURCE_FREE_ARRAY(global, function->topLevelCallableBindings, function->topLevelCallableBindingsLength);
        IO_SOURCE_FREE_ARRAY(global, function->compileTimeVariableInfos, function->compileTimeVariableInfosLength);
        IO_SOURCE_FREE_ARRAY(global, function->compileTimeFunctionInfos, function->compileTimeFunctionInfosLength);
        IO_SOURCE_FREE_ARRAY(global, function->escapeBindings, function->escapeBindingLength);
        IO_SOURCE_FREE_ARRAY(global, function->returnEscapeSlots, function->returnEscapeSlotCount);
        IO_SOURCE_FREE_ARRAY(global, function->decoratorNames, function->decoratorNamesLength);
        IO_SOURCE_FREE_ARRAY(global, function->memberEntries, function->memberEntriesLength);
        IO_SOURCE_FREE_ARRAY(global, function->semIrTypeTable, function->semIrTypeTableLength);
        IO_SOURCE_FREE_ARRAY(global, function->semIrOwnershipTable, function->semIrOwnershipTableLength);
        IO_SOURCE_FREE_ARRAY(global, function->semIrEffectTable, function->semIrEffectTableLength);
        IO_SOURCE_FREE_ARRAY(global, function->semIrBlockTable, function->semIrBlockTableLength);
        IO_SOURCE_FREE_ARRAY(global, function->semIrInstructions, function->semIrInstructionLength);
        IO_SOURCE_FREE_ARRAY(global, function->semIrDeoptTable, function->semIrDeoptTableLength);
        IO_SOURCE_FREE_ARRAY(global, function->callSiteCaches, function->callSiteCacheLength);
        IO_SOURCE_FREE_ARRAY(global, function->nativeImportContracts, function->nativeImportContractLength);
        IO_SOURCE_FREE_ARRAY(global, function->testManifestData, function->testManifestDataLength);
        IO_SOURCE_FREE_ARRAY(global, function->prototypeData, function->prototypeDataLength);
        IO_SOURCE_FREE_ARRAY(global, function->closures, function->closuresLength);
        IO_SOURCE_FREE_ARRAY(global, function->debugInfos, function->debugInfosLength);
    }
    IO_SOURCE_FREE_ARRAY(global, functions, count);
}

/* 依照读取时的 type 判别成员联合体，先释放有效分支再释放声明表。 */
static void io_source_free_members(SZrGlobalState *global, SZrIoMemberDeclare *members, TZrSize count) {
    if (members == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        SZrIoMemberDeclare *member = &members[index];
        switch (member->type) {
            case ZR_IO_MEMBER_DECLARE_TYPE_METHOD:
                if (member->method != ZR_NULL) {
                    io_source_free_functions(global, member->method->functions, member->method->functionsLength);
                    IO_SOURCE_FREE_ARRAY(global, member->method, 1u);
                }
                break;
            case ZR_IO_MEMBER_DECLARE_TYPE_META:
                if (member->meta != ZR_NULL) {
                    io_source_free_functions(global, member->meta->functions, member->meta->functionsLength);
                    IO_SOURCE_FREE_ARRAY(global, member->meta, 1u);
                }
                break;
            case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY:
                if (member->property != ZR_NULL) {
                    io_source_free_functions(global, member->property->getter, 1u);
                    io_source_free_functions(global, member->property->setter, 1u);
                    IO_SOURCE_FREE_ARRAY(global, member->property, 1u);
                }
                break;
            case ZR_IO_MEMBER_DECLARE_TYPE_ENUM:
                IO_SOURCE_FREE_ARRAY(global, member->enumField, 1u);
                break;
            case ZR_IO_MEMBER_DECLARE_TYPE_FIELD:
                IO_SOURCE_FREE_ARRAY(global, member->field, 1u);
                break;
            default:
                break;
        }
    }
    IO_SOURCE_FREE_ARRAY(global, members, count);
}

/* 模块声明的各类型子树所有权不同，释放器必须与读取器的分支协议一致。 */
static void io_source_free_module_declares(SZrGlobalState *global,
                                          SZrIoModuleDeclare *declares,
                                          TZrSize count) {
    if (declares == ZR_NULL) {
        return;
    }
    for (TZrSize index = 0u; index < count; ++index) {
        SZrIoModuleDeclare *declare = &declares[index];
        switch (declare->type) {
            case ZR_IO_MODULE_DECLARE_TYPE_CLASS:
                io_source_free_classes(global, declare->class_, 1u);
                break;
            case ZR_IO_MODULE_DECLARE_TYPE_STRUCT:
                io_source_free_structs(global, declare->struct_, 1u);
                break;
            case ZR_IO_MODULE_DECLARE_TYPE_INTERFACE:
                if (declare->interface_ != ZR_NULL) {
                    IO_SOURCE_FREE_ARRAY(global, declare->interface_->superInterfaces,
                                         declare->interface_->superInterfaceLength);
                    io_source_free_members(global, declare->interface_->declares, declare->interface_->declaresLength);
                    IO_SOURCE_FREE_ARRAY(global, declare->interface_, 1u);
                }
                break;
            case ZR_IO_MODULE_DECLARE_TYPE_FUNCTION:
                io_source_free_functions(global, declare->function, 1u);
                break;
            case ZR_IO_MODULE_DECLARE_TYPE_ENUM:
                if (declare->enum_ != ZR_NULL) {
                    IO_SOURCE_FREE_ARRAY(global, declare->enum_->fields, declare->enum_->fieldsLength);
                    IO_SOURCE_FREE_ARRAY(global, declare->enum_, 1u);
                }
                break;
            case ZR_IO_MODULE_DECLARE_TYPE_FIELD:
                IO_SOURCE_FREE_ARRAY(global, declare->field, 1u);
                break;
            default:
                break;
        }
    }
    IO_SOURCE_FREE_ARRAY(global, declares, count);
}

/* 释放已读取的源树；既可清理未投影的输入，也可在运行时或 AOT 复制所需数据后调用。 */
void ZrCore_Io_ReadSourceFree(SZrGlobalState *global, SZrIoSource *source) {
    if (global == ZR_NULL || source == ZR_NULL) {
        return;
    }
    /* 只回收源树拥有的原生数组；字符串和值由 GC 管理。 */
    if (source->modules != ZR_NULL) {
        for (TZrSize index = 0u; index < source->modulesLength; ++index) {
            SZrIoModule *module = &source->modules[index];
            IO_SOURCE_FREE_ARRAY(global, module->imports, module->importsLength);
            io_source_free_module_declares(global, module->declares, module->declaresLength);
            io_source_free_functions(global, module->entryFunction, 1u);
        }
    }
    IO_SOURCE_FREE_ARRAY(global, source->modules, source->modulesLength);
    IO_SOURCE_FREE_ARRAY(global, source, 1u);
}

#undef IO_SOURCE_FREE_ARRAY
