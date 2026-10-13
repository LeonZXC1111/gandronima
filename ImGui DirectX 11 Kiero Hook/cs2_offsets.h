#pragma once
//
// cs2_offsets.h - resolved addresses for CS2 build 14190 (from sdk dump).
//
#include <cstddef>
#include <cstdint>

namespace cs2
{
    inline constexpr unsigned int kBuild = 14190;

    namespace client
    {
        constexpr std::ptrdiff_t GlobalVars = 0x2228090;
        constexpr std::ptrdiff_t EntityList = 0x2711598;
        constexpr std::ptrdiff_t LocalPlayerController = 0x25338A8;
        constexpr std::ptrdiff_t LocalPlayerPawn = 0x255C3C8;
        constexpr std::ptrdiff_t Prediction = 0x255C2D0;
        constexpr std::ptrdiff_t CSGOInput = 0x2572460;
        constexpr std::ptrdiff_t ViewAngles = 0x2572AE8;
        constexpr std::ptrdiff_t ViewMatrix = 0x2561CD0;
    }

    namespace engine2
    {
        constexpr std::ptrdiff_t NetworkGameClient = 0x91AFC0;
        constexpr std::ptrdiff_t NetworkGameClient_deltaTick = 0x24C;
        constexpr std::ptrdiff_t NetworkGameClient_serverTickCount = 0x24C;
        constexpr std::ptrdiff_t NetworkGameClient_clientTickCount = 0x398;
        constexpr std::ptrdiff_t NetworkGameClient_signOnState = 0x230;
    }

    // RVAs of important functions (build 14190, client.dll).
    namespace rva
    {
        constexpr std::ptrdiff_t CInButtonStatePB_New = 0x4F70A0;
    }

    namespace sigs
    {
        constexpr const char* CreateMove =
            "48 8B C4 4C 89 40 ? 48 89 48 ? 55 53 57";

        constexpr const char* MouseInputEnabled =
            "40 53 48 83 EC 20 80 B9 ? ? ? ? ? 48 8B D9 75 78";

        constexpr const char* DrawGlow =
            "40 53 48 83 EC 20 48 8B 54";

        constexpr const char* GetBaseEntity =
            "4C 8D 49 10 81 FA ? ? 00 00 77 ? 8B CA C1 F9 09";

        // Appends a new sub-tick move step to a usercmd and returns it.
        // Начало самой функции (не call-site). Размер объекта 0x38.
        constexpr const char* CreateNewSubtickMoveStep =
            "48 89 5C 24 10 57 48 83 EC 20 33 DB 48 8B F9 48 85 C9 75 2D B9 38 00 00 00";
    }

    namespace schema
    {
        // C_BaseEntity
        constexpr std::ptrdiff_t m_iHealth = 0x34C;
        constexpr std::ptrdiff_t m_iTeamNum = 0x3E7;
        constexpr std::ptrdiff_t m_fFlags = 0x3F4;
        constexpr std::ptrdiff_t m_vecVelocity = 0x430;
        constexpr std::ptrdiff_t m_pGameSceneNode = 0x330;
        constexpr std::ptrdiff_t m_nActualMoveType = 0x526;
        constexpr std::ptrdiff_t m_Glow = 0xDE8;
        constexpr std::ptrdiff_t m_glowColorOverride = 0x40;
        constexpr std::ptrdiff_t m_bGlowing = 0x51;
        constexpr std::ptrdiff_t m_vecAbsVelocity = 0x3F8;
        constexpr std::ptrdiff_t m_vOldOrigin = 0x14A4;
        constexpr std::ptrdiff_t m_pWeaponServices = 0x12F0;
        constexpr std::ptrdiff_t m_pCameraServices = 0x1328;
        constexpr std::ptrdiff_t m_hGroundEntity = 0x530;
        constexpr std::ptrdiff_t m_flGravityScale = 0x540;
        constexpr std::ptrdiff_t m_ModernJump = 0x6C8;
        constexpr std::ptrdiff_t ModernJump_nLastLandedTick = 0x20;
        constexpr std::ptrdiff_t ModernJump_flLastLandedFrac = 0x24;
        constexpr std::ptrdiff_t m_bDucking = 0x416;
        constexpr std::ptrdiff_t m_flStamina = 0x694;
        constexpr std::ptrdiff_t m_hPlayerPawn = 0x92C;
        constexpr std::ptrdiff_t m_pMovementServices = 0x1330;
        constexpr std::ptrdiff_t m_nTickBase = 0x6B8;
        constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8;
        constexpr std::ptrdiff_t m_Collision = 0xD30;
        constexpr std::ptrdiff_t m_vecMins = 0x40;
        constexpr std::ptrdiff_t m_vecMaxs = 0x4C;
        constexpr std::ptrdiff_t mv_flMaxSpeed = 0x1AC;
        constexpr std::ptrdiff_t mv_flForwardMove = 0x1C0;
        constexpr std::ptrdiff_t mv_flLeftMove = 0x1C4;
        constexpr std::ptrdiff_t mv_flUpMove = 0x1C8;
        constexpr std::ptrdiff_t mv_vecLastMoveImpulses = 0x1CC;
        constexpr std::ptrdiff_t mv_arrForceSubtickWhen = 0x1B0;
        constexpr std::ptrdiff_t mv_bDucked = 0x408;
        constexpr std::ptrdiff_t mv_nLastJumpTick = 0x700;
        constexpr std::ptrdiff_t m_nLastLandedTick = 0x6E8;
        constexpr std::ptrdiff_t m_flLastLandedFrac = 0x6EC;
    }

    // =========================================================================
    //  CUserCmd (build 14190, size 0x98)
    // =========================================================================
    namespace usercmd
    {
        constexpr std::ptrdiff_t cmd_number = 0x08;
        constexpr std::ptrdiff_t pb_in_cmd = 0x10;
        constexpr std::ptrdiff_t has_bits = 0x20;
        constexpr std::ptrdiff_t base = 0x40;

        constexpr std::ptrdiff_t bt_state = 0x58;
        constexpr std::ptrdiff_t bt_value = 0x60;
        constexpr std::ptrdiff_t bt_value_changed = 0x68;
        constexpr std::ptrdiff_t bt_value_scroll = 0x70;
        constexpr std::ptrdiff_t n_subtick_state = 0x94;

        constexpr std::ptrdiff_t buc_has_bits = 0x10;
        constexpr std::ptrdiff_t subtick_moves = 0x18;
        constexpr std::ptrdiff_t buttons_pb = 0x38;
        constexpr std::ptrdiff_t viewangles = 0x40;
        constexpr std::ptrdiff_t forwardmove = 0x58;
        constexpr std::ptrdiff_t leftmove = 0x5C;
        constexpr std::ptrdiff_t upmove = 0x60;
        constexpr std::ptrdiff_t mousedx = 0x70;
        constexpr std::ptrdiff_t mousedy = 0x74;

        constexpr std::ptrdiff_t va_x = 0x18;
        constexpr std::ptrdiff_t va_y = 0x1C;
        constexpr std::ptrdiff_t va_z = 0x20;

        // ---- CSubtickMoveStep (размер 0x38) ----
        constexpr std::ptrdiff_t sub_has_bits = 0x10;
        constexpr std::ptrdiff_t subtick_button = 0x18;
        constexpr std::ptrdiff_t subtick_pressed = 0x20;
        constexpr std::ptrdiff_t subtick_when = 0x24;

        constexpr std::ptrdiff_t buttonstate1 = 0x18;
        constexpr std::ptrdiff_t buttonstate2 = 0x20;
        constexpr std::ptrdiff_t buttonstate3 = 0x28;
    }

    namespace buttons
    {
        constexpr std::uint64_t IN_ATTACK = 1ull << 0;
        constexpr std::uint64_t IN_JUMP = 1ull << 1;
        constexpr std::uint64_t IN_DUCK = 1ull << 2;
        constexpr std::uint64_t IN_FORWARD = 1ull << 3;
        constexpr std::uint64_t IN_BACK = 1ull << 4;
        constexpr std::uint64_t IN_USE = 1ull << 5;
        constexpr std::uint64_t IN_LEFT = 1ull << 7;
        constexpr std::uint64_t IN_RIGHT = 1ull << 8;
        constexpr std::uint64_t IN_MOVELEFT = 1ull << 9;
        constexpr std::uint64_t IN_MOVERIGHT = 1ull << 10;
        constexpr std::uint64_t IN_ATTACK2 = 1ull << 11;
    }

    constexpr std::uint32_t FL_ONGROUND = 1u << 0;
    constexpr std::uint32_t FL_DUCKING = 1u << 1;

    namespace movetype
    {
        constexpr std::uint8_t NONE = 0;
        constexpr std::uint8_t WALK = 2;
        constexpr std::uint8_t NOCLIP = 8;
        constexpr std::uint8_t LADDER = 9;
        constexpr std::uint8_t OBSERVER = 10;
    }
}