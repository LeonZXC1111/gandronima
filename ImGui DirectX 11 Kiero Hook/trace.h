#pragma once
//
// trace.h - Source 2 ray/hull tracing for the surf features (PixelSurf,
// TextureBug). Resolves the game's trace manager + TraceShape by signature and
// exposes a small bbox-trace API. If the signatures don't resolve on this build,
// ready() stays false and the surf features simply don't activate (no crash).
//
// Reference logic: functions needed/px.txt and TextureBug + PixelSurf.txt
// (CTraceFilter(0x1C3003, pawn, layer, 11) + TracePlayerBBox / TraceShape).
//
#include <cstdint>

namespace trace
{
    struct Vec3 { float x = 0, y = 0, z = 0; };

    struct Result
    {
        float fraction   = 1.0f;
        bool  start_solid = false;
        bool  all_solid   = false;
        Vec3  normal{};
        Vec3  end_pos{};
    };

    // Resolve the trace manager + TraceShape. Safe to call repeatedly.
    bool ready();

    // Hull trace from `start` to `end` with the given AABB (mins/maxs) against the
    // world + players, filtering out `skip_pawn`. `mask` is the interaction mask
    // (0x1C3003 for player movement). Returns a filled Result; on any fault or
    // unresolved sig it returns the default (fraction = 1, nothing hit).
    Result player_bbox(const Vec3& start, const Vec3& end,
                       const Vec3& mins, const Vec3& maxs,
                       void* skip_pawn, std::uint64_t mask);
}
