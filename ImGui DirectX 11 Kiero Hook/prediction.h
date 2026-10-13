#pragma once
#include <cstdint>

namespace prediction
{
    struct Result
    {
        bool  valid = false;
        bool  onground = false;
        bool  edge_hit = false;
        bool  will_land = false;
        int   edge_ticks = 0;
        float velx = 0.0f;
        float vely = 0.0f;
        float velz = 0.0f;
        float land_frac = 0.0f; // Доля тика, в которой произойдёт приземление
    };

    bool ready();
    Result run(void* cmd, int ticks = 1);
}