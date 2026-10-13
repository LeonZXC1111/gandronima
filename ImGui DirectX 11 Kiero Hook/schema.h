#pragma once
#include <cstdint>

namespace schema
{
    // Инициализировать SchemaSystem. Вызвать ОДИН раз при старте DLL.
    bool initialize();

    // Получить оффсет поля через SchemaSystem. Возвращает -1, если не найдено.
    int32_t offset(const char* module_name, const char* class_name, const char* field_name);

    // Резолвит через SchemaSystem; если не вышло — использует fallback.
    int32_t resolve_or_default(const char* module_name, const char* class_name,
        const char* field_name, int32_t fallback);

    namespace client
    {
        int32_t m_iHealth();
        int32_t m_iTeamNum();
        int32_t m_fFlags();
        int32_t m_vecVelocity();
        int32_t m_pGameSceneNode();
        int32_t m_nActualMoveType();
        int32_t m_vecAbsVelocity();
        int32_t m_vOldOrigin();

        int32_t m_pMovementServices();
        int32_t m_nTickBase();

        int32_t m_vecAbsOrigin();

        int32_t m_Glow();
        int32_t m_glowColorOverride();
        int32_t m_bGlowing();

        int32_t m_hPlayerPawn();

        int32_t m_ModernJump();
        int32_t m_flLastLandedFrac();
    }
}