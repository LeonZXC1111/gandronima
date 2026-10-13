#include "trace.h"
#include "signatures.h"
#include "interfaces.h"
#include "schema.h"
#include "debuglog.h"
#include "cs2_offsets.h"

#include <Windows.h>
#include <cstdint>
#include <cstring>

// =============================================================================
//  Trace, ported 1:1 from the working "velocity" cheat (systems/impl/tracing.cpp).
//  Our old approach (a free TracePlayerBBox with the global trace manager) was the
//  wrong function/ABI and faulted every call. velocity does it like this:
//
//    filter = {};                                   // player_movement_filter, 0x48 bytes
//    trace_filter_set_collision(&filter, pawn, mask, 11);
//    result = {};                                   // CGameTrace
//    trace_hull(movement_services + 0x638, &result, &start, &end, &bbox, &filter);
//
//  i.e. the hull trace is a METHOD on (movement_services + 0x638), the OUT result
//  is the 2nd arg, and the bbox is one {mins,maxs} struct. CGameTrace field offsets
//  are from velocity's `tracing::result`.
// =============================================================================

namespace
{
    // velocity patterns (protection/patterns.cpp), translated to this project's
    // byte-scanner + resolve_call. '>' call-targets become find+resolve_call.
    //  trace_filter_set_collision: "000041B800010000 >E8???????? 4C397F38"
    constexpr const char* kFilterSetCollisionSig =
        "00 00 41 B8 00 01 00 00 E8 ? ? ? ? 4C 39 7F 38";
    constexpr int kFilterSetCollisionCallOff = 8; // E8 is 8 bytes in
    //  trace_hull: ">E8???????? 0F2F754C"
    constexpr const char* kTraceHullSig =
        "E8 ? ? ? ? 0F 2F 75 4C";
    constexpr int kTraceHullCallOff = 0;

    // Trace context lives inside CCSPlayer_MovementServices.
    constexpr std::ptrdiff_t kTraceCtxOffset = 0x638; // 1592

    // CGameTrace (velocity tracing::result) field offsets.
    constexpr std::ptrdiff_t kTrace_hitEntity = 0x08;
    constexpr std::ptrdiff_t kTrace_endPos = 0x84;
    constexpr std::ptrdiff_t kTrace_normal = 0x90;
    constexpr std::ptrdiff_t kTrace_fraction = 0xAC;
    constexpr std::ptrdiff_t kTrace_allSolid = 0xB6;

    // trace_filter_set_collision(player_movement_filter* out, pawn, mask, group)
    using FilterSetCollision_t = void(__fastcall*)(void* filter, std::uintptr_t entity,
        std::uint64_t mask, int collision_group);
    // trace_hull(ctx = movement_services+0x638, CGameTrace* out, Vector* start,
    //            Vector* end, bbox{mins,maxs}* bbox, player_movement_filter* filter)
    using TraceHull_t = void(__fastcall*)(std::uintptr_t ctx, void* out,
        const void* start, const void* end, const void* bbox, const void* filter);

    FilterSetCollision_t g_filter_set = nullptr;
    TraceHull_t          g_trace_hull = nullptr;
    bool                 g_resolved = false;
    bool                 g_disabled = false;

    inline std::uintptr_t client()
    {
        return reinterpret_cast<std::uintptr_t>(GetModuleHandleA("client.dll"));
    }

    void resolve()
    {
        if (g_resolved || g_disabled) return;

        const auto f = sig::find("client.dll", kFilterSetCollisionSig);
        if (f) g_filter_set = reinterpret_cast<FilterSetCollision_t>(
            sig::resolve_call(f + kFilterSetCollisionCallOff));

        const auto t = sig::find("client.dll", kTraceHullSig);
        if (t) g_trace_hull = reinterpret_cast<TraceHull_t>(
            sig::resolve_call(t + kTraceHullCallOff));

        g_resolved = true;
        dbg::log("[trace] resolve filterSet=%llX traceHull=%llX\n",
            (unsigned long long)(std::uintptr_t)g_filter_set,
            (unsigned long long)(std::uintptr_t)g_trace_hull);
    }
}

bool trace::ready()
{
    resolve();
    return !g_disabled && g_filter_set && g_trace_hull;
}

trace::Result trace::player_bbox(const Vec3& start, const Vec3& end,
    const Vec3& mins, const Vec3& maxs,
    void* skip_pawn, std::uint64_t mask)
{
    Result out;
    if (!ready() || !skip_pawn) return out;

    const auto pawn = reinterpret_cast<std::uintptr_t>(skip_pawn);
    const auto ms = *reinterpret_cast<std::uintptr_t*>(pawn + schema::client::m_pMovementServices());
    if (ms < 0x10000) return out;

    const float s[3] = { start.x, start.y, start.z };
    const float e[3] = { end.x,   end.y,   end.z };
    const float bbox[6] = { mins.x, mins.y, mins.z, maxs.x, maxs.y, maxs.z };

    alignas(16) std::uint8_t filter[0x48]{};
    alignas(16) std::uint8_t result[0x180]{};

    __try
    {
        g_filter_set(filter, pawn, mask, 11); // 11 = COLLISION_GROUP_PLAYER_MOVEMENT
        g_trace_hull(ms + kTraceCtxOffset, result, s, e, bbox, filter);

        const auto t = reinterpret_cast<std::uintptr_t>(result);
        out.fraction = *reinterpret_cast<float*>(t + kTrace_fraction);
        out.all_solid = *reinterpret_cast<bool*>(t + kTrace_allSolid);
        out.start_solid = false; // velocity's CGameTrace maps only all_solid
        out.normal.x = *reinterpret_cast<float*>(t + kTrace_normal + 0);
        out.normal.y = *reinterpret_cast<float*>(t + kTrace_normal + 4);
        out.normal.z = *reinterpret_cast<float*>(t + kTrace_normal + 8);
        out.end_pos.x = *reinterpret_cast<float*>(t + kTrace_endPos + 0);
        out.end_pos.y = *reinterpret_cast<float*>(t + kTrace_endPos + 4);
        out.end_pos.z = *reinterpret_cast<float*>(t + kTrace_endPos + 8);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_disabled = true;
        out = Result{};
        dbg::log("[trace] player_bbox faulted - tracing disabled (verify trace_hull sig / ctx 0x638)\n");
    }
    return out;
}
