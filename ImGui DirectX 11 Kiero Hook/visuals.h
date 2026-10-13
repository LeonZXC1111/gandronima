#pragma once
#include "math.h"
#include <cstddef>
#include <cstdint>
#include "ImGui/imgui.h"
//#include "ImGui/imgui_impl_win32.h"
//#include "ImGui/imgui_impl_dx11.h"






enum class TracerStartPosition
{
	BottomCenter,
	ScreenCenter,
	TopLeft,
	TopRight,
	TopCenter,
	BottomLeft,
	BottomRight
};


// Nur Deklaration
extern TracerStartPosition tracerStartPosition;
// Default starting position for tracers





extern float SnaplinesThickness; // Standarddicke der Linien

extern  int currentFOV;
// int currentFOV = 90;
extern bool showFOVChanger;







extern bool showTracers;
extern ImVec4 tracersColor;
extern ImVec4 glowColor;
extern  bool glow;







extern  bool antiFlashEnabled;











namespace O
{
	constexpr std::ptrdiff_t dwEntityList = 0x27130E8;      // 14184 (was 0x1A359C0)
	constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x255E658; // 14184 (was 0x1889F30)
	constexpr std::ptrdiff_t dwViewMatrix = 0x25639A0;      // 14184 (was 0x1AA17C0)

	constexpr std::ptrdiff_t m_hPlayerPawn = 0x92C;         // 14184 (was 0x80C)


	constexpr std::ptrdiff_t  m_iHealth = 0x34C;            // 14184 (was 0x344)
	constexpr std::ptrdiff_t m_iTeamNum = 0x3E7;            // 14184 (was 0x3E3)
	constexpr std::ptrdiff_t m_vOldOrigin = 0x14A4;         // 14184 (was 0x1324)

	//constexpr std::ptrdiff_t localPlayer = 0x1A0E9A8;



	//constexpr std::ptrdiff_t m_ArmorValue = 0x2404; //

	//constexpr std::ptrdiff_t m_hPawn = 0x62C; // 
	//constexpr std::ptrdiff_t m_iPawnHealth = 0x818; // 


	constexpr	std::ptrdiff_t m_bGlowing = 0x51;           // 14184 (unchanged, within CGlowProperty)
	constexpr	std::ptrdiff_t m_glowColorOverride = 0x40;  // 14184 (unchanged, within CGlowProperty)
	constexpr	std::ptrdiff_t m_Glow = 0xDE8;              // 14184 (was 0xC00) C_BaseModelEntity::m_Glow
	constexpr	std::ptrdiff_t m_hActiveWeapon = 0x58;      // STALE/unused - not verified for 14184


	///TEST OFFSETS - NOTE: none of these are used by visuals.cpp. Only the few
	/// marked "14184" below were refreshed from the dump; the rest are STALE
	/// (old build) and UNVERIFIED - fix before using them for a new feature.
	constexpr    std::ptrdiff_t m_vecAbsVelocity = 0x3F0; // STALE/unused - not verified for 14184

	constexpr    std::ptrdiff_t m_pGameSceneNode = 0x330; // 14184 (was 0x328) CGameSceneNode*
	constexpr    std::ptrdiff_t m_modelState = 0x170; // STALE/unused - not verified
	constexpr    std::ptrdiff_t m_skeletonInstance = 0x50; // STALE/unused - not verified
	constexpr    std::ptrdiff_t m_pClippingWeapon = 0x13A0; // GONE in 14184 (no such field) - use weapon services
	constexpr    std::ptrdiff_t m_AttributeManager = 0x1148; // STALE/unused - not verified
	constexpr    std::ptrdiff_t m_Item = 0x50; // STALE/unused - not verified
	constexpr    std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA; // STALE/unused - not verified
	constexpr    std::ptrdiff_t m_fFlags = 0x3F4; // 14184 (was 0x3EC) - unused here; movement uses cs2::schema::m_fFlags
	constexpr    std::ptrdiff_t m_steamID = 0x6E8; // STALE/unused - not verified

	constexpr    std::ptrdiff_t m_iszPlayerName = 0x660; // STALE/unused - not verified



	constexpr    std::ptrdiff_t m_angEyeAngles = 0x35F0; // 14184 (was 0x1438)

	constexpr    std::ptrdiff_t m_bIsLocalPlayerController = 0x6F0; // STALE/unused - not verified


	constexpr std::ptrdiff_t m_iFOV = 0x298; // uint32 - 14184, on CPlayer_CameraServices (was 0x210)
	constexpr std::ptrdiff_t m_pCameraServices = 0x1328; // 14184 (was 0x11E0)
	constexpr std::ptrdiff_t m_bIsScoped = 0x1EA0; // bool - 14184 (was 0x23E8)


	constexpr std::ptrdiff_t m_flFlashBangTime = 0x14FC; // float32 - 14184 (was 0x13F8)



	constexpr std::ptrdiff_t m_skybox3d = 0x1268; // STALE/unused - not verified for 14184

}





namespace V
{
	extern uintptr_t client;

	void PlayerESP();



	void PlayerTracers();

	void PlayerGlow();
	//void PlayerNameTags();

	void FOVChanger();



	void AntiFlash();





	void DistanceESP();

	void ArrowESP();

};



