#pragma once
//
// signatures.h - IDA-style AOB pattern scanner for x64 modules.
//
// Resolves addresses at runtime from the signatures in patterns.txt instead of
// relying on hardcoded offsets that break on every game update. Header-only to
// match the rest of the project.
//
#include <Windows.h>
#include <Psapi.h>
#include <cstdint>
#include <string_view>
#include <vector>

#pragma comment(lib, "psapi.lib")

namespace sig
{
    // Returns [base, size] of a loaded module's image, or {0,0} if not loaded.
    inline bool module_range(const char* module_name, std::uintptr_t& out_base, std::size_t& out_size)
    {
        HMODULE mod = GetModuleHandleA(module_name);
        if (!mod)
            return false;

        MODULEINFO info{};
        if (!GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info)))
            return false;

        out_base = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
        out_size = info.SizeOfImage;
        return true;
    }

    // Parse an IDA-style signature ("48 8B ? ? 90") into bytes + mask.
    // '?' or '??' is a wildcard byte.
    inline void parse(std::string_view pattern, std::vector<std::uint8_t>& bytes, std::vector<bool>& mask)
    {
        for (std::size_t i = 0; i < pattern.size();)
        {
            const char c = pattern[i];
            if (c == ' ')
            {
                ++i;
                continue;
            }
            if (c == '?')
            {
                bytes.push_back(0);
                mask.push_back(false);          // wildcard
                i += (i + 1 < pattern.size() && pattern[i + 1] == '?') ? 2 : 1;
                continue;
            }

            // Two hex nibbles -> one byte.
            auto nibble = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return -1;
            };
            const int hi = nibble(pattern[i]);
            const int lo = (i + 1 < pattern.size()) ? nibble(pattern[i + 1]) : -1;
            if (hi < 0 || lo < 0)   // malformed; skip the char
            {
                ++i;
                continue;
            }
            bytes.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
            mask.push_back(true);
            i += 2;
        }
    }

    // Scan a module for an IDA-style signature. Returns the address of the first
    // match, or 0 if not found.
    inline std::uintptr_t find(const char* module_name, std::string_view pattern)
    {
        std::uintptr_t base = 0;
        std::size_t size = 0;
        if (!module_range(module_name, base, size))
            return 0;

        std::vector<std::uint8_t> bytes;
        std::vector<bool> mask;
        parse(pattern, bytes, mask);
        if (bytes.empty())
            return 0;

        const auto* data = reinterpret_cast<const std::uint8_t*>(base);
        const std::size_t last = size - bytes.size();
        for (std::size_t i = 0; i <= last; ++i)
        {
            bool matched = true;
            for (std::size_t j = 0; j < bytes.size(); ++j)
            {
                if (mask[j] && data[i + j] != bytes[j])
                {
                    matched = false;
                    break;
                }
            }
            if (matched)
                return base + i;
        }
        return 0;
    }

    // Resolve a RIP-relative reference (e.g. the target of a `lea`/`mov`/`call`).
    // `addr` points at the start of the instruction; `offset_pos` is where the
    // 4-byte displacement begins; `instruction_size` is the full length so the
    // next-instruction address can be computed.
    inline std::uintptr_t resolve_rip(std::uintptr_t addr, int offset_pos, int instruction_size)
    {
        if (!addr)
            return 0;
        const std::int32_t disp = *reinterpret_cast<std::int32_t*>(addr + offset_pos);
        return addr + instruction_size + disp;
    }

    // Convenience: resolve the target of a relative CALL/JMP (E8/E9 xx xx xx xx).
    inline std::uintptr_t resolve_call(std::uintptr_t addr)
    {
        return resolve_rip(addr, 1, 5);
    }
}
