#include "glowhook.h"
#include "settings.h"
#include "signatures.h"
#include "cs2_offsets.h"
#include "interfaces.h"
#include "kiero/minhook/include/MinHook.h"

#include <Windows.h>
#include <cstdint>

// =============================================================================
//  Flicker-free glow, ported from the working "velocity" cheat.
//
//  The old approach (write CGlowProperty fields every Present frame) races the
//  game's own per-frame glow recompute -> flicker. velocity instead HOOKS the
//  game's own two glow queries and answers them: the game asks us whether/what
//  color an entity glows on every query, so our value can never be overwritten.
//
//    bool CGlowProperty::IsGlowing(glowprop)         -> force true for enemies
//    void CGlowProperty::GetGlowColor(glowprop,float*)-> fill our color
//
//  The owning entity is at glowprop + 0x18 (velocity cheat.cpp:515/541).
// =============================================================================

using IsGlowing_t    = bool(__fastcall*)(std::uintptr_t glow);
using GetGlowColor_t = void(__fastcall*)(std::uintptr_t glow, float* color);

namespace
{
    // velocity patterns (its DSL translated to find + resolve_call):
    //  is_glowing     "0000488BEA488BF9 >E8???????? 4533F684C0"  (E8 at +8)
    //  get_glow_color ">E8???????? F30F10BE???????? 488BCF"       (E8 at +0)
    constexpr const char* kIsGlowingSig =
        "00 00 48 8B EA 48 8B F9 E8 ? ? ? ? 45 33 F6 84 C0";
    constexpr int kIsGlowingCallOff = 8;
    constexpr const char* kGetGlowColorSig =
        "E8 ? ? ? ? F3 0F 10 BE ? ? ? ? 48 8B CF";
    constexpr int kGetGlowColorCallOff = 0;

    constexpr std::ptrdiff_t kGlowOwner = 0x18; // CGlowProperty -> owner entity

    IsGlowing_t    oIsGlowing = nullptr;
    GetGlowColor_t oGetGlowColor = nullptr;
    void* gIsGlowingTarget = nullptr;
    void* gGetGlowColorTarget = nullptr;
    volatile bool gActive = false;
    std::uintptr_t gClientBase = 0;

    template <typename T>
    inline T rd(std::uintptr_t a)
    {
        if (!mem_readable(reinterpret_cast<void*>(a), sizeof(T))) return T{};
        return *reinterpret_cast<T*>(a);
    }

    // Enemy = valid pawn, team 2/3, not our team, alive.
    bool is_enemy(std::uintptr_t owner)
    {
        if (owner < 0x10000) return false;
        const int team = rd<std::uint8_t>(owner + cs2::schema::m_iTeamNum);
        if (team != 2 && team != 3) return false;

        const auto local = gClientBase ? rd<std::uintptr_t>(gClientBase + cs2::client::LocalPlayerPawn) : 0;
        const int lt = local ? rd<std::uint8_t>(local + cs2::schema::m_iTeamNum) : -1;
        if (team == lt) return false;

        const int hp = rd<int>(owner + cs2::schema::m_iHealth);
        if (hp <= 0 || hp > 100) return false;
        return true;
    }

    bool __fastcall hkIsGlowing(std::uintptr_t glow)
    {
        if (settings::visuals::glow && glow)
        {
            const auto owner = rd<std::uintptr_t>(glow + kGlowOwner);
            if (is_enemy(owner)) return true;
        }
        return oIsGlowing(glow);
    }

    void __fastcall hkGetGlowColor(std::uintptr_t glow, float* color)
    {
        if (settings::visuals::glow && glow && color)
        {
            const auto owner = rd<std::uintptr_t>(glow + kGlowOwner);
            if (is_enemy(owner))
            {
                color[0] = settings::visuals::glow_color[0];
                color[1] = settings::visuals::glow_color[1];
                color[2] = settings::visuals::glow_color[2];
                color[3] = settings::visuals::glow_color[3];
                return;
            }
        }
        oGetGlowColor(glow, color);
    }
}

bool glowhook::init()
{
    if (gActive) return true;

    gClientBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("client.dll"));
    if (!gClientBase) return false;

    const auto is_g = sig::find("client.dll", kIsGlowingSig);
    const auto get_c = sig::find("client.dll", kGetGlowColorSig);
    if (is_g)  gIsGlowingTarget    = reinterpret_cast<void*>(sig::resolve_call(is_g + kIsGlowingCallOff));
    if (get_c) gGetGlowColorTarget = reinterpret_cast<void*>(sig::resolve_call(get_c + kGetGlowColorCallOff));

    bool any = false;
    if (gIsGlowingTarget &&
        MH_CreateHook(gIsGlowingTarget, reinterpret_cast<void*>(&hkIsGlowing),
            reinterpret_cast<void**>(&oIsGlowing)) == MH_OK &&
        MH_EnableHook(gIsGlowingTarget) == MH_OK) any = true;
    if (gGetGlowColorTarget &&
        MH_CreateHook(gGetGlowColorTarget, reinterpret_cast<void*>(&hkGetGlowColor),
            reinterpret_cast<void**>(&oGetGlowColor)) == MH_OK &&
        MH_EnableHook(gGetGlowColorTarget) == MH_OK) any = true;

    gActive = any;
    return gActive;
}

void glowhook::shutdown()
{
    if (gIsGlowingTarget) { MH_DisableHook(gIsGlowingTarget); MH_RemoveHook(gIsGlowingTarget); gIsGlowingTarget = nullptr; }
    if (gGetGlowColorTarget) { MH_DisableHook(gGetGlowColorTarget); MH_RemoveHook(gGetGlowColorTarget); gGetGlowColorTarget = nullptr; }
    oIsGlowing = nullptr;
    oGetGlowColor = nullptr;
    gClientBase = 0;
    gActive = false;
}

bool glowhook::active() { return gActive; }
