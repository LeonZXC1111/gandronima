#include "schema.h"
#include "D:/movement gandronima/movement gandronima/ImGui DirectX 11 Kiero Hook/sdk/schema_runtime.hpp"
#include "cs2_offsets.h"

namespace schema
{
    static bool g_initialized = false;

    bool initialize()
    {
        if (g_initialized) return true;
        void* sys = sdk::schema::get_schema_system();
        if (!sys) return false;
        g_initialized = true;
        return true;
    }

    int32_t offset(const char* module_name, const char* class_name, const char* field_name)
    {
        return sdk::schema::resolve_offset(module_name, class_name, field_name);
    }

    int32_t resolve_or_default(const char* module_name, const char* class_name,
        const char* field_name, int32_t fallback)
    {
        const int32_t off = offset(module_name, class_name, field_name);
        return (off > 0) ? off : fallback;
    }

    namespace client
    {
#define DEF_FIELD(func_name, class_name, field_name, fallback)              \
            int32_t func_name()                                                     \
            {                                                                       \
                static const int32_t v = resolve_or_default(                        \
                    "client.dll", class_name, field_name, (int32_t)fallback);       \
                return v;                                                           \
            }

        DEF_FIELD(m_iHealth, "C_BaseEntity", "m_iHealth", cs2::schema::m_iHealth)
            DEF_FIELD(m_iTeamNum, "C_BaseEntity", "m_iTeamNum", cs2::schema::m_iTeamNum)
            DEF_FIELD(m_fFlags, "C_BaseEntity", "m_fFlags", cs2::schema::m_fFlags)
            DEF_FIELD(m_vecVelocity, "C_BaseEntity", "m_vecVelocity", cs2::schema::m_vecVelocity)
            DEF_FIELD(m_pGameSceneNode, "C_BaseEntity", "m_pGameSceneNode", cs2::schema::m_pGameSceneNode)
            DEF_FIELD(m_nActualMoveType, "C_BaseEntity", "m_nActualMoveType", cs2::schema::m_nActualMoveType)
            DEF_FIELD(m_vecAbsVelocity, "C_BaseEntity", "m_vecAbsVelocity", cs2::schema::m_vecAbsVelocity)
            DEF_FIELD(m_vOldOrigin, "C_BaseEntity", "m_vOldOrigin", cs2::schema::m_vOldOrigin)

            DEF_FIELD(m_pMovementServices, "C_BasePlayerPawn", "m_pMovementServices", cs2::schema::m_pMovementServices)
            DEF_FIELD(m_nTickBase, "CBasePlayerController", "m_nTickBase", cs2::schema::m_nTickBase)

            DEF_FIELD(m_vecAbsOrigin, "CGameSceneNode", "m_vecAbsOrigin", cs2::schema::m_vecAbsOrigin)

            DEF_FIELD(m_Glow, "C_BaseModelEntity", "m_Glow", cs2::schema::m_Glow)
            DEF_FIELD(m_glowColorOverride, "CGlowProperty", "m_glowColorOverride", cs2::schema::m_glowColorOverride)
            DEF_FIELD(m_bGlowing, "CGlowProperty", "m_bGlowing", cs2::schema::m_bGlowing)

            DEF_FIELD(m_hPlayerPawn, "CCSPlayerController", "m_hPlayerPawn", cs2::schema::m_hPlayerPawn)

            DEF_FIELD(m_ModernJump, "CCSPlayer_MovementServices", "m_ModernJump", cs2::schema::m_ModernJump)
            DEF_FIELD(m_flLastLandedFrac, "CCSPlayerModernJump", "m_flLastLandedFrac", cs2::schema::ModernJump_flLastLandedFrac)

#undef DEF_FIELD
    }
}