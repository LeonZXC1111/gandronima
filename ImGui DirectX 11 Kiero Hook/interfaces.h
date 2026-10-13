#pragma once
//
// interfaces.h - resolve engine interfaces by walking a module's
// s_pInterfaceRegs list (what CreateInterface iterates). Matches by PREFIX so a
// changed version suffix still resolves. Robust: scans CreateInterface for every
// RIP-relative load and validates which one is the real interface list.
//
#include <Windows.h>
#include <cstdint>
#include <cstring>

struct InterfaceReg
{
    void* (*m_CreateFn)();
    const char* m_pName;
    InterfaceReg* m_pNext;
};

inline bool mem_readable(const void* p, std::size_t n = 1)
{
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD ok = PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & ok) || (mbi.Protect & PAGE_GUARD)) return false;
    return reinterpret_cast<std::uintptr_t>(p) + n <=
           reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
}

inline bool mem_writable(const void* p, std::size_t n = 1)
{
    if (!p) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD ok = PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & ok) || (mbi.Protect & PAGE_GUARD)) return false;
    return reinterpret_cast<std::uintptr_t>(p) + n <=
           reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
}

inline bool looks_like_reg(const InterfaceReg* r)
{
    if (!mem_readable(r, sizeof(InterfaceReg))) return false;
    if (!mem_readable(r->m_pName, 1)) return false;
    const char c = r->m_pName[0];
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); // interface names start with a letter
}

// Return the head of the module's InterfaceReg list, or null. Fully bounds-checked.
inline InterfaceReg* find_interface_regs(const char* module)
{
    HMODULE h = GetModuleHandleA(module);
    if (!h) return nullptr;
    auto fn = reinterpret_cast<std::uint8_t*>(GetProcAddress(h, "CreateInterface"));
    if (!fn) return nullptr;
    if (mem_readable(fn, 5) && fn[0] == 0xE9)
        fn = fn + 5 + *reinterpret_cast<std::int32_t*>(fn + 1);

    for (int i = 0; i < 160; i++)
    {
        if (!mem_readable(fn + i, 7)) break;
        const std::uint8_t rex = fn[i], op = fn[i + 1], modrm = fn[i + 2];
        if ((rex == 0x48 || rex == 0x4C) && (op == 0x8B || op == 0x8D) && (modrm & 0xC7) == 0x05)
        {
            const std::int32_t disp = *reinterpret_cast<std::int32_t*>(fn + i + 3);
            const auto global = reinterpret_cast<std::uintptr_t>(fn + i + 7) + disp;
            if (!mem_readable(reinterpret_cast<void*>(global), sizeof(void*)))
                continue;
            auto head = *reinterpret_cast<InterfaceReg**>(global);
            if (looks_like_reg(head))
                return head;
        }
    }
    return nullptr;
}

inline void* get_interface(const char* module, const char* prefix)
{
    const std::size_t plen = std::strlen(prefix);
    int guard = 0;
    for (InterfaceReg* reg = find_interface_regs(module); reg && looks_like_reg(reg) && guard < 512; reg = reg->m_pNext, ++guard)
        if (std::strncmp(reg->m_pName, prefix, plen) == 0 && reg->m_CreateFn)
            return reg->m_CreateFn();
    return nullptr;
}

// Diagnostics: list registered interface names. Returns count.
inline int list_interfaces(const char* module, const char** out, int max)
{
    int n = 0, guard = 0;
    for (InterfaceReg* reg = find_interface_regs(module); reg && looks_like_reg(reg) && n < max && guard < 512; reg = reg->m_pNext, ++guard)
        out[n++] = reg->m_pName;
    return n;
}
