#pragma once
//
// glowhook.h - enemy glow via a detour on the game's native glow-draw function.
// Modifies the CGlowProperty the renderer passes in (flicker-free, no per-frame
// memory churn), the way the proven Andromeda base does it.
//
namespace glowhook
{
    bool init();      // install the DrawGlow detour (MinHook)
    void shutdown();
    bool active();
}
