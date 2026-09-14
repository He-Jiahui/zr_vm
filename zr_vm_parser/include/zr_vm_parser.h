//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_PARSER_H
#define ZR_VM_PARSER_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/lexer.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_array_lowering.h"
#include "zr_vm_parser/exec_ir_binding_facts.h"
#include "zr_vm_parser/exec_ir_alias.h"
#include "zr_vm_parser/exec_ir_ranges.h"
#include "zr_vm_parser/exec_ir_gvn.h"
#include "zr_vm_parser/aot_ir_lowering.h"
#include "zr_vm_parser/compile_optimization_profile.h"
#include "zr_vm_parser/compile_ir_cache.h"
#include "zr_vm_parser/exec_ir_fusion.h"
#include "zr_vm_parser/exec_ir_layout_visibility.h"
#include "zr_vm_parser/exec_ir_send_sync.h"
#include "zr_vm_parser/aot_generic_policy.h"
#include "zr_vm_parser/exec_ir_aggregate_layout.h"
#include "zr_vm_parser/exec_ir_escape.h"
#include "zr_vm_parser/exec_ir_allocation.h"
#include "zr_vm_parser/exec_ir_ownership_elision.h"
#include "zr_vm_parser/exec_ir_container_specialize.h"
#include "zr_vm_parser/exec_ir_loops.h"
#include "zr_vm_parser/exec_ir_vectorize.h"
#include "zr_vm_parser/optimization_remarks.h"
#include "zr_vm_parser/exec_ir_profile.h"
#include "zr_vm_parser/optimization_facts.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_parser/writer.h"

#endif //ZR_VM_PARSER_H
