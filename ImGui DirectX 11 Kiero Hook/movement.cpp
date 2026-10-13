#include "movement.h"
#include "settings.h"
#include "signatures.h"
#include "cs2_offsets.h"
#include "interfaces.h"
#include "globals.h"
#include "debuglog.h"
#include "prediction.h"
#include "strafe.h"
#include "trace.h"
#include "schema.h"
#include "jumpbug.h"
#include "bhop.h"
#include "kiero/minhook/include/MinHook.h"

#include <Windows.h>
#include <cstdint>
#include <cmath>
#include <algorithm>

using CreateMove_t = bool(__fastcall*)(void* pInput, std::uint32_t ssi, void* a3);
using MouseInputEnabled_t = bool(__fastcall*)(void* thisptr);
using SubtickCreate_t = void* (__fastcall*)(void* pArena);

namespace
{
    CreateMove_t        oCreateMove = nullptr;
    MouseInputEnabled_t oMouseInput = nullptr;
    SubtickCreate_t     gSubtickCreate = nullptr;
    void* gCreateMoveTarget = nullptr;
    void* gMouseTarget = nullptr;
    bool  gInstalled = false;
    bool  gSubtickDisabled = false;
    volatile unsigned long gCallCount = 0;
    std::uintptr_t gClientBase = 0;

    inline bool valid_ptr(std::uintptr_t a) { return a >= 0x10000 && a < 0x7FFFFFFFFFFFULL; }
    inline bool aligned_ptr(std::uintptr_t a) { return valid_ptr(a) && (a & 0x7) == 0; }

    template <typename T> inline T rd(std::uintptr_t a)
    {
        if (!valid_ptr(a)) return T{}; return *reinterpret_cast<T*>(a);
    }
    template <typename T> inline void wr(std::uintptr_t a, const T& v)
    {
        if (valid_ptr(a)) *reinterpret_cast<T*>(a) = v;
    }

    bool     g_pred_ready_cache = false;
    bool     g_trace_ready_cache = false;
    uint32_t g_ready_cache_age = 0;
    inline void refresh_readiness_caches() {
        if (g_ready_cache_age > 0) { --g_ready_cache_age; return; }
        g_pred_ready_cache = prediction::ready();
        g_trace_ready_cache = trace::ready();
        g_ready_cache_age = 32;
    }

    std::uintptr_t local_pawn() {
        return gClientBase ? rd<std::uintptr_t>(gClientBase + cs2::client::LocalPlayerPawn) : 0;
    }

    std::uintptr_t proto_base(void* cmdArg) {
        const auto cmd = reinterpret_cast<std::uintptr_t>(cmdArg);
        if (!aligned_ptr(cmd)) return 0;
        const auto pb = rd<std::uintptr_t>(cmd + cs2::usercmd::base);
        return aligned_ptr(pb) ? pb : 0;
    }
    std::uintptr_t proto_buttons(void* cmdArg) {
        const auto pb = proto_base(cmdArg);
        if (!pb) return 0;
        const auto btn = rd<std::uintptr_t>(pb + cs2::usercmd::buttons_pb);
        return aligned_ptr(btn) ? btn : 0;
    }

    void write_u64(std::uintptr_t a, std::uint64_t v) { wr<std::uint64_t>(a, v); }
    void or_u64(std::uintptr_t a, std::uint64_t b) { wr<std::uint64_t>(a, rd<std::uint64_t>(a) | b); }
    void and_u64(std::uintptr_t a, std::uint64_t m) { wr<std::uint64_t>(a, rd<std::uint64_t>(a) & m); }
    void write_f(std::uintptr_t a, float v) { wr<float>(a, v); }
    int  read_i(std::uintptr_t a) { return rd<int>(a); }

    void live_button(std::uintptr_t cmd, std::uint64_t bit, bool down) {
        if (!aligned_ptr(cmd)) return;
        auto value = rd<std::uint64_t>(cmd + cs2::usercmd::bt_value);
        auto changed = rd<std::uint64_t>(cmd + cs2::usercmd::bt_value_changed);
        if (down) { value |= bit; changed |= bit; }
        else { value &= ~bit; changed |= bit; }
        wr<std::uint64_t>(cmd + cs2::usercmd::bt_value, value);
        wr<std::uint64_t>(cmd + cs2::usercmd::bt_value_changed, changed);
    }
    void button(std::uintptr_t cmd, std::uintptr_t pbtn, std::uint64_t bit, bool down) {
        live_button(cmd, bit, down);
        if (!pbtn) return;
        if (down) or_u64(pbtn + cs2::usercmd::buttonstate1, bit);
        else      and_u64(pbtn + cs2::usercmd::buttonstate1, ~bit);
    }

    void* alloc_step(std::uintptr_t base) {
        if (!aligned_ptr(base) || !gSubtickCreate || gSubtickDisabled) return nullptr;
        __try {
            // CreateNewSubtickMoveStep(arena). arena_ is the RepeatedPtrField's at
            // base+0x18; it may be null (then the fn heap-allocates), so don't reject it.
            const auto arena = rd<std::uintptr_t>(base + 0x18);
            return gSubtickCreate(reinterpret_cast<void*>(arena));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { gSubtickDisabled = true; return nullptr; }
    }

    // Append a CSubtickMoveStep to CBaseUserCmdPB::subtick_moves (field @ base+0x18)
    // the way protobuf RepeatedPtrField::Add does, and return it (has_bits cleared).
    // Field layout: arena* +0x18, int current_size +0x20, int total_size +0x24,
    // Rep* +0x28; Rep: int allocated_size +0x00, void* elements[] +0x08.
    // Just calling CreateNewSubtickMoveStep makes an ORPHAN the engine never sees -
    // the step MUST be linked into the Rep. Returns 0 if the field can't take one
    // (then only the live button write drives the action - still works tick-level).
    std::uintptr_t add_subtick_move(std::uintptr_t base) {
        if (!aligned_ptr(base) || gSubtickDisabled) return 0;
        std::uintptr_t step = 0;
        __try {
            const int cur = rd<int>(base + 0x20);
            const int tot = rd<int>(base + 0x24);
            const std::uintptr_t rep = rd<std::uintptr_t>(base + 0x28);
            if (cur < 0 || cur > 60 || !aligned_ptr(rep)) return 0;
            const int alloc = rd<int>(rep + 0x00);
            if (alloc < 0 || alloc > 4096) return 0;
            const std::uintptr_t elems = rep + 0x08;
            if (cur < alloc) {
                // reuse a cached, constructed slot
                step = rd<std::uintptr_t>(elems + 8ull * static_cast<unsigned>(cur));
                if (!aligned_ptr(step)) return 0;
            }
            else if (cur < tot) {
                // array slot free but no cached message: construct one and link it
                step = reinterpret_cast<std::uintptr_t>(alloc_step(base));
                if (!aligned_ptr(step)) return 0;
                wr<std::uintptr_t>(elems + 8ull * static_cast<unsigned>(cur), step);
                wr<int>(rep + 0x00, alloc + 1);
            }
            else {
                return 0; // full; growing needs the arena realloc path - skip safely
            }
            wr<int>(base + 0x20, cur + 1);     // current_size++
            wr<std::uint32_t>(step + 0x10, 0); // clear the step's has_bits
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { gSubtickDisabled = true; return 0; }
        return step;
    }

    void add_subtick(std::uintptr_t base, std::uint64_t btnbit, float when, bool pressed) {
        if (!(when >= 0.0f && when <= 1.0f)) when = 0.0f;
        const std::uintptr_t s = add_subtick_move(base);
        if (!s) return;
        wr<std::uint64_t>(s + 0x18, btnbit);
        wr<bool>(s + 0x20, pressed);
        wr<float>(s + 0x24, when);
        wr<std::uint32_t>(s + 0x10, 0x1u | 0x2u | 0x4u); // button + pressed + when
    }

    void add_subtick_yaw(std::uintptr_t base, float when, float yaw_delta) {
        if (!(when >= 0.0f && when <= 1.0f)) when = 0.0f;
        const std::uintptr_t s = add_subtick_move(base);
        if (!s) return;
        wr<std::uint64_t>(s + 0x18, 0);
        wr<bool>(s + 0x20, false);
        wr<float>(s + 0x24, when);
        wr<float>(s + 0x28, 0.0f);
        wr<float>(s + 0x2C, 0.0f);
        wr<float>(s + 0x30, 0.0f);
        wr<float>(s + 0x34, yaw_delta);
        wr<std::uint32_t>(s + 0x10, 0x4u | 0x40u); // when + yaw_delta
    }

    // =========================================================================
    //  SUBTICK AUTOSTRAFE (Movement Keys Strafer)
    // =========================================================================
    constexpr float kRad2Deg = 57.29577951308232f;
    constexpr float kDeg2Rad = 0.017453292519943295f;
    constexpr int   kSubticks = 32;
    constexpr float kSvAiraccel = 12.0f;
    constexpr float kSvAirMaxWish = 30.0f;
    constexpr float kMaxSpeed = 250.0f;
    constexpr float kFriction = 1.0f;

    inline float strafe_norm(float a) {
        while (a > 180.0f) a -= 360.0f;
        while (a < -180.0f) a += 360.0f;
        return a;
    }

    void sim_air_accelerate(float& vx, float& vy, float wish_yaw, float sub_frame) {
        const float wx = std::cos(wish_yaw * kDeg2Rad);
        const float wy = std::sin(wish_yaw * kDeg2Rad);
        const float wish_speed = kMaxSpeed;
        const float wishspd = (wish_speed < kSvAirMaxWish) ? wish_speed : kSvAirMaxWish;
        const float addspeed = wishspd - (vx * wx + vy * wy);
        if (addspeed <= 0.0f) return;
        float accelspeed = wish_speed * kSvAiraccel * kFriction * sub_frame;
        if (accelspeed > addspeed) accelspeed = addspeed;
        vx += wx * accelspeed;
        vy += wy * accelspeed;
    }

    void subtick_strafe(std::uintptr_t cmd, std::uintptr_t pbtn,
        std::uintptr_t base, const prediction::Result& pr)
    {
        if (!aligned_ptr(base) || !pr.valid || pr.onground) return;

        float vx = pr.velx, vy = pr.vely;
        float speed = std::sqrt(vx * vx + vy * vy);
        if (speed < 5.0f) return;

        const auto va = rd<std::uintptr_t>(base + cs2::usercmd::viewangles);
        if (!aligned_ptr(va)) return;

        float run_yaw = strafe_norm(rd<float>(va + cs2::usercmd::va_y));

        const std::uint64_t m = cs2::buttons::IN_FORWARD | cs2::buttons::IN_BACK
            | cs2::buttons::IN_MOVELEFT | cs2::buttons::IN_MOVERIGHT;
        const std::uint64_t held = aligned_ptr(cmd)
            ? rd<std::uint64_t>(cmd + cs2::usercmd::bt_value) : 0;

        float offset = 0.0f;
        if (held & cs2::buttons::IN_MOVELEFT)  offset += 90.0f;
        if (held & cs2::buttons::IN_MOVERIGHT) offset -= 90.0f;
        if (held & cs2::buttons::IN_FORWARD)   offset *= 0.5f;
        else if (held & cs2::buttons::IN_BACK) offset = (-offset * 0.5f) + 180.0f;

        if (offset == 0.0f) return;

        const float wish_yaw = strafe_norm(run_yaw + offset);

        if (aligned_ptr(cmd)) and_u64(cmd + cs2::usercmd::bt_value, ~m);
        if (pbtn)             and_u64(pbtn + cs2::usercmd::buttonstate1, ~m);

        write_f(base + cs2::usercmd::forwardmove, 1.0f);
        write_f(base + cs2::usercmd::leftmove, 0.0f);

        const float sub_frame = (1.0f / 64.0f) / static_cast<float>(kSubticks);
        const float wish_cap = 30.0f;

        for (int i = 1; i <= kSubticks; ++i)
        {
            const float vel_angle = strafe_norm(std::atan2(vy, vx) * kRad2Deg);
            const float vel_delta = strafe_norm(wish_yaw - vel_angle);

            const float air_gain = kSvAiraccel * kMaxSpeed * sub_frame * kFriction;
            const float cos_ideal = std::clamp((wish_cap - air_gain) / speed, -1.0f, 1.0f);
            float ideal = std::min(std::acos(cos_ideal) * kRad2Deg, 90.0f);

            const float target = ((std::fabs(vel_delta) > 170.0f && speed > 80.0f) || vel_delta > 0.0f)
                ? vel_angle + ideal
                : vel_angle - ideal;
            const float tgt = strafe_norm(target);

            add_subtick_yaw(base, static_cast<float>(i) / static_cast<float>(kSubticks),
                strafe_norm(tgt - run_yaw));
            run_yaw = tgt;

            sim_air_accelerate(vx, vy, tgt, sub_frame);
            speed = std::sqrt(vx * vx + vy * vy);
        }
    }

    // =========================================================================
    //  SURF-SLIDE (PixelSurf / TextureBug)
    //  Steer the command view parallel to a nearby wall (+/- a small turn angle)
    //  and strafe into it so the player slides along it. Ported from
    //  functions needed/{pixelsurf,TextureBug + PixelSurf}.txt (NOT velocity).
    //  magic = turn strength, scan = probe distance. Both features share this;
    //  only the params differ (PixelSurf 29.9/5, TextureBug 27.8/distance).
    // =========================================================================
    void surf_slide(std::uintptr_t cmd, std::uintptr_t pbtn, std::uintptr_t base,
        std::uintptr_t pawn, float magic, float scan_dist)
    {
        if (!aligned_ptr(base) || !g_trace_ready_cache) return;
        const auto scene = rd<std::uintptr_t>(pawn + schema::client::m_pGameSceneNode());
        if (!aligned_ptr(scene)) return;
        const auto va = rd<std::uintptr_t>(base + cs2::usercmd::viewangles);
        if (!aligned_ptr(va)) return;

        const auto origin = rd<trace::Vec3>(scene + schema::client::m_vecAbsOrigin());
        const auto vel = rd<trace::Vec3>(pawn + schema::client::m_vecVelocity());
        const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        const float pitch = rd<float>(va + cs2::usercmd::va_x);
        const float cur_yaw = strafe_norm(rd<float>(va + cs2::usercmd::va_y));
        const bool slipping = vel.z >= -7.0f && vel.z <= -6.0f;

        float turn = 10.0f;
        if (speed >= 60.0f)
            turn = std::asin(std::clamp(magic / speed, -1.0f, 1.0f)) * kRad2Deg;

        const bool vel_rev = speed >= 5.0f &&
            std::fabs(strafe_norm(std::atan2(vel.y, vel.x) * kRad2Deg - cur_yaw)) > 90.0f;

        const trace::Vec3 mins{ -16.0f, -16.0f, 0.0f };
        const trace::Vec3 maxs{ 16.0f, 16.0f, 72.0f };

        float best_dist = 3.4e38f, best_target = cur_yaw;
        bool best_side = false, found = false;

        const trace::Vec3 dirs[4] = { {1,0,0},{0,1,0},{-1,0,0},{0,-1,0} };
        for (const auto& d : dirs) {
            const trace::Vec3 end{ origin.x + d.x * scan_dist, origin.y + d.y * scan_dist, origin.z };
            const auto tr = trace::player_bbox(origin, end, mins, maxs,
                reinterpret_cast<void*>(pawn), 0x1C3003);
            if (tr.fraction >= 1.0f || tr.all_solid) continue;
            if (std::fabs(tr.normal.z) >= 0.7f) continue;

            const float normal_yaw = std::atan2(tr.normal.y, tr.normal.x) * kRad2Deg;
            const float first = strafe_norm(normal_yaw + 90.0f);
            const float second = strafe_norm(normal_yaw - 90.0f);
            const bool side = std::fabs(strafe_norm(second - cur_yaw)) < std::fabs(strafe_norm(first - cur_yaw));
            const float parallel = side ? second : first;
            const bool add_turn = slipping ^ vel_rev ^ side;
            const float target = strafe_norm(parallel + (add_turn ? turn : -turn));
            const float dx = tr.end_pos.x - origin.x, dy = tr.end_pos.y - origin.y;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= best_dist) continue;
            best_dist = dist; best_target = target; best_side = side; found = true;
        }

        if (!found) return;

        const bool move_right = best_side ^ slipping;

        // Steer the command view parallel to the wall.
        wr<float>(va + cs2::usercmd::va_x, std::clamp(pitch, -89.0f, 89.0f));
        wr<float>(va + cs2::usercmd::va_y, best_target);
        wr<float>(va + cs2::usercmd::va_z, 0.0f);
        wr<std::uint32_t>(va + 0x10, rd<std::uint32_t>(va + 0x10) | 0x1u | 0x2u | 0x4u);
        wr<std::uint32_t>(base + 0x10, rd<std::uint32_t>(base + 0x10) | 0x4u);

        write_f(base + cs2::usercmd::forwardmove, 0.0f);
        write_f(base + cs2::usercmd::leftmove, move_right ? -1.0f : 1.0f);

        const std::uint64_t movemask = cs2::buttons::IN_FORWARD | cs2::buttons::IN_BACK
            | cs2::buttons::IN_MOVELEFT | cs2::buttons::IN_MOVERIGHT;
        const std::uint64_t setbit = move_right ? cs2::buttons::IN_MOVERIGHT : cs2::buttons::IN_MOVELEFT;
        if (aligned_ptr(cmd)) {
            auto v = rd<std::uint64_t>(cmd + cs2::usercmd::bt_value);
            v = (v & ~movemask) | setbit;
            wr<std::uint64_t>(cmd + cs2::usercmd::bt_value, v);
            or_u64(cmd + cs2::usercmd::bt_value_changed, movemask);
        }
        if (pbtn) {
            auto b = rd<std::uint64_t>(pbtn + cs2::usercmd::buttonstate1);
            b = (b & ~movemask) | setbit;
            wr<std::uint64_t>(pbtn + cs2::usercmd::buttonstate1, b);
        }

        // Surf steers through analog move, so drop any queued subtick moves.
        wr<int>(base + 0x20, 0);
    }

    // =========================================================================
    //  Controllers
    // =========================================================================
    static jumpbug::Controller g_jb_controller;
    static bhop::Controller    g_bhop_controller;
    static std::int64_t        g_last_jump_tick = -1; // shared tracker for both features

    bool __fastcall hkCreateMove(void* pInput, std::uint32_t ssi, void* cmdArg)
    {
        ++gCallCount;
        const bool result = oCreateMove(pInput, ssi, cmdArg);
        if (!pInput || !cmdArg) return result;

        refresh_readiness_caches();

        const std::uintptr_t cmd = reinterpret_cast<std::uintptr_t>(cmdArg);
        void* const cmdPred = cmdArg;
        const std::uintptr_t pbtn = proto_buttons(cmdPred);
        const std::uintptr_t base = proto_base(cmdPred);
        const std::uintptr_t pawn = local_pawn();
        if (!pawn) return result;

        const std::uint32_t flags = rd<std::uint32_t>(pawn + schema::client::m_fFlags());
        const bool onground = (flags & cs2::FL_ONGROUND) != 0;
        const float velZ = rd<float>(pawn + schema::client::m_vecVelocity() + 8);
        const bool falling = velZ < 0.0f;

        const std::uint8_t moveType = rd<std::uint8_t>(pawn + schema::client::m_nActualMoveType());
        const bool badMoveType = (moveType == cs2::movetype::LADDER
            || moveType == cs2::movetype::NOCLIP || moveType == cs2::movetype::OBSERVER);

        static bool prevEnd = false;
        const bool endDown = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
        if (endDown && !prevEnd) {
            dbg::log("\n[MOVE] cmd=%llX base=%llX pawn=%llX ongr=%d velZ=%.2f predReady=%d subtick=%d traceReady=%d\n",
                (unsigned long long)cmd, (unsigned long long)base, (unsigned long long)pawn,
                onground ? 1 : 0, velZ,
                g_pred_ready_cache ? 1 : 0,
                (gSubtickCreate && !gSubtickDisabled) ? 1 : 0,
                g_trace_ready_cache ? 1 : 0);
        }
        prevEnd = endDown;

        if (globals::show) {
            if (aligned_ptr(cmd)) {
                write_u64(cmd + cs2::usercmd::bt_value, 0);
                write_u64(cmd + cs2::usercmd::bt_value_changed, 0);
            }
            if (pbtn) {
                write_u64(pbtn + cs2::usercmd::buttonstate1, 0);
                write_u64(pbtn + cs2::usercmd::buttonstate2, 0);
                write_u64(pbtn + cs2::usercmd::buttonstate3, 0);
            }
            return result;
        }
        if (badMoveType) return result;

        // --- Prediction ---
        const bool bhop_on = bind_active(settings::movement::bhop);
        const bool jb_on = bind_active(settings::movement::jumpbug);
        const bool strafe_on = bind_active(settings::movement::autostrafe);
        const bool edge_on = bind_active(settings::movement::edgebug)
            || bind_active(settings::movement::smart_edgebug);
        const bool ps_on = bind_active(settings::movement::pixelsurf);
        const bool tb_on = bind_active(settings::movement::texturebug);

        // Prediction now enters via velocity's sig path (SEH-guarded, disables
        // itself if it ever faults). Predict 1 tick for bhop (to catch the landing
        // tick and jump at the exact subtick fraction -> perfect auto-hop, the
        // Prediction BHop.txt method) and for autostrafe; several ticks for edgebug
        // so the predicted edge-clip (velz snapping to ~-7) is caught ahead of time.
        int max_pred_ticks = 0;
        if (bhop_on && !onground) max_pred_ticks = std::max(max_pred_ticks, 1);
        if (strafe_on && !onground) max_pred_ticks = std::max(max_pred_ticks, 1);
        if (edge_on && !onground && falling) max_pred_ticks = std::max(max_pred_ticks, 64);

        prediction::Result pr{};
        if (max_pred_ticks > 0 && g_pred_ready_cache)
            pr = prediction::run(cmdPred, max_pred_ticks);

        const std::uint64_t live_btns = aligned_ptr(cmd)
            ? rd<std::uint64_t>(cmd + cs2::usercmd::bt_value) : 0;
        const bool user_jump = (live_btns & cs2::buttons::IN_JUMP) != 0;

        // =====================================================================
        //  JUMPBUG (priority) + BHOP
        //  Jumpbug runs first. If it decides to own the jump this tick,
        //  bhop yields (jump_owned_elsewhere = true) so they never fight.
        // =====================================================================
        const int cmd_num = rd<int>(cmd + cs2::usercmd::cmd_number);
        const std::int64_t tick_now = cmd_num;
        bool jumpbug_owns = false;

        // Common motion data (shared by both controllers)
        const auto scene = rd<std::uintptr_t>(pawn + schema::client::m_pGameSceneNode());
        const auto vel = rd<trace::Vec3>(pawn + schema::client::m_vecVelocity());
        trace::Vec3 org{};
        if (aligned_ptr(scene))
            org = rd<trace::Vec3>(scene + schema::client::m_vecAbsOrigin());

        // --- JUMPBUG first (higher priority) ---
        if (jb_on && !onground && falling && user_jump)
        {
            jumpbug::Frame frame{};
            frame.command = static_cast<std::uint32_t>(cmd_num);
            frame.readable = true;
            frame.enabled = true;
            frame.movement_allowed = true;
            frame.grounded = onground;
            frame.falling = falling;
            frame.jump_held = true;
            frame.duck_held = (live_btns & cs2::buttons::IN_DUCK) != 0;
            frame.ducking = (flags & cs2::FL_DUCKING) != 0;
            frame.penalty = 0.2f;
            frame.sim_time = static_cast<float>(cmd_num) * (1.0f / 64.0f);
            // duck_ready needs realistic values; 2.0 is default duck speed in CS2
            frame.duck_speed = 2.0f;
            frame.last_duck_time = 0.0f;
            frame.duck_interval = 0.0f;
            if (g_last_jump_tick >= 0)
                frame.last_jump_tick = g_last_jump_tick;

            jumpbug::Motion motion{};
            motion.origin = { org.x, org.y, org.z };
            motion.velocity = { vel.x, vel.y, vel.z };
            motion.hull.mins = { -16.0f, -16.0f, 0.0f };
            motion.hull.maxs = {  16.0f,  16.0f, 72.0f };
            motion.gravity = 800.0f;
            motion.gravity_scale = 1.0f;

            auto jb_trace = [pawn](jumpbug::Vec3 from, jumpbug::Vec3 to, jumpbug::Hull hull)
                -> std::optional<jumpbug::Hit>
            {
                if (!g_trace_ready_cache) return std::nullopt;
                const trace::Vec3 t_from{ from.x, from.y, from.z };
                const trace::Vec3 t_to{ to.x, to.y, to.z };
                const trace::Vec3 t_min{ hull.mins.x, hull.mins.y, hull.mins.z };
                const trace::Vec3 t_max{ hull.maxs.x, hull.maxs.y, hull.maxs.z };
                const auto r = trace::player_bbox(t_from, t_to, t_min, t_max,
                    reinterpret_cast<void*>(pawn), 0x1C3003);
                if (r.fraction >= 1.0f || r.start_solid) return std::nullopt;
                jumpbug::Hit h{};
                h.fraction = r.fraction;
                h.normal = { r.normal.x, r.normal.y, r.normal.z };
                h.start_solid = r.start_solid;
                h.all_solid = r.all_solid;
                return h;
            };

            auto jb_writer = [&](const jumpbug::Batch& b) -> bool {
                // Drive both subticks AND live buttons so the engine sees the action
                for (std::size_t i = 0; i < b.count; ++i) {
                    const auto& s = b.steps[i];
                    const std::uint64_t btn = (s.key == jumpbug::Key::duck)
                        ? cs2::buttons::IN_DUCK : cs2::buttons::IN_JUMP;
                    add_subtick(base, btn, s.when, s.down);
                    // Also set live button state for the "down" events
                    if (s.down)
                        button(cmd, pbtn, btn, true);
                }
                return true;
            };

            const auto jb_res = g_jb_controller.run_with_prediction(
                frame, motion, std::span<const jumpbug::NativeStep>{}, jb_trace, jb_writer);

            jumpbug_owns = g_jb_controller.owns_jump();
            if (jumpbug_owns)
                g_last_jump_tick = tick_now;
        }
        else if (onground || !jb_on) {
            g_jb_controller.reset();
        }

        // --- BHOP (yields if jumpbug already owns the jump) ---
        if (bhop_on && user_jump)
        {
            bhop::Frame frame{};
            frame.command = static_cast<std::uint32_t>(cmd_num);
            frame.tick = tick_now;
            frame.readable = true;
            frame.enabled = true;
            frame.held = true;
            frame.movement_allowed = true;
            frame.grounded = onground;
            frame.falling = falling;
            frame.jump_owned_elsewhere = jumpbug_owns; // critical: yield to jumpbug
            frame.penalty = 0.2f;

            // Feed landing info carefully:
            // - predicted_landing only when we are still in air and prediction says we will land this tick
            // - landed only on the actual ground tick (age==0 logic in decide)
            if (!onground && pr.valid && pr.will_land && pr.land_frac > 0.0f && pr.land_frac < 1.0f) {
                frame.predicted_landing = bhop::Time{ tick_now, pr.land_frac };
            }
            if (onground) {
                frame.landed = bhop::Time{ tick_now, 0.0f };
            }

            bhop::Motion motion{};
            motion.origin = { org.x, org.y, org.z };
            motion.velocity = { vel.x, vel.y, vel.z };
            motion.hull.mins = { -16.0f, -16.0f, 0.0f };
            motion.hull.maxs = {  16.0f,  16.0f, 72.0f };
            motion.gravity = 800.0f;
            motion.gravity_scale = 1.0f;
            motion.standable_normal = 0.7f;

            auto bhop_trace = [pawn](bhop::Vec3 from, bhop::Vec3 to, bhop::Hull hull)
                -> std::optional<bhop::Hit>
            {
                if (!g_trace_ready_cache) return std::nullopt;
                const trace::Vec3 t_from{ from.x, from.y, from.z };
                const trace::Vec3 t_to{ to.x, to.y, to.z };
                const trace::Vec3 t_min{ hull.mins.x, hull.mins.y, hull.mins.z };
                const trace::Vec3 t_max{ hull.maxs.x, hull.maxs.y, hull.maxs.z };
                const auto r = trace::player_bbox(t_from, t_to, t_min, t_max,
                    reinterpret_cast<void*>(pawn), 0x1C3003);
                if (r.fraction >= 1.0f || r.start_solid) return std::nullopt;
                bhop::Hit h{};
                h.fraction = r.fraction;
                h.normal = { r.normal.x, r.normal.y, r.normal.z };
                h.start_solid = r.start_solid;
                h.all_solid = r.all_solid;
                return h;
            };

            auto bhop_writer = [&](const bhop::Batch& batch) -> bool {
                if (!batch.owns_jump)
                    return true; // nothing to do (yielded or disabled)

                // Fully own the button this tick
                button(cmd, pbtn, cs2::buttons::IN_JUMP, false);

                for (std::size_t i = 0; i < batch.count; ++i) {
                    const auto& s = batch.steps[i];
                    if (s.source < 0) {
                        add_subtick(base, cs2::buttons::IN_JUMP, s.when, s.down);
                        if (s.down)
                            button(cmd, pbtn, cs2::buttons::IN_JUMP, true);
                    }
                }
                if (!batch.jump_down)
                    button(cmd, pbtn, cs2::buttons::IN_JUMP, false);
                else
                    g_last_jump_tick = tick_now;
                return true;
            };

            g_bhop_controller.run_with_prediction(
                frame, motion, std::span<const bhop::NativeStep>{}, bhop_trace, bhop_writer);
        }
        else if (!user_jump || !bhop_on) {
            g_bhop_controller.reset();
        }

        // --- AUTOSTRAFE ---
        if (strafe_on && !onground && base && pr.valid) {
            subtick_strafe(cmd, pbtn, base, pr);
        }

        // --- EDGEBUG ---
        // When prediction detects an edge-clip ahead (vertical velocity snapping to
        // ~-7 while still airborne), duck through the clip and stop steering so the
        // player catches the edge cleanly (no fall damage / speed kept). Not taken
        // from velocity (its edgebug is poor) - uses our own prediction edge flag.
        if (edge_on && !onground && falling && pr.valid && pr.edge_hit)
        {
            const std::uint64_t movemask = cs2::buttons::IN_FORWARD | cs2::buttons::IN_BACK
                | cs2::buttons::IN_MOVELEFT | cs2::buttons::IN_MOVERIGHT;
            if (aligned_ptr(cmd)) and_u64(cmd + cs2::usercmd::bt_value, ~movemask);
            if (pbtn)             and_u64(pbtn + cs2::usercmd::buttonstate1, ~movemask);
            if (base) {
                write_f(base + cs2::usercmd::forwardmove, 0.0f);
                write_f(base + cs2::usercmd::leftmove, 0.0f);
            }
            button(cmd, pbtn, cs2::buttons::IN_DUCK, true);
            add_subtick(base, cs2::buttons::IN_DUCK, 0.0f, true);
        }

        // --- PIXELSURF / TEXTUREBUG (shared surf-slide, different params) ---
        if ((ps_on || tb_on) && !onground && base)
        {
            const bool slip = velZ >= -7.0f && velZ <= -6.0f;
            if (ps_on)
                surf_slide(cmd, pbtn, base, pawn, slip ? 30.006001f : 29.933001f, 5.0f);
            else
                surf_slide(cmd, pbtn, base, pawn, 27.866333f, 20.0f);
        }

        return result;
    }

    bool __fastcall hkMouseInputEnabled(void* thisptr) {
        if (globals::show) return false;
        return oMouseInput(thisptr);
    }
}

bool movement::initialize()
{
    if (gInstalled) return true;
    gClientBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("client.dll"));
    if (!gClientBase) return false;

    schema::initialize();

    gCreateMoveTarget = reinterpret_cast<void*>(sig::find("client.dll", cs2::sigs::CreateMove));
    gMouseTarget = reinterpret_cast<void*>(sig::find("client.dll", cs2::sigs::MouseInputEnabled));
    const auto fn = sig::find("client.dll", cs2::sigs::CreateNewSubtickMoveStep);
    if (fn) gSubtickCreate = reinterpret_cast<SubtickCreate_t>(fn);

    bool any = false;
    if (gCreateMoveTarget &&
        MH_CreateHook(gCreateMoveTarget, reinterpret_cast<void*>(&hkCreateMove),
            reinterpret_cast<void**>(&oCreateMove)) == MH_OK &&
        MH_EnableHook(gCreateMoveTarget) == MH_OK) any = true;
    if (gMouseTarget &&
        MH_CreateHook(gMouseTarget, reinterpret_cast<void*>(&hkMouseInputEnabled),
            reinterpret_cast<void**>(&oMouseInput)) == MH_OK &&
        MH_EnableHook(gMouseTarget) == MH_OK) any = true;

    dbg::log("[movement] init createMove=%llX mouse=%llX subtick=%llX installed=%d\n",
        (unsigned long long)(std::uintptr_t)gCreateMoveTarget,
        (unsigned long long)(std::uintptr_t)gMouseTarget,
        (unsigned long long)(std::uintptr_t)gSubtickCreate, any ? 1 : 0);
    gInstalled = any;
    return gInstalled;
}

void movement::shutdown()
{
    if (gCreateMoveTarget) { MH_DisableHook(gCreateMoveTarget); MH_RemoveHook(gCreateMoveTarget); }
    if (gMouseTarget) { MH_DisableHook(gMouseTarget);      MH_RemoveHook(gMouseTarget); }
    gInstalled = false;
}

bool movement::is_hooked() { return gInstalled; }
unsigned long movement::hook_calls() { return gCallCount; }