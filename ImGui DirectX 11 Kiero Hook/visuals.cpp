#include "visuals.h"
#include <cstdint>
#include <vector>
#include <string>   // für std::string
#include <cmath>    // für std::sqrt und std::pow
#include <algorithm>

#include "ImGui/imgui.h"
#include <stdio.h>
#include "imgui/imstb_rectpack.h"
#include "imgui/imstb_truetype.h"


bool glow = false;
//ImVec4 glowColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
ImVec4 glowColor = ImVec4(0.5f, 0.0f, 0.5f, 1.0f); // Purple color (lila)

bool showTracers = false;
//ImVec4 tracersColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
ImVec4 tracersColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f); // Black color

float SnaplinesThickness = 1.5f; // Standarddicke der Linien

TracerStartPosition tracerStartPosition = TracerStartPosition::BottomCenter;




uintptr_t V::client = (uintptr_t)GetModuleHandle("client.dll");

void V::PlayerESP()
{
    float(*ViewMatrix)[4][4] = (float(*)[4][4])(client + O::dwViewMatrix);
    auto localPawn = *(uintptr_t*)(client + O::dwLocalPlayerPawn);

    if (!localPawn)
        return;

    auto localTeam = *(int*)(client + O::m_iTeamNum);
    auto entityList = *(uintptr_t*)(client + O::dwEntityList);

    if (!entityList)
        return;

    for (int i = 1; i < 64; i++)
    {
        uintptr_t list_entry1 = *(uintptr_t*)(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
        if (!list_entry1)
            continue;

        uintptr_t playerController = *(uintptr_t*)(list_entry1 + 120 * (i & 0x1FF));
        if (!playerController)
            continue;

        uint32_t playerPawn = *(uint32_t*)(playerController + O::m_hPlayerPawn); ///uint32_t
        if (!playerPawn)
            continue;

        uintptr_t list_entry2 = *(uintptr_t*)(entityList + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
        if (!list_entry2)
            continue;

        uintptr_t pCSPlayerPawnPtr = *(uintptr_t*)(list_entry2 + 120 * (playerPawn & 0x1FF));
        if (!pCSPlayerPawnPtr)
            continue;

        int health = *(int*)(pCSPlayerPawnPtr + O::m_iHealth);
        if (!health || health > 100)
            continue;

        int team = *(int*)(pCSPlayerPawnPtr + O::m_iTeamNum);
        if (team == localTeam)
            continue;

        Vec3 feetpos = *(Vec3*)(pCSPlayerPawnPtr + O::m_vOldOrigin);
        Vec3 headpos = { feetpos.x, feetpos.y, feetpos.z + 65.0f };

        Vec2 feet, head;
        if (feetpos.WorldToScreen(feet, ViewMatrix) && headpos.WorldToScreen(head, ViewMatrix)) {
            float height = (feet.y - head.y) * 1.1f; // Adjusted height for box
            float width = height / 2.0f; // Width of the box

            float x = feet.x - width / 2;
            float y = head.y;

            // Draw the main player box
            ImGui::GetBackgroundDrawList()->AddRect({ x, y }, { x + width, y + height }, ImColor(195, 25, 210));

            // Calculate health bar dimensions
            float healthBarHeight = (health / 100.0f) * height; // Height based on current health
            float healthBarWidth = 5.0f; // Fixed width for the health bar
            float healthBarX = x - healthBarWidth - 5.0f; // Position to the left of the player box
            float healthBarY = y + height - healthBarHeight; // Position based on current health

            // Draw the health bar background
            ImGui::GetBackgroundDrawList()->AddRectFilled({ healthBarX, y }, { healthBarX + healthBarWidth, y + height }, ImColor(50, 50, 50)); // Background color

            // Draw the actual health bar
            ImGui::GetBackgroundDrawList()->AddRectFilled({ healthBarX, healthBarY }, { healthBarX + healthBarWidth, y + height }, ImColor(0, 255, 0)); // Health color

            // Draw outline for the health bar and main box
            ImGui::GetBackgroundDrawList()->AddRect({ x, y }, { x + width, y + height }, ImColor(0, 0, 0), 0.0f, 0, 0.5f); // Outline for main box
            ImGui::GetBackgroundDrawList()->AddRect({ healthBarX, y }, { healthBarX + healthBarWidth, y + height }, ImColor(0, 0, 0), 0.0f, 0, 0.5f); // Outline for health bar
        }
    }
}


bool showFOVChanger = false;  // Toggle for FOV Changer
int currentFOV = 90;         // Standard-FOV

void V::FOVChanger() {
    if (!showFOVChanger)
        return;

    uintptr_t client = (uintptr_t)GetModuleHandle("client.dll");
    if (!client) return;

    uintptr_t localPlayerPawn = *(uintptr_t*)(client + O::dwLocalPlayerPawn);
    if (!localPlayerPawn) return;

    uintptr_t cameraServices = *(uintptr_t*)(localPlayerPawn + O::m_pCameraServices);
    if (!cameraServices) return;

    // Detect if FOV is already updated to avoid unnecessary writes
    int currentGameFOV = *(int*)(cameraServices + O::m_iFOV);

    bool isScoped = *(bool*)(localPlayerPawn + O::m_bIsScoped);
    if (!isScoped && currentGameFOV != currentFOV) {
        // Only update when necessary
        *(int*)(cameraServices + O::m_iFOV) = currentFOV;
    }





}


    bool antiFlashEnabled = false;  // Keeps track of the checkbox state (on/off)

    void V::AntiFlash() {
        // If the Anti-Flash feature is disabled, do nothing
        if (!antiFlashEnabled) return;

        // Holt den Client (client.dll)
        uintptr_t client = (uintptr_t)GetModuleHandle("client.dll");
        if (!client) return;

        // Holt die Adresse des lokalen Spielers (Pawn)
        uintptr_t localPlayerPawn = *(uintptr_t*)(client + O::dwLocalPlayerPawn);
        if (!localPlayerPawn) return;

        // Holt die Flashbang-Zeit (m_flFlashBangTime)
        float flashBangTime = *(float*)(localPlayerPawn + O::m_flFlashBangTime);
        if (flashBangTime > 0.0f) {
            // Setzt die Flashbang-Zeit auf 0, um den Flashbang zu löschen
            *(float*)(localPlayerPawn + O::m_flFlashBangTime) = 0.0f;
            // std::cout << "Flashbang removed!" << std::endl;
        }
    }


    // Funktion zur Berechnung der Distanz zwischen zwei Vektoren
    float GetDistance(const Vec3& v1, const Vec3& v2) {
        float dx = v1.x - v2.x;
        float dy = v1.y - v2.y;
        float dz = v1.z - v2.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    void V::DistanceESP() {
        uintptr_t client = (uintptr_t)GetModuleHandle("client.dll");

        // Zugriff auf ViewMatrix und lokalen Spieler
        float(*ViewMatrix)[4][4] = (float(*)[4][4])(client + O::dwViewMatrix);
        auto localPawn = *(uintptr_t*)(client + O::dwLocalPlayerPawn);

        if (!localPawn)
            return;

        auto localTeam = *(int*)(localPawn + O::m_iTeamNum);
        auto entityList = *(uintptr_t*)(client + O::dwEntityList);
        if (!entityList)
            return;

        // Schleife für alle Spieler im Spiel
        for (int i = 1; i < 64; i++) {
            uintptr_t listEntry1 = *(uintptr_t*)(entityList + (8 * (i & 0x7FFF) >> 9) + 16);
            if (!listEntry1)
                continue;

            uintptr_t playerController = *(uintptr_t*)(listEntry1 + 120 * (i & 0x1FF));
            if (!playerController)
                continue;

            uintptr_t playerPawn = *(uintptr_t*)(playerController + O::m_hPlayerPawn);
            if (!playerPawn)
                continue;

            uintptr_t listEntry2 = *(uintptr_t*)(entityList + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
            if (!listEntry2)
                continue;

            uintptr_t pCSPlayerPawnPtr = *(uintptr_t*)(listEntry2 + 120 * (playerPawn & 0x1FF));
            if (!pCSPlayerPawnPtr)
                continue;

            int health = *(int*)(pCSPlayerPawnPtr + O::m_iHealth);
            if (health <= 0 || health > 100)
                continue;

            int team = *(int*)(pCSPlayerPawnPtr + O::m_iTeamNum);
            if (team == localTeam)
                continue;

            // Berechne die Position des Spielers
            Vec3 feetpos = *(Vec3*)(pCSPlayerPawnPtr + O::m_vOldOrigin);

            // Berechne die Distanz zwischen dem lokalen Spieler und dem Gegner
            Vec3 localPos = *(Vec3*)(localPawn + O::m_vOldOrigin);

            // Berechnung der Distanz (Euklidische Distanz)
            float distance = GetDistance(feetpos, localPos);  // Verwendet die GetDistance-Funktion

            // Skaliere die Distanz mit einem Faktor (z. B. 0.1, um sie kleiner zu machen)
            float scale_factor = 0.1f;  // Beispielwert für den Skalierungsfaktor (10x kleiner)
            distance *= scale_factor;  // Skaliert die berechnete Distanz

            Vec2 feet, box;
            // WorldToScreen für die Position des Spielers (Box)
            if (feetpos.WorldToScreen(box, ViewMatrix)) {
                // Weiße Farbe für den Distance ESP
                ImColor color = ImColor(255, 255, 255);

                // Erstelle den Dist-Text (nur als Zahl ohne "m")
                std::string distanceStr = std::to_string((int)distance);  // Nur die Zahl der Distanz

                // Berechne die Textgröße (kleiner machen)
                ImVec2 textSize = ImGui::CalcTextSize(distanceStr.c_str());

                // Position des Textes unter der Box
                ImVec2 textPos = ImVec2(box.x - textSize.x / 2, box.y + 18); // Etwas nach unten verschieben

                // Zeichne nur den Text (ohne die Box)
                ImGui::GetBackgroundDrawList()->AddText(textPos, color, distanceStr.c_str());
            }
        }
    }





    void V::PlayerGlow()
    {
        uintptr_t client = (uintptr_t)GetModuleHandle("client.dll");
        auto localpawn = *(uintptr_t*)(client + O::dwLocalPlayerPawn);
        if (!localpawn || !glow) // Exit if the glow effect is disabled
            return;

        auto localTeam = *(int*)(localpawn + O::m_iTeamNum);
        auto entitylist = *(uintptr_t*)(client + O::dwEntityList);
        if (!entitylist)
            return;

        for (int i = 1; i < 64; i++)
        {
            uintptr_t list_entry1 = *(uintptr_t*)(entitylist + (8 * (i & 0x7FFF) >> 9) + 16);
            if (!list_entry1)
                continue;

            uintptr_t playerController = *(uintptr_t*)(list_entry1 + 120 * (i & 0x1FF));
            if (!playerController)
                continue;

            uintptr_t  playerPawn = *(uintptr_t*)(playerController + O::m_hPlayerPawn); ///uint32_t
            if (!playerPawn)
                continue;

            uintptr_t list_entry2 = *(uintptr_t*)(entitylist + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
            if (!list_entry2)
                continue;

            uintptr_t pCSPlayerPawnPtr = *(uintptr_t*)(list_entry2 + 120 * (playerPawn & 0x1FF));
            if (!pCSPlayerPawnPtr)
                continue;

            int team = *(int*)(pCSPlayerPawnPtr + O::m_iTeamNum);
            if (team == localTeam)
                continue;

            int health = *(int*)(pCSPlayerPawnPtr + O::m_iHealth);
            if (health <= 0)
                continue;

            DWORD colorArgb = ((DWORD)(glowColor.w * 255) << 24) |
                ((DWORD)(glowColor.z * 255) << 16) |
                ((DWORD)(glowColor.y * 255) << 8) |
                ((DWORD)(glowColor.x * 255));

            *(DWORD*)(pCSPlayerPawnPtr + O::m_Glow + O::m_glowColorOverride) = colorArgb;
            *(DWORD*)(pCSPlayerPawnPtr + O::m_Glow + O::m_bGlowing) = 1;
        }
    }


    void V::PlayerTracers()
    {
        if (!showTracers)
            return;

        uintptr_t client = (uintptr_t)GetModuleHandle("client.dll");

        float(*ViewMatrix)[4][4] = (float(*)[4][4])(client + O::dwViewMatrix);

        auto localpawn = *(uintptr_t*)(client + O::dwLocalPlayerPawn);
        if (!localpawn)
            return;

        Vec2 screenCenter = { (float)GetSystemMetrics(SM_CXSCREEN) / 2, (float)GetSystemMetrics(SM_CYSCREEN) / 2 };
        Vec2 screen = { (float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN) };

        auto localTeam = *(int*)(localpawn + O::m_iTeamNum);
        auto entitylist = *(uintptr_t*)(client + O::dwEntityList);
        if (!entitylist)
            return;

        ImColor lineColor = ImColor(tracersColor.x * 255, tracersColor.y * 255, tracersColor.z * 255, tracersColor.w * 255);

        for (int i = 1; i < 64; i++)
        {
            uintptr_t list_entry1 = *(uintptr_t*)(entitylist + (8 * (i & 0x7FFF) >> 9) + 16);
            if (!list_entry1)
                continue;

            uintptr_t playerController = *(uintptr_t*)(list_entry1 + 120 * (i & 0x1FF));
            if (!playerController)
                continue;

            uintptr_t playerPawn = *(uintptr_t*)(playerController + O::m_hPlayerPawn); ///uint32_t
            if (!playerPawn)
                continue;

            uintptr_t list_entry2 = *(uintptr_t*)(entitylist + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
            if (!list_entry2)
                continue;

            uintptr_t pCSPlayerPawnPtr = *(uintptr_t*)(list_entry2 + 120 * (playerPawn & 0x1FF));
            if (!pCSPlayerPawnPtr)
                continue;

            int team = *(int*)(pCSPlayerPawnPtr + O::m_iTeamNum);
            if (team == localTeam)
                continue;

            int health = *(int*)(pCSPlayerPawnPtr + O::m_iHealth);
            if (health <= 0)
                continue;

            Vec3 feetpos = *(Vec3*)(pCSPlayerPawnPtr + O::m_vOldOrigin);
            Vec2 feet;

            if (feetpos.WorldToScreen(feet, ViewMatrix))
            {
                Vec2 tracerStart;

                switch (tracerStartPosition)
                {
                case TracerStartPosition::BottomCenter:
                    tracerStart = { screen.x / 2, screen.y };
                    break;
                case TracerStartPosition::ScreenCenter:
                    tracerStart = { screen.x / 2, screen.y / 2 };
                    break;
                case TracerStartPosition::TopLeft:
                    tracerStart = { 0.0f, 0.0f };
                    break;
                case TracerStartPosition::TopRight:
                    tracerStart = { screen.x, 0.0f };
                    break;
                case TracerStartPosition::TopCenter:
                    tracerStart = { screen.x / 2, 0.0f };
                    break;
                case TracerStartPosition::BottomLeft:
                    tracerStart = { 0.0f, screen.y };
                    break;
                case TracerStartPosition::BottomRight:
                    tracerStart = { screen.x, screen.y };
                    break;
                }

                // Draw line from the selected starting position
                ImGui::GetBackgroundDrawList()->AddLine(
                    { tracerStart.x, tracerStart.y },
                    { feet.x, feet.y },
                    lineColor,
                    SnaplinesThickness
                );


            }
        }
    }




//[4]
//= *ViewMatrix