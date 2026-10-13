#pragma once
//
// game.h - runtime-resolved game state (globals via signatures, member offsets
// via the schema system). Build-independent as long as the signatures hold and
// the schema layout in schema.cpp is correct.
//
#include <cstdint>

namespace game
{
    struct Offsets
    {
        int m_iHealth;
        int m_iTeamNum;
        int m_fFlags;
        int m_pGameSceneNode;
        int m_vecAbsOrigin;
        int m_hPlayerPawn;
        int m_Glow;
        int m_glowColorOverride;
        int m_bGlowing;
    };

    bool init();          // resolve everything once (call after modules loaded)
    bool ready();
    const Offsets& off();

    std::uintptr_t entity_list();       // dereferenced GameEntitySystem
    std::uintptr_t local_controller();  // dereferenced local controller
    std::uintptr_t local_pawn();        // derived from the local controller
    float        (*view_matrix())[4][4];

    std::uintptr_t pawn_from_index(int index);   // 1..63
    std::uintptr_t entity_by_index(int index);   // via game's GetBaseEntity
    bool           has_get_base_entity();         // did the sig resolve? (diag)
    int            game_build();                 // engine build number (diag)
}
