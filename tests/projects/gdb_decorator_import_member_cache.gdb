set pagination off
set breakpoint pending on

file ./build/codex-wsl-gcc-debug/bin/zr_vm_cli
set args ./tests/fixtures/projects/decorator_import/decorator_import.zrp

start
sharedlibrary libzr_vm_core
break /mnt/e/Git/zr_vm/zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:321
commands
  silent
  printf "\n[execution_resolve_cached_member_symbol] cacheIndex=%u expectedKind=%u function=%p\n", cacheIndex, expectedKind, function
  if function != 0
    printf "  memberEntryLength=%u callSiteCacheLength=%u memberEntries=%p callSiteCaches=%p\n", function->memberEntryLength, function->callSiteCacheLength, function->memberEntries, function->callSiteCaches
    if function->callSiteCaches != 0
      printf "  cache[0].kind=%u member=%u\n", function->callSiteCaches[0].kind, function->callSiteCaches[0].memberEntryIndex
      printf "  cache[1].kind=%u member=%u\n", function->callSiteCaches[1].kind, function->callSiteCaches[1].memberEntryIndex
      printf "  cache[2].kind=%u member=%u\n", function->callSiteCaches[2].kind, function->callSiteCaches[2].memberEntryIndex
      printf "  cache[3].kind=%u member=%u\n", function->callSiteCaches[3].kind, function->callSiteCaches[3].memberEntryIndex
      printf "  cache[4].kind=%u member=%u\n", function->callSiteCaches[4].kind, function->callSiteCaches[4].memberEntryIndex
    end
  end
  if cacheEntry != 0
    printf "  cacheEntry=%p kind=%u memberEntryIndex=%u hits=%u misses=%u\n", cacheEntry, cacheEntry->kind, cacheEntry->memberEntryIndex, cacheEntry->runtimeHitCount, cacheEntry->runtimeMissCount
  else
    printf "  cacheEntry=<null>\n"
  end
  bt 6
  continue
end

continue
