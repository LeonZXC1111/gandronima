#pragma once
//
// strafe.h - AutoStrafer math (port of functions needed/AutoStrafer.txt).
//
// Pure, self-contained C++20 air-strafe solver. No game offsets here: the caller
// fills a strafe::Frame from the live player state each tick and supplies a
// Writer that applies the produced turns to the outgoing command. Integration
// glue (reading velocity/yaw, writing subtick turns) lives in movement.cpp.
//
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

// Windows.h (included before us by movement.cpp) defines min/max macros that
// break std::min / std::max. Neutralize them for this header.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif


namespace strafe {
constexpr std::size_t capacity = 32;
constexpr float rad = 0.017453292519943295f;

struct Vec2 {
    float x{}, y{};
    Vec2 operator+(Vec2 b) const { return {x + b.x, y + b.y}; }
    Vec2 operator-(Vec2 b) const { return {x - b.x, y - b.y}; }
    Vec2 operator*(float k) const { return {x * k, y * k}; }
};
inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length(Vec2 v) { return std::hypot(v.x, v.y); }
inline bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
inline float wrap(float yaw) { return std::remainder(yaw, 360.0f); }
inline float snap(float when) { return (when + 131072.0f) - 131072.0f; }
inline bool keyboard(float v) { return v == -1.0f || v == 0.0f || v == 1.0f; }

struct NativeStep { float when{}, yaw_delta{}, forward_delta{}, left_delta{}; };
struct Turn { float when{}, yaw_delta{}; };
struct Frame {
    std::uint32_t command{};
    bool readable{}, enabled{}, airborne{}, movement_allowed{};
    bool contact_this_tick{}, using_item{}, basis_owned_elsewhere{};
    Vec2 velocity{};
    float command_yaw{}, opening_forward{}, opening_left{}, forward{}, left{}; // Opening axes: m_flCmdForwardMove/m_flCmdLeftMove
    float air_accelerate{}, speed_bound{}, air_cap{}, friction{}, interval{};
    std::size_t reserved_steps{}; // Other accepted producers; exclude native.size().
};
struct Seed {
    Vec2 velocity{};
    float forward{}, left{};
    int side{1};
};
struct Plan {
    std::array<Turn, capacity> turns{};
    std::size_t count{};
    Vec2 velocity{};
    int side{1};
};
enum class Result { written, skipped, bad_snapshot, bad_prefix, no_room, write_failed, restore_required };

inline Vec2 accelerate(Vec2 velocity, Vec2 wish, float cap, float budget) {
    const float applied = std::max(0.0f, std::min(budget, cap - dot(velocity, wish)));
    return velocity + wish * applied;
}

inline Vec2 solve(Vec2 velocity, Vec2 wanted, float cap, float budget, float response_speed, int& side) {
    const float speed = length(velocity);
    if (speed == 0.0f) return wanted;
    if (budget < 2.0f * cap) {
        const Vec2 difference = wanted * response_speed - velocity;
        const float distance = length(difference);
        const float across = velocity.x * wanted.y - velocity.y * wanted.x;
        if ((dot(velocity, wanted) <= 0.0f || std::abs(across) > budget) && distance >= budget) {
            const Vec2 wish = difference * (1.0f / distance);
            if (dot(velocity, wish) <= cap - budget) return wish;
        }
    }
    const Vec2 unit = velocity * (1.0f / speed);
    const Vec2 across{-unit.y, unit.x};
    const float toward = dot(across, wanted);
    side = toward > 0.0f ? 1 : toward < 0.0f ? -1 : -side;
    const float impulse = std::min(budget, 2.0f * cap);
    const float cosine_limit = 1.0f - impulse * impulse / (2.0f * speed * speed);
    const float projection = dot(unit, wanted) < cosine_limit ? -impulse * 0.5f : std::max(0.0f, cap - budget);
    const float cosine = std::clamp(projection / speed, -1.0f, 1.0f);
    const float sine = std::sqrt(std::max(0.0f, 1.0f - cosine * cosine));
    return unit * cosine + across * (static_cast<float>(side) * sine);
}

inline bool valid(const Frame& f) {
    return f.readable && finite(f.velocity) && std::isfinite(length(f.velocity))
        && std::isfinite(f.command_yaw) && keyboard(f.opening_forward) && keyboard(f.opening_left)
        && keyboard(f.forward) && keyboard(f.left)
        && std::isfinite(f.air_accelerate) && f.air_accelerate > 0.0f
        && std::isfinite(f.speed_bound) && f.speed_bound > 0.0f
        && std::isfinite(f.air_cap) && f.air_cap > 0.0f
        && std::isfinite(f.friction) && f.friction > 0.0f
        && std::isfinite(f.interval) && f.interval > 0.0f;
}

inline bool replay(const Frame& f, const Seed& seed, std::span<const NativeStep> native,
    float cap, float budget, Vec2& velocity, float& yaw, float& start) {
    velocity = seed.velocity;
    float forward = seed.forward, left = seed.left;
    yaw = wrap(f.command_yaw);
    start = 0.0f;
    float previous_raw = 0.0f;
    for (const auto& step : native) {
        if (!(step.when >= previous_raw && step.when <= 1.0f)
            || !std::isfinite(step.yaw_delta) || !std::isfinite(step.forward_delta)
            || !std::isfinite(step.left_delta)) return false;
        previous_raw = step.when;
        const float when = snap(step.when);
        yaw = wrap(yaw + wrap(step.yaw_delta));
        const float magnitude = std::hypot(forward, left);
        if (magnitude > 0.0f && when > start) {
            const float c = std::cos(yaw * rad), s = std::sin(yaw * rad);
            const Vec2 wish{(forward * c - left * s) / magnitude, (forward * s + left * c) / magnitude};
            velocity = accelerate(velocity, wish, cap, budget * (when - start));
        }
        // Yaw governs the ending interval; input deltas apply after it
        forward += step.forward_delta;
        left += step.left_delta;
        if (!keyboard(forward) || !keyboard(left)) return false;
        start = when;
    }
    return forward == f.forward && left == f.left && finite(velocity);
}

inline Result build(const Frame& f, const Seed& seed, std::span<const NativeStep> native, Plan& output) {
    if (native.size() >= capacity || f.reserved_steps >= capacity - native.size()) return Result::no_room;
    const float cap = std::min(f.air_cap, f.speed_bound);
    const float budget = f.air_accelerate * f.speed_bound * f.friction * f.interval;
    if (!std::isfinite(budget) || !(budget > 0.0f)) return Result::bad_snapshot;
    Plan plan;
    plan.side = seed.side;
    float basis{}, start{};
    if (!replay(f, seed, native, cap, budget, plan.velocity, basis, start)) return Result::bad_prefix;
    if (!(start < 1.0f)) return Result::no_room;
    const float bearing = std::atan2(f.left, f.forward) / rad;
    const float wanted_angle = wrap(basis + bearing) * rad;
    const Vec2 wanted{std::cos(wanted_angle), std::sin(wanted_angle)};
    const float response = std::max(length(plan.velocity), f.speed_bound);
    if (!std::isfinite(response)) return Result::bad_snapshot;
    const std::size_t slices = capacity - native.size() - f.reserved_steps;
    float previous = start;
    for (std::size_t i = 1; i <= slices; ++i) {
        const float when = i == slices ? 1.0f : snap(start + (1.0f - start) * static_cast<float>(i) / static_cast<float>(slices));
        if (when <= previous) continue;
        const float acceleration = budget * (when - previous);
        const Vec2 wish = solve(plan.velocity, wanted, cap, acceleration, response, plan.side);
        const float yaw = wrap(std::atan2(wish.y, wish.x) / rad - bearing);
        const float turn = wrap(yaw - basis);
        if (!finite(wish) || !std::isfinite(turn)) return Result::bad_snapshot;
        plan.turns[plan.count++] = {when, turn};
        plan.velocity = accelerate(plan.velocity, wish, cap, acceleration);
        basis = yaw;
        previous = when;
    }
    if (!finite(plan.velocity)) return Result::bad_snapshot;
    output = plan;
    return Result::written;
}

class Controller {
public:
    void reset() { command_.reset(); side_ = 1; }
    int checkpoint() const { return side_; }
    void restore(int side) { reset(); side_ = side < 0 ? -1 : 1; }

    // Writer replaces our turns atomically, preserving native steps and base view angles.
    template<class Writer>
    Result run(const Frame& frame, std::span<const NativeStep> native, Writer&& write) {
        if (!valid(frame)) return Result::bad_snapshot;
        if (command_ && frame.command < *command_) return Result::restore_required;
        if (!command_ || frame.command != *command_) {
            command_ = frame.command;
            seed_ = {frame.velocity, frame.opening_forward, frame.opening_left, side_};
        }
        if (!frame.enabled || !frame.airborne || !frame.movement_allowed || frame.contact_this_tick
            || frame.using_item || frame.basis_owned_elsewhere || (frame.forward == 0.0f && frame.left == 0.0f))
            return Result::skipped;
        Plan plan;
        const auto result = build(frame, seed_, native, plan);
        if (result != Result::written) return result;
        if (!write(plan)) return Result::write_failed;
        side_ = plan.side;
        return Result::written;
    }

private:
    std::optional<std::uint32_t> command_;
    Seed seed_{};
    int side_{1};
};
} // namespace strafe


