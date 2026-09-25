set pagination off
set confirm off
set breakpoint pending off
start
break compiler_semantic_cfg_finish
commands
  silent
  printf "source CFG finish: active=%u block=%u instructions=%llu\n", cs->preSemanticIrCfgActive, cs->preSemanticIrCfgBlock, (unsigned long long)cs->preSemanticIr.instructions.length
  continue
end
continue
if $_exitcode != 0
  quit 1
end
quit 0
