#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

// Windows.h min/max macros break std::min/max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace bhop {
constexpr float grid = 1.0f / 64.0f;
constexpr float interval = 1.0f / 64.0f;
constexpr std::size_t capacity = 32;

struct Time {
    std::int64_t tick{};
    float fraction{};
    bool operator==(const Time&) const = default;
};

struct Vec3 {
    float x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator*(float scale) const { return {x * scale, y * scale, z * scale}; }
};
struct Hull { Vec3 mins{}, maxs{}; };
struct Motion {
    Vec3 origin{}, velocity{};
    Hull hull{};
    float gravity{}, gravity_scale{}, standable_normal{}; // read from game state
};
struct Hit {
    float fraction{1.0f};
    Vec3 normal{};
    bool start_solid{}, all_solid{};
};

bool finite(Vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

template<class Trace>
std::optional<Time> predict_landing(std::int64_t tick, const Motion& motion, Trace&& trace) {
    const Hull hull = motion.hull;
    if (tick < 0 || !finite(motion.origin) || !finite(motion.velocity)
        || !finite(hull.mins) || !finite(hull.maxs)
        || hull.mins.x >= hull.maxs.x || hull.mins.y >= hull.maxs.y || hull.mins.z >= hull.maxs.z
        || !std::isfinite(motion.gravity) || !std::isfinite(motion.gravity_scale)
        || motion.gravity < 0.0f || motion.gravity_scale < 0.0f
        || !(motion.standable_normal > 0.0f && motion.standable_normal <= 1.0f))
        return {};
    Vec3 velocity = motion.velocity;
    velocity.z -= motion.gravity * motion.gravity_scale * interval;
    const Vec3 end = motion.origin + velocity * interval;
    if (!(velocity.z < 0.0f) || !finite(end))
        return {};
    const auto hit = trace(motion.origin, end, hull);
    if (!hit || hit->start_solid || hit->all_solid || !finite(hit->normal)
        || !(hit->fraction > 0.0f && hit->fraction < 1.0f)
        || hit->normal.z < motion.standable_normal)
        return {};
    return Time{tick, hit->fraction};
}

struct Frame {
    std::uint32_t command{};
    std::int64_t tick{};
    bool readable{}, enabled{}, held{}, movement_allowed{}, grounded{}, falling{};
    bool jump_owned_elsewhere{};
    float penalty{}; // replicated sv_jump_spam_penalty_time, in seconds
    std::optional<Time> actual_press;
    std::optional<Time> landed;
    std::optional<Time> predicted_landing;
};

struct NativeStep {
    float when{};
    bool jump{};
};

struct State {
    std::optional<Time> last_press;
    std::optional<Time> first;
    std::optional<Time> completed_followup;
};

struct Decision {
    bool owns_jump{};
    std::optional<float> when;
    State after;
};

struct Step { int source{-1}; float when{}; bool down{}; };
struct Batch {
    std::array<Step, capacity> steps{};
    std::size_t count{};
    bool owns_jump{}, jump_down{};
};

bool valid(Time t) {
    return t.tick >= 0 && std::isfinite(t.fraction)
        && t.fraction >= 0.0f && t.fraction < 1.0f;
}

float quantize(float fraction) {
    return std::round(fraction * 64.0f) * grid;
}

float first_when(float contact) {
    return contact + grid <= 62.0f * grid
        ? std::min(contact + 3.0f * grid, 62.0f * grid)
        : std::min(contact - grid, 62.0f * grid);
}

bool clear_of(Time now, std::optional<Time> previous, float penalty) {
    if (!previous)
        return true;
    if (now.tick < previous->tick)
        return false;
    // Subtract ticks BEFORE converting to float. Equality is rejected.
    const float elapsed = (static_cast<float>(now.tick - previous->tick)
        + now.fraction - previous->fraction) * interval;
    return elapsed > penalty;
}

bool ready(const Frame& f) {
    return f.readable && f.tick >= 0 && std::isfinite(f.penalty) && f.penalty >= 0.0f
        && (!f.actual_press || valid(*f.actual_press))
        && (!f.landed || valid(*f.landed));
}

Decision decide(const State& state, const Frame& f) {
    Decision result{false, {}, state};
    if (!f.enabled || !f.held || !f.movement_allowed || f.jump_owned_elsewhere)
        return result;
    result.owns_jump = true;
    enum class Update { press, first, followup };
    auto offer = [&](float when, Update update = Update::press) {
        const Time at{f.tick, when};
        if (!clear_of(at, f.actual_press, f.penalty)
            || !clear_of(at, state.last_press, f.penalty))
            return false;
        result.when = when;
        result.after.last_press = at;
        if (update == Update::first)
            result.after.first = at;
        if (update == Update::followup)
            result.after.completed_followup = f.landed;
        return true;
    };
    const bool has_first = f.landed && state.first && state.first->tick == f.landed->tick;
    auto followup = [&] {
        const float first = has_first ? state.first->fraction : 0.0f;
        return offer(std::clamp(quantize(std::max(f.landed->fraction, first) + grid),
            grid, 63.0f * grid), Update::followup);
    };

    const auto age = f.landed ? f.tick - f.landed->tick : -1;
    if (f.landed) {
        const float contact = quantize(f.landed->fraction);
        if (age == 0 && offer(first_when(contact), Update::first))
            return result;
        if (f.grounded && age == 1) {
            if (!has_first && contact > 49.0f * grid && offer(grid, Update::followup))
                return result;
            if (followup())
                return result;
        }
    }
    if (!f.grounded && f.falling && f.predicted_landing && valid(*f.predicted_landing)) {
        const float contact = quantize(f.predicted_landing->fraction);
        if (f.predicted_landing->tick == f.tick && contact + grid <= 62.0f * grid
            && offer(first_when(contact), Update::first))
            return result;
    }

    if (f.grounded && !(age >= 0 && age <= 1) && offer(0.0f))
        return result;
    if (f.grounded && age >= 2 && age <= 5 && state.completed_followup != f.landed)
        followup();
    return result;
}

bool merge(std::span<const NativeStep> native, const Decision& decision, Batch& output) {
    Batch batch{};
    batch.owns_jump = decision.owns_jump;
    batch.jump_down = decision.when.has_value();
    if (native.size() > capacity)
        return false;
    for (std::size_t i = 0; i < native.size(); ++i) {
        const NativeStep step = native[i];
        if (!std::isfinite(step.when) || step.when < 0.0f || step.when > 1.0f)
            return false;
        if (decision.owns_jump && step.jump)
            continue;
        batch.steps[batch.count++] = {static_cast<int>(i), step.when, false};
    }
    if (decision.when) {
        if (batch.count + 2 > capacity)
            return false;
        batch.steps[batch.count++] = {-1, *decision.when, false};
        batch.steps[batch.count++] = {-1, *decision.when, true};
    }
    for (std::size_t i = 1; i < batch.count; ++i) {
        const Step value = batch.steps[i];
        std::size_t j = i;
        while (j != 0 && batch.steps[j - 1].when > value.when) {
            batch.steps[j] = batch.steps[j - 1];
            --j;
        }
        batch.steps[j] = value;
    }
    output = batch;
    return true;
}

enum class Result { written, bad_snapshot, no_room, write_failed, restore_required };

class Controller {
public:
    State checkpoint() const { return state_; }
    void restore(State state) { state_ = state; command_.reset(); }
    void reset() { restore({}); } // disconnect, respawn, pawn/session change

    template<class Trace, class Writer>
    Result run_with_prediction(Frame frame, const Motion& motion,
        std::span<const NativeStep> native, Trace&& trace, Writer&& write) {
        if ((!command_ || frame.command != *command_) && ready(frame)
            && frame.enabled && frame.held && frame.movement_allowed
            && !frame.jump_owned_elsewhere && !frame.grounded && frame.falling)
            frame.predicted_landing = predict_landing(frame.tick, motion, trace);
        return run(frame, native, write);
    }

    template<class Writer>
    Result run(const Frame& frame, std::span<const NativeStep> native, Writer&& write) {
        if (command_ && frame.command < *command_)
            return Result::restore_required;
        if (!command_ || frame.command != *command_) {
            if (!ready(frame))
                return Result::bad_snapshot;
            command_ = frame.command;
            decision_ = decide(state_, frame);
        }
        Batch batch{};
        if (!merge(native, decision_, batch))
            return Result::no_room;
        if (!write(batch))
            return Result::write_failed;
        state_ = decision_.after; // commit only after the entire write succeeds
        return Result::written;
    }

private:
    State state_{};
    Decision decision_{};
    std::optional<std::uint32_t> command_;
};
} // namespace bhop
