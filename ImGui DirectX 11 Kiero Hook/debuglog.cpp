#include "debuglog.h"
#include <Windows.h>
#include <Psapi.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>

#pragma comment(lib, "psapi.lib")

namespace
{
    const char* kLogPath = "D:\\movement gandronima\\cheat_log.txt";

    void write_file(const char* text)
    {
        HANDLE h = CreateFileA(kLogPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(h, text, (DWORD)lstrlenA(text), &written, nullptr);
        CloseHandle(h);
    }

    // Resolve "module+offset" for a crash address so the log is actionable.
    void describe_addr(void* addr, char* out, size_t n)
    {
        HMODULE mods[256]; DWORD needed = 0;
        HANDLE proc = GetCurrentProcess();
        if (EnumProcessModules(proc, mods, sizeof(mods), &needed))
        {
            for (unsigned i = 0; i < needed / sizeof(HMODULE); i++)
            {
                MODULEINFO mi{};
                if (!GetModuleInformation(proc, mods[i], &mi, sizeof(mi))) continue;
                auto base = reinterpret_cast<std::uintptr_t>(mi.lpBaseOfDll);
                auto a    = reinterpret_cast<std::uintptr_t>(addr);
                if (a >= base && a < base + mi.SizeOfImage)
                {
                    char name[MAX_PATH]{};
                    GetModuleBaseNameA(proc, mods[i], name, MAX_PATH);
                    wsprintfA(out, "%s+0x%llX", name, (unsigned long long)(a - base));
                    return;
                }
            }
        }
        wsprintfA(out, "0x%llX", (unsigned long long)(uintptr_t)addr);
    }

    LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep)
    {
        char where[320]{};
        describe_addr(ep->ExceptionRecord->ExceptionAddress, where, sizeof(where));
        char line[512]{};
        wsprintfA(line, "\n[CRASH] code=0x%08X at %s\n",
                  ep->ExceptionRecord->ExceptionCode, where);
        write_file(line);
        dbg::log("%s", line);
        return EXCEPTION_CONTINUE_SEARCH; // let the game crash normally, but we logged it
    }
}

void dbg::init()
{
#ifdef _DEBUG
    AllocConsole();
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA("movement gandronima - debug");
#endif
    SetUnhandledExceptionFilter(crash_filter);
    dbg::log("[init] debug logging online\n");
}

void dbg::log(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
    va_end(args);
#ifdef _DEBUG
    fputs(buf, stdout);
    fflush(stdout);
#endif
    write_file(buf);
}
