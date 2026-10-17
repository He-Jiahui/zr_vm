#ifndef ZR_TESTS_WINDOWS_CRASH_DIAGNOSTICS_H
#define ZR_TESTS_WINDOWS_CRASH_DIAGNOSTICS_H

#if defined(_MSC_VER) && defined(_M_X64)
#include <stdio.h>
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

/* Report the original fault before CRT/Unity signal recovery changes its stack.
 * Debug CI keeps the PDBs beside the executables and runtime DLLs. */
static LONG CALLBACK zr_tests_windows_report_exception(EXCEPTION_POINTERS *exception) {
    static LONG reported = 0;
    DWORD code = exception->ExceptionRecord->ExceptionCode;
    HANDLE process = GetCurrentProcess();
    CONTEXT context;
    STACKFRAME64 frame = {0};

    if ((code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW &&
         code != EXCEPTION_ILLEGAL_INSTRUCTION) ||
        InterlockedCompareExchange(&reported, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    fprintf(stderr, "[zr-tests] Windows exception 0x%08lx at %p\n",
            (unsigned long)code, exception->ExceptionRecord->ExceptionAddress);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(process, NULL, TRUE)) {
        fflush(stderr);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    context = *exception->ContextRecord;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
    for (unsigned i = 0; i < 32 && frame.AddrPC.Offset != 0; ++i) {
        union {
            SYMBOL_INFO symbol;
            char bytes[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
        } storage = {0};
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line = {0};
        DWORD lineDisplacement = 0;
        storage.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
        storage.symbol.MaxNameLen = MAX_SYM_NAME;
        fprintf(stderr, "[zr-tests] #%u 0x%llx", i,
                (unsigned long long)frame.AddrPC.Offset);
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, &storage.symbol)) {
            fprintf(stderr, " %s+0x%llx", storage.symbol.Name,
                    (unsigned long long)displacement);
        }
        line.SizeOfStruct = sizeof(line);
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line)) {
            fprintf(stderr, " %s:%lu", line.FileName, (unsigned long)line.LineNumber);
        }
        fputc('\n', stderr);
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(),
                         &frame, &context, NULL, SymFunctionTableAccess64,
                         SymGetModuleBase64, NULL)) break;
    }
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void ZrTests_WindowsCrashDiagnostics_Install(void) {
    ULONG stackGuarantee = 32768;
    (void)SetThreadStackGuarantee(&stackGuarantee);
    (void)AddVectoredExceptionHandler(1, zr_tests_windows_report_exception);
}
#else
static void ZrTests_WindowsCrashDiagnostics_Install(void) {}
#endif

#endif
