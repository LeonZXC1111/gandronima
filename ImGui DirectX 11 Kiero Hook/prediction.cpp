#include "prediction.h"
#include "cs2_offsets.h"
#include "interfaces.h"
#include "schema.h"
#include "signatures.h"
#include "debuglog.h"

#include <Windows.h>
#include <cstdint>
#include <cmath>

// =============================================================================
//  Engine prediction, ported from the working "velocity" cheat. Our old vtable
//  (SetCmd/RunCommand/ResetCmd) approach FAULTED because RunCommand needs the
//  prediction context set up first. velocity enters prediction via sig'd engine
//  functions + globals instead:
//      write simulation_player = pawn; write prediction_player = pawn;
//      set_pawn(guard, pawn); set_state(pred_state, 1);
//      run_command(movement_services, cmd)  x N ticks
//      set_state(pred_state, 0); reset_pawn(guard);
//  then restore. SEH-guarded; if any sig fails to resolve it stays disabled.
// =============================================================================

namespace
{
    // velocity patterns (protection/patterns.cpp), '??'->'?', '>E8' = resolve_call,
    // '*' RIP-relative = resolve_rip.
    constexpr const char* kSetPawnSig    = "48 89 5C 24 ? 57 48 83 EC 20 48 C7 01 00 00 00 00 48 8B FA 48 8B D9 48 85 D2 74 ? 48 8B 02";
    constexpr const char* kSetStateSig   = "8B 81 ? ? ? ? 84 D2 74 ? FF C0 89 81 ? ? ? ? C3 83 E8 01 89 81 ? ? ? ? 75 ? 80 B9";
    constexpr const char* kResetPawnSig  = "48 8B 11 48 85 D2 74 ? 80 6A ? ? 75 ? 48 8B 05 ? ? ? ? 8B 40 44 89 42 18";
    constexpr const char* kRunCmdSig     = "8B C5 49 8B D5 48 8B CB E8 ? ? ? ? 48 8B 03 4C 8B C5"; // >E8 at +8
    constexpr const char* kPredStateSig  = "48 8B 0D ? ? ? ? 33 D2 8B 5B 38";                        // mov rcx,[rip] @+3
    constexpr const char* kSimPlayerSig  = "4C 39 05 ? ? ? ? 41 0F 94 C6";                            // cmp [rip],r8 @+3
    constexpr const char* kPredPlayerSig = "48 8B 43 38 48 89 05 ? ? ? ? 41 83 FC 03";                // mov [rip],rax @+4(+3)

    using SetPawn_t    = void(__fastcall*)(void* guard, std::uintptr_t pawn);
    using SetState_t   = void(__fastcall*)(std::uintptr_t pred_state, std::uint8_t on);
    using ResetPawn_t  = void(__fastcall*)(void* guard);
    using RunCommand_t = void(__fastcall*)(std::uintptr_t movement_services, void* cmd);

    SetPawn_t    g_set_pawn = nullptr;
    SetState_t   g_set_state = nullptr;
    ResetPawn_t  g_reset_pawn = nullptr;
    RunCommand_t g_run_command = nullptr;
    std::uintptr_t g_pred_state = 0;      // the prediction-state object
    std::uintptr_t g_sim_player_addr = 0; // &simulation_player global
    std::uintptr_t g_pred_player_addr = 0;// &prediction_player global

    bool g_resolved = false;
    bool g_disabled = false;

    inline std::uintptr_t client() { return reinterpret_cast<std::uintptr_t>(GetModuleHandleA("client.dll")); }
    inline bool ptr_ok(std::uintptr_t a) { return a > 0x10000 && a < 0x7FFFFFFFFFFFull; }

    template <typename T> inline T rd(std::uintptr_t a) { if (!ptr_ok(a)) return T{}; return *reinterpret_cast<T*>(a); }
    template <typename T> inline void wr(std::uintptr_t a, const T& v) { if (ptr_ok(a)) *reinterpret_cast<T*>(a) = v; }

    void resolve()
    {
        if (g_resolved || g_disabled) return;
        g_resolved = true;

        std::uintptr_t base = 0; std::size_t size = 0;
        if (!sig::module_range("client.dll", base, size)) { g_disabled = true; return; }
        auto in_mod = [&](std::uintptr_t a) { return a >= base && a < base + size; };

        const auto sp  = sig::find("client.dll", kSetPawnSig);
        const auto ss  = sig::find("client.dll", kSetStateSig);
        const auto rp  = sig::find("client.dll", kResetPawnSig);
        const auto rc  = sig::find("client.dll", kRunCmdSig);
        const auto ps  = sig::find("client.dll", kPredStateSig);
        const auto simp = sig::find("client.dll", kSimPlayerSig);
        const auto pp  = sig::find("client.dll", kPredPlayerSig);

        if (sp)  g_set_pawn    = reinterpret_cast<SetPawn_t>(sp);
        if (ss)  g_set_state   = reinterpret_cast<SetState_t>(ss);
        if (rp)  g_reset_pawn  = reinterpret_cast<ResetPawn_t>(rp);
        if (rc)  g_run_command = reinterpret_cast<RunCommand_t>(sig::resolve_call(rc + 8));
        if (ps)  { const auto g = sig::resolve_rip(ps, 3, 7); if (in_mod(g)) g_pred_state = rd<std::uintptr_t>(g); }
        if (simp) g_sim_player_addr  = sig::resolve_rip(simp, 3, 7);
        if (pp)   g_pred_player_addr = sig::resolve_rip(pp + 4, 3, 7);

        // Everything must resolve inside client.dll, or prediction stays off (safe).
        if (!g_set_pawn || !g_set_state || !g_reset_pawn || !g_run_command || !g_pred_state
            || !in_mod(reinterpret_cast<std::uintptr_t>(g_set_pawn))
            || !in_mod(reinterpret_cast<std::uintptr_t>(g_run_command))
            || !in_mod(g_sim_player_addr) || !in_mod(g_pred_player_addr))
        {
            g_disabled = true;
        }
        dbg::log("[prediction] resolve setPawn=%llX setState=%llX reset=%llX run=%llX predState=%llX disabled=%d\n",
            (unsigned long long)sp, (unsigned long long)ss, (unsigned long long)rp,
            (unsigned long long)(g_run_command ? (std::uintptr_t)g_run_command : 0),
            (unsigned long long)g_pred_state, g_disabled ? 1 : 0);
    }

    struct Vec3 { float x, y, z; };
}

bool prediction::ready()
{
    resolve();
    if (g_disabled) return false;
    const auto c = client(); if (!c) return false;
    const auto pawn = rd<std::uintptr_t>(c + cs2::client::LocalPlayerPawn);
    return pawn != 0;
}

prediction::Result prediction::run(void* cmd, int ticks)
{
    Result out;
    if (!ready() || !cmd) return out;

    const auto c = client();
    const auto pawn = rd<std::uintptr_t>(c + cs2::client::LocalPlayerPawn);
    const auto controller = rd<std::uintptr_t>(c + cs2::client::LocalPlayerController);
    if (!pawn || !controller) return out;
    const auto mv = rd<std::uintptr_t>(pawn + schema::client::m_pMovementServices());
    if (!mv) return out;

    // ---- BACKUP ----
    const Vec3 velBk = rd<Vec3>(pawn + schema::client::m_vecVelocity());
    const Vec3 absVelBk = rd<Vec3>(pawn + schema::client::m_vecAbsVelocity());
    const Vec3 originBk = rd<Vec3>(pawn + schema::client::m_vOldOrigin());
    const std::uint32_t flagsBk = rd<std::uint32_t>(pawn + schema::client::m_fFlags());
    const std::uint32_t tickBk = rd<std::uint32_t>(controller + schema::client::m_nTickBase());
    const float staminaBk = rd<float>(mv + cs2::schema::m_flStamina);
    const std::uint32_t lastJumpBk = rd<std::uint32_t>(mv + cs2::schema::mv_nLastJumpTick);
    const std::uintptr_t simBk = rd<std::uintptr_t>(g_sim_player_addr);
    const std::uintptr_t predBk = rd<std::uintptr_t>(g_pred_player_addr);

    const auto gv = rd<std::uintptr_t>(c + cs2::client::GlobalVars);
    float gv48 = 0, gv52 = 0, gv80 = 0; int gv68 = 0; std::uint32_t gv88 = 0;
    if (gv) { gv48 = rd<float>(gv + 48); gv52 = rd<float>(gv + 52); gv68 = rd<int>(gv + 68); gv80 = rd<float>(gv + 80); gv88 = rd<std::uint32_t>(gv + 88); }

    std::uintptr_t pawn_guard[1]{};
    __try
    {
        wr<std::uintptr_t>(g_sim_player_addr, pawn);
        wr<std::uintptr_t>(g_pred_player_addr, pawn);
        g_set_pawn(pawn_guard, pawn);
        g_set_state(g_pred_state, 1);

        float prev_z = absVelBk.z;
        for (int i = 0; i < ticks && i < 64; ++i)
        {
            g_run_command(mv, cmd);
            const Vec3 absv = rd<Vec3>(pawn + schema::client::m_vecAbsVelocity());
            out.velx = absv.x; out.vely = absv.y; out.velz = absv.z;
            out.onground = (rd<std::uint32_t>(pawn + schema::client::m_fFlags()) & cs2::FL_ONGROUND) != 0;
            out.land_frac = rd<float>(mv + schema::client::m_ModernJump() + schema::client::m_flLastLandedFrac());
            if (!out.edge_hit && !out.onground && prev_z < -7.0f && std::floor(out.velz) == -7.0f) { out.edge_hit = true; out.edge_ticks = i + 1; }
            if (!out.will_land && out.onground) { out.will_land = true; if (out.edge_ticks == 0) out.edge_ticks = i + 1; }
            prev_z = out.velz;
        }

        g_set_state(g_pred_state, 0);
        g_reset_pawn(pawn_guard);
        out.valid = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_disabled = true;
        dbg::log("[prediction] FAULTED in run - disabled (velocity sig path)\n");
    }

    // ---- RESTORE ----
    wr<Vec3>(pawn + schema::client::m_vecVelocity(), velBk);
    wr<Vec3>(pawn + schema::client::m_vecAbsVelocity(), absVelBk);
    wr<Vec3>(pawn + schema::client::m_vOldOrigin(), originBk);
    wr<std::uint32_t>(pawn + schema::client::m_fFlags(), flagsBk);
    wr<std::uint32_t>(controller + schema::client::m_nTickBase(), tickBk);
    wr<float>(mv + cs2::schema::m_flStamina, staminaBk);
    wr<std::uint32_t>(mv + cs2::schema::mv_nLastJumpTick, lastJumpBk);
    wr<std::uintptr_t>(g_sim_player_addr, simBk);
    wr<std::uintptr_t>(g_pred_player_addr, predBk);
    if (gv) { wr<float>(gv + 48, gv48); wr<float>(gv + 52, gv52); wr<int>(gv + 68, gv68); wr<float>(gv + 80, gv80); wr<std::uint32_t>(gv + 88, gv88); }
    return out;
}
