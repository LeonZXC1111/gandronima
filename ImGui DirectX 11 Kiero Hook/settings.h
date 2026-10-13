#pragma once
//
// settings.h - all feature toggles live here.
//
#include <Windows.h>
#include "binds.h"

namespace settings
{
    namespace visuals
    {
        inline bool  box  = false;
        inline float box_color[4]  = { 0.78f, 0.20f, 0.72f, 1.0f };

        inline bool  glow = false;
        inline float glow_color[4] = { 0.78f, 0.20f, 0.72f, 1.0f };

        inline bool  skeleton = false;
        inline float skeleton_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        inline bool  name = false;
        inline float name_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

        inline bool  healthbar = false;
        inline bool  hp_gradient = false;                          // off = single colour
        inline float hp_color[4]  = { 0.10f, 0.90f, 0.20f, 1.0f }; // single-colour mode
        inline float hp_top[4]    = { 0.10f, 0.90f, 0.20f, 1.0f }; // gradient full (top)
        inline float hp_bottom[4] = { 0.90f, 0.10f, 0.10f, 1.0f }; // gradient low (bottom)
    }

    namespace movement
    {
        inline Bind bhop;
        inline Bind autostrafe;
        inline Bind edgebug;
        inline Bind smart_edgebug;
        inline Bind jumpbug;
        inline Bind pixelsurf;
        inline Bind texturebug;
    }
}
