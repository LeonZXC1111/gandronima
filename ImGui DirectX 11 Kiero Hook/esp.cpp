#include "esp.h"
#include "settings.h"
#include "game.h"
#include "my_math.h"
#include "debuglog.h"
#include "interfaces.h"   // mem_writable
#include "signatures.h"   // sig::find (bone id resolver)
#include "imgui/imgui.h"

#include <Windows.h>
#include <cstdint>
#include <cfloat>

// Verdana text font built in menu::init_fonts (font[0] is the 44px icon font, so
// drawing ESP text with the default font renders garbage). Use this explicitly.
extern ImFont* font;

namespace
{
    inline ImFont* text_font() { return font ? font : ImGui::GetFont(); }

    inline bool valid_ptr(std::uintptr_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFFull; }

    template <typename T>
    inline T rd(std::uintptr_t addr)
    {
        if (!valid_ptr(addr)) return T{};
        return *reinterpret_cast<T*>(addr);
    }

    inline DWORD to_rgba(const float c[4])
    {
        return  (static_cast<DWORD>(c[3] * 255) << 24) |
                (static_cast<DWORD>(c[2] * 255) << 16) |
                (static_cast<DWORD>(c[1] * 255) << 8)  |
                (static_cast<DWORD>(c[0] * 255));
    }

    // ---- Bone ESP (skeleton) --------------------------------------------------
    // Verified chain (build 14190): pawn -> m_pGameSceneNode (0x330, a
    // CSkeletonInstance) -> bone transform array at +0x1C0. Each bone entry is 32
    // bytes: Vector pos @ +0x0, float scale @ +0xC, Quaternion @ +0x10.
    constexpr std::ptrdiff_t kBoneArray  = 0x1C0;
    constexpr std::size_t    kBoneStride = 32;

    // C_BaseEntity::GetBoneIdByName (LookupBone): int(this_entity, const char*).
    // Loads m_pGameSceneNode itself, so pass the pawn pointer.
    using GetBoneId_t = int(__fastcall*)(void* entity, const char* name);
    GetBoneId_t g_getBoneId = nullptr;
    bool        g_boneResolved = false;

    void resolve_bone_fn()
    {
        if (g_boneResolved) return;
        g_boneResolved = true;
        const auto a = sig::find("client.dll",
            "40 53 48 83 EC 20 48 8B 89 30 03 00 00 48 8B DA 48 8B 01 FF 50 68 48 8B");
        if (a) g_getBoneId = reinterpret_cast<GetBoneId_t>(a);
    }

    int bone_id(void* pawn, const char* name) noexcept
    {
        if (!g_getBoneId || !pawn) return -1;
        __try { return g_getBoneId(pawn, name); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
    }

    // Skeleton bone names + the segments that connect them.
    const char* const kBones[] = {
        "head_0", "neck_0", "spine_2", "spine_1", "pelvis",
        "arm_upper_L", "arm_lower_L", "hand_L",
        "arm_upper_R", "arm_lower_R", "hand_R",
        "leg_upper_L", "leg_lower_L", "ankle_L",
        "leg_upper_R", "leg_lower_R", "ankle_R",
    };
    constexpr int kBoneCount = sizeof(kBones) / sizeof(kBones[0]);
    const int kConns[][2] = {
        {0,1},{1,2},{2,3},{3,4},
        {1,5},{5,6},{6,7},
        {1,8},{8,9},{9,10},
        {4,11},{11,12},{12,13},
        {4,14},{14,15},{15,16},
    };

    // World->screen in ImGui draw space (DisplaySize).
    bool w2s(const Vec3& p, float(*m)[4][4], float sw, float sh, ImVec2& out)
    {
        const float w = (*m)[3][0] * p.x + (*m)[3][1] * p.y + (*m)[3][2] * p.z + (*m)[3][3];
        if (w < 0.01f) return false;
        const float inv = 1.0f / w;
        out.x = sw * 0.5f + ((*m)[0][0] * p.x + (*m)[0][1] * p.y + (*m)[0][2] * p.z + (*m)[0][3]) * inv * sw * 0.5f;
        out.y = sh * 0.5f - ((*m)[1][0] * p.x + (*m)[1][1] * p.y + (*m)[1][2] * p.z + (*m)[1][3]) * inv * sh * 0.5f;
        return true;
    }
}

void esp::render()
{
    if (!settings::visuals::glow && !settings::visuals::box &&
        !settings::visuals::name && !settings::visuals::healthbar &&
        !settings::visuals::skeleton)
        return;
    if (!game::ready())
        return;

    if (settings::visuals::skeleton)
        resolve_bone_fn();

    const auto& o = game::off();

    const std::uintptr_t local = game::local_pawn();
    if (!valid_ptr(local))
        return;
    const int local_team = rd<std::uint8_t>(local + o.m_iTeamNum);

    auto vm = game::view_matrix();
    if (!vm) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const ImU32 box_col = ImGui::GetColorU32(ImVec4(
        settings::visuals::box_color[0], settings::visuals::box_color[1],
        settings::visuals::box_color[2], settings::visuals::box_color[3]));
    const ImU32 name_col = ImGui::GetColorU32(ImVec4(
        settings::visuals::name_color[0], settings::visuals::name_color[1],
        settings::visuals::name_color[2], settings::visuals::name_color[3]));
    const ImU32 hp_top = ImGui::GetColorU32(ImVec4(
        settings::visuals::hp_top[0], settings::visuals::hp_top[1],
        settings::visuals::hp_top[2], settings::visuals::hp_top[3]));
    const ImU32 hp_bot = ImGui::GetColorU32(ImVec4(
        settings::visuals::hp_bottom[0], settings::visuals::hp_bottom[1],
        settings::visuals::hp_bottom[2], settings::visuals::hp_bottom[3]));
    const ImU32 hp_single = ImGui::GetColorU32(ImVec4(
        settings::visuals::hp_color[0], settings::visuals::hp_color[1],
        settings::visuals::hp_color[2], settings::visuals::hp_color[3]));

    static bool prevEnd = false;
    const bool endDown = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    const bool dumpNow = endDown && !prevEnd;
    prevEnd = endDown;
    bool dumped = false;

    for (int i = 1; i < 64; i++)
    {
        const std::uintptr_t ctrl = game::entity_by_index(i);
        if (!valid_ptr(ctrl)) continue;
        const std::uint32_t h = rd<std::uint32_t>(ctrl + o.m_hPlayerPawn);
        if (!h || h == 0xFFFFFFFF) continue;
        const std::uintptr_t pawn = game::entity_by_index(h & 0x7FFF);
        if (!valid_ptr(pawn) || pawn == local) continue;

        const int team = rd<std::uint8_t>(pawn + o.m_iTeamNum);
        if ((team != 2 && team != 3) || team == local_team) continue;

        const int health = rd<int>(pawn + o.m_iHealth);
        if (health <= 0 || health > 100) continue;

        if (dumpNow && !dumped)
        {
            dumped = true;
            const char* nm = reinterpret_cast<const char*>(ctrl + 0x6FC);
            dbg::log("\n[ESP] ctrl=%llX pawn=%llX team=%d hp=%d name='%s'\n",
                     (unsigned long long)ctrl, (unsigned long long)pawn, team, health,
                     mem_readable(nm, 1) ? nm : "<unreadable>");
        }

        // Glow is now forced flicker-free by the IsGlowing/GetGlowColor hooks in
        // glowhook.cpp (the game asks us each query), so we no longer write the
        // CGlowProperty every frame here - that per-frame write was the flicker.

        if (!settings::visuals::box && !settings::visuals::name &&
            !settings::visuals::healthbar && !settings::visuals::skeleton)
            continue;

        const std::uintptr_t scene = rd<std::uintptr_t>(pawn + o.m_pGameSceneNode);
        if (!valid_ptr(scene)) continue;

        // Skeleton (bone ESP): resolve each bone by name, read its world position
        // from the bone transform array, project, and connect the segments.
        if (settings::visuals::skeleton && g_getBoneId)
        {
            const std::uintptr_t bones = rd<std::uintptr_t>(scene + kBoneArray);
            const Vec3 ref = rd<Vec3>(scene + o.m_vecAbsOrigin); // feet, for sanity
            if (valid_ptr(bones))
            {
                ImVec2 sp[kBoneCount];
                bool   ok[kBoneCount];
                for (int bi = 0; bi < kBoneCount; ++bi)
                {
                    ok[bi] = false;
                    const int id = bone_id(reinterpret_cast<void*>(pawn), kBones[bi]);
                    // LookupBone returns <=0 for names this agent's rig doesn't have;
                    // treating those as bone 0 made every miss collapse onto the root
                    // and draw a star. Require a real, positive index.
                    if (id <= 0 || id > 255) continue;
                    const Vec3 bp = rd<Vec3>(bones + kBoneStride * static_cast<std::size_t>(id));
                    if (bp.x == 0.0f && bp.y == 0.0f && bp.z == 0.0f) continue;
                    // Reject implausible bones (bad stride/garbage): a human bone is
                    // within ~160u of the pawn origin.
                    const float dx = bp.x - ref.x, dy = bp.y - ref.y, dz = bp.z - ref.z;
                    if (dx * dx + dy * dy + dz * dz > 160.0f * 160.0f) continue;
                    ok[bi] = w2s(bp, vm, sw, sh, sp[bi]);
                }
                const ImU32 sk_col = ImGui::GetColorU32(ImVec4(
                    settings::visuals::skeleton_color[0], settings::visuals::skeleton_color[1],
                    settings::visuals::skeleton_color[2], settings::visuals::skeleton_color[3]));
                for (const auto& c : kConns)
                    if (ok[c[0]] && ok[c[1]])
                        dl->AddLine(sp[c[0]], sp[c[1]], sk_col, 1.3f);
            }
        }

        if (!settings::visuals::box && !settings::visuals::name && !settings::visuals::healthbar)
            continue;

        const Vec3 feet = rd<Vec3>(scene + o.m_vecAbsOrigin);
        const Vec3 head = { feet.x, feet.y, feet.z + 72.0f };
        ImVec2 sfeet, shead;
        if (!w2s(feet, vm, sw, sh, sfeet) || !w2s(head, vm, sw, sh, shead))
            continue;

        const float bh = sfeet.y - shead.y;
        if (bh < 2.0f) continue;
        const float bw = bh / 2.0f;
        const float x = sfeet.x - bw / 2.0f;
        const float y = shead.y;

        if (settings::visuals::box)
        {
            // thin: 1px black "shadow" just outside + 1px colour box
            dl->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + bw + 1, y + bh + 1), IM_COL32(0, 0, 0, 160), 0, 0, 1.0f);
            dl->AddRect(ImVec2(x, y),         ImVec2(x + bw, y + bh),         box_col,                0, 0, 1.0f);
        }

        if (settings::visuals::healthbar)
        {
            const float hbx = x - 5.0f;
            const float hbw = 2.0f;
            dl->AddRectFilled(ImVec2(hbx - 1, y - 1), ImVec2(hbx + hbw + 1, y + bh + 1), IM_COL32(0, 0, 0, 180));
            const float fh = bh * (health / 100.0f);
            const float fy = y + bh - fh;
            if (settings::visuals::hp_gradient)
                dl->AddRectFilledMultiColor(ImVec2(hbx, fy), ImVec2(hbx + hbw, y + bh), hp_top, hp_top, hp_bot, hp_bot);
            else
                dl->AddRectFilled(ImVec2(hbx, fy), ImVec2(hbx + hbw, y + bh), hp_single);
        }

        if (settings::visuals::name)
        {
            const char* nm = reinterpret_cast<const char*>(ctrl + 0x6FC); // m_iszPlayerName char[128]
            if (mem_readable(nm, 2) && nm[0])
            {
                char safe[128];
                lstrcpynA(safe, nm, sizeof(safe));
                ImFont* f = text_font();
                const float fs = 14.0f;
                const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, safe);
                const ImVec2 np(x + bw * 0.5f - ts.x * 0.5f, y - ts.y - 2.0f);
                dl->AddText(f, fs, ImVec2(np.x + 1, np.y + 1), IM_COL32(0, 0, 0, 200), safe); // shadow
                dl->AddText(f, fs, np, name_col, safe);
            }
        }
    }
}