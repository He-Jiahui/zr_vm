#ifndef ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PUBLICATION_H
#define ZR_VM_PARSER_BACKEND_AOT_C_ZRP_METADATA_PUBLICATION_H

#include "backend_aot_c_zrp_metadata_prune.h"

TZrBool backend_aot_c_publish_compacted_zrp_metadata(const SZrAotWriterOptions *options,
                                                     const SZrAotCEmbeddedZrpMetadata *metadata);

#endif
