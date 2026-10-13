#include "game.h"
#include "schema.h"
#include "signatures.h"
#include "cs2_offsets.h"
#include "interfaces.h"   // mem_readable

#include <Windows.h>
#include <cstdint>

namespace
{
    // client.dll signatures that reference the globals (RIP-relative).
    constexpr const char* SIG_ENTITY_LIST      = "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ?";
    constexpr const char* SIG_LOCAL_CONTROLLER = "48 8B 05 ? ? ? ? 41 89 BE";
    constexpr const char* SIG_GLOBAL_VARS      = "48 8B 0D ? ? ? ? 4C 8D 05 ? ? ? ? 48 85 D2";
    constexpr const char* SIG_VIEW_MATRIX      = "48 8D 0D ? ? ? ? 48 C1 E0 06";

    bool g_ready = false;
    game::Offsets g_off{};

    std::uintptr_t g_entityListAddr      = 0;
    std::uintptr_t g_localControllerAddr = 0;
    std::uintptr_t g_globalVarsAddr      = 0;
    std::uintptr_t g_viewMatrix          = 0;

    using GetBaseEntity_t = void* (__fastcall*)(void* entity_system, int index);
    GetBaseEntity_t g_getBaseEntity = nullptr;

    template <typename T>
    inline T rd(std::uintptr_t a)
    {
        // cheap validity check (no VirtualQuery - that tanks FPS in hot loops)
        if (a <= 0x10000 || a >= 0x7FFFFFFFFFFFull) return T{};
        return *reinterpret_cast<T*>(a);
    }

    inline std::uintptr_t client_base()
    {
        return reinterpret_cast<std::uintptr_t>(GetModuleHandleA("client.dll"));
    }

    // Resolve a global. On the CURRENT build the dump RVA is exact, so use it
    // first; the 14184 signatures could mis-resolve to a wrong address. The sig
    // is only a fallback for when the module base can't be read.
    std::uintptr_t resolve_global(const char* sig, std::ptrdiff_t rva)
    {
        const std::uintptr_t base = client_base();
        if (base) return base + rva;
        return sig::resolve_rip(sig::find("client.dll", sig), 3, 7);
    }
}

bool game::init()
{
    if (g_ready) return true;
    if (!client_base()) return false;

    // Member offsets, straight from the 14190 dump. These are stable across
    // builds; we deliberately do NOT use the runtime schema walk here (its
    // vtable indices are unverified and a wrong value corrupts writes/crashes).
    g_off.m_iHealth          = 0x34C;
    g_off.m_iTeamNum         = 0x3E7;
    g_off.m_fFlags           = 0x3F4;
    g_off.m_pGameSceneNode   = 0x330;
    g_off.m_vecAbsOrigin     = 0xC8;
    g_off.m_hPlayerPawn      = 0x92C;
    g_off.m_Glow             = 0xDE8;
    g_off.m_glowColorOverride = 0x40;
    g_off.m_bGlowing         = 0x51;

    static bool scanned = false;
    if (!scanned)
    {
        scanned = true;
        g_entityListAddr      = resolve_global(SIG_ENTITY_LIST,      cs2::client::EntityList);
        g_localControllerAddr = resolve_global(SIG_LOCAL_CONTROLLER, cs2::client::LocalPlayerController);
        g_globalVarsAddr      = resolve_global(SIG_GLOBAL_VARS,      cs2::client::GlobalVars);
        g_viewMatrix          = resolve_global(SIG_VIEW_MATRIX,      cs2::client::ViewMatrix);
        g_getBaseEntity       = reinterpret_cast<GetBaseEntity_t>(sig::find("client.dll", cs2::sigs::GetBaseEntity));
    }

    g_ready = g_entityListAddr && g_localControllerAddr && g_viewMatrix;
    return g_ready;
}

bool game::ready() { return g_ready; }
const game::Offsets& game::off() { return g_off; }

std::uintptr_t game::entity_list()      { return rd<std::uintptr_t>(g_entityListAddr); }
std::uintptr_t game::local_controller() { return rd<std::uintptr_t>(g_localControllerAddr); }
float        (*game::view_matrix())[4][4] { return reinterpret_cast<float(*)[4][4]>(g_viewMatrix); }

std::uintptr_t game::local_pawn()
{
    // The local pawn global is simplest read directly.
    const std::uintptr_t base = client_base();
    if (base)
    {
        const std::uintptr_t p = rd<std::uintptr_t>(base + cs2::client::LocalPlayerPawn);
        if (p) return p;
    }
    // fallback: via controller handle
    const std::uintptr_t ctrl = local_controller();
    if (!ctrl) return 0;
    const std::uint32_t h = rd<std::uint32_t>(ctrl + g_off.m_hPlayerPawn);
    if (!h || h == 0xFFFFFFFF) return 0;
    const std::uintptr_t el = entity_list();
    if (!el) return 0;
    const std::uintptr_t le = rd<std::uintptr_t>(el + 0x8 * ((h & 0x7FFF) >> 9) + 16);
    if (!le) return 0;
    return rd<std::uintptr_t>(le + 120 * (h & 0x1FF));
}

std::uintptr_t game::entity_by_index(int index)
{
    if (!g_getBaseEntity) return 0;
    void* es = reinterpret_cast<void*>(entity_list());   // CGameEntitySystem
    if (!es) return 0;
    return reinterpret_cast<std::uintptr_t>(g_getBaseEntity(es, index));
}

bool game::has_get_base_entity() { return g_getBaseEntity != nullptr; }

std::uintptr_t game::pawn_from_index(int i)
{
    // controller at entity index i -> its pawn handle -> pawn entity
    const std::uintptr_t ctrl = entity_by_index(i);
    if (!ctrl) return 0;
    const std::uint32_t h = rd<std::uint32_t>(ctrl + g_off.m_hPlayerPawn);
    if (!h || h == 0xFFFFFFFF) return 0;
    return entity_by_index(h & 0x7FFF);
}

int game::game_build()
{
    static int cached = -1;
    if (cached != -1) return cached;
    const std::uintptr_t eng = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("engine2.dll"));
    cached = eng ? rd<int>(eng + 0x61CFE8) : 0; // dwBuildNumber (14190 dump)
    return cached;
}
