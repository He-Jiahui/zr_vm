#include "type_inference_reflection_surface.h"

#include <string.h>

static const SZrParserReflectionCompileTypeDescriptor
        kReflectionCompileTypeDescriptors[] = {
                {"zr.builtin.TypeInfo",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_NONE,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_METADATA_MEMBERS,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Class",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_CLASS,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Struct",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_STRUCT,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Function",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_FUNCTION,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_CALLABLE_MEMBERS,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Field",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_FIELD,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Method",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_METHOD,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_CALLABLE_MEMBERS,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Property",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_PROPERTY,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Parameter",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_PARAMETER,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"Object",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_OBJECT,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_METADATA_ROOT,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"zr.reflection.Type",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_NONE,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_RUNTIME_TYPE_MEMBERS,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"zr.reflection.TypeId",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_ID,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_NONE,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"zr.reflection.TypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_REPRESENTED_TYPE_ID,
                 ZR_REFLECTION_TYPE_CATEGORY_ERASED},
                {"zr.reflection.declaration.ClassTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_CLASS_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_CLASS},
                {"zr.reflection.declaration.ConcreteClassTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_CONCRETE_CLASS_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_CLASS_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_CONSTRUCTIBLE,
                 ZR_REFLECTION_TYPE_CATEGORY_CONCRETE_CLASS},
                {"zr.reflection.declaration.InstanceClassTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_INSTANCE_CLASS_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_CONCRETE_CLASS_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_INSTANCE_CLASS},
                {"zr.reflection.declaration.StructTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_STRUCT_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_CONSTRUCTIBLE,
                 ZR_REFLECTION_TYPE_CATEGORY_STRUCT},
                {"zr.reflection.declaration.InterfaceTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_INTERFACE_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_INTERFACE},
                {"zr.reflection.declaration.ResourceClassTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_RESOURCE_CLASS_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_RESOURCE_CLASS},
                {"zr.reflection.declaration.RefStructTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_REF_STRUCT_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_REF_STRUCT},
                {"zr.reflection.declaration.EnumTypeOf",
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_ENUM_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_TYPE_OF,
                 ZR_PARSER_REFLECTION_COMPILE_SURFACE_NONE,
                 ZR_REFLECTION_TYPE_CATEGORY_ENUM},
        };

const SZrParserReflectionCompileTypeDescriptor *
ZrParser_ReflectionCompileSurface_Find(const TZrChar *canonicalName) {
    if (canonicalName == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U;
         index < ZR_ARRAY_COUNT(kReflectionCompileTypeDescriptors);
         index++) {
        const SZrParserReflectionCompileTypeDescriptor *descriptor =
                &kReflectionCompileTypeDescriptors[index];

        if (strcmp(descriptor->canonicalName, canonicalName) == 0) {
            return descriptor;
        }
    }
    return ZR_NULL;
}

const SZrParserReflectionCompileTypeDescriptor *
ZrParser_ReflectionCompileSurface_FindByCapability(
        EZrParserReflectionCompileCapability capability) {
    if (capability == ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U;
         index < ZR_ARRAY_COUNT(kReflectionCompileTypeDescriptors);
         index++) {
        if (kReflectionCompileTypeDescriptors[index].capability == capability) {
            return &kReflectionCompileTypeDescriptors[index];
        }
    }
    return ZR_NULL;
}

const TZrChar *ZrParser_ReflectionCompileSurface_DescriptorName(
        EZrReflectionTypeCategory category) {
    for (TZrSize index = 0U;
         index < ZR_ARRAY_COUNT(kReflectionCompileTypeDescriptors);
         index++) {
        const SZrParserReflectionCompileTypeDescriptor *descriptor =
                &kReflectionCompileTypeDescriptors[index];

        if (descriptor->descriptorCategory == category &&
            descriptor->capability >=
                    ZR_PARSER_REFLECTION_COMPILE_CAPABILITY_CLASS_TYPE_OF) {
            return descriptor->canonicalName;
        }
    }
    return "zr.reflection.Type";
}
