#pragma once
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace jumpbug {
    constexpr float grid = 1.0f / 64.0f;
    constexpr float interval = 1.0f / 64.0f;
    constexpr std::size_t capacity = 32;
    constexpr float probe = 2.0f;
    constexpr float stand_clearance = 1.0f;
    constexpr float standable = 0.7f;
    constexpr float max_contact = 4.0f;
    constexpr float highest_press = 1.0f - grid;
    constexpr float highest_leading = 1.0f - 2.0f * grid;
    constexpr float landing_offset = 3.0f * grid;
    constexpr float min_duck_speed = 1.5f;

    struct Vec3 {
        float x{}, y{}, z{};
        Vec3 operator+(Vec3 b) const { return { x + b.x, y + b.y, z + b.z }; }
        Vec3 operator*(float scale) const { return { x * scale, y * scale, z * scale }; }
    };
    struct Hull { Vec3 mins{}, maxs{}; };
    struct Motion { Vec3 origin{}, velocity{}; Hull hull{}; float gravity{}, gravity_scale{}; };
    struct Hit { float fraction{ 1.0f }; Vec3 normal{}; bool start_solid{}, all_solid{}; };
    struct Contact { float ticks{}; float fall{}; float drop{}; };

    inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    inline bool finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
    inline float snap(float when) { return (when + 131072.0f) - 131072.0f; }

    template<class Trace>
    std::optional<Contact> predict_contact(const Motion& motion, Trace&& trace) {
        const Hull hull = motion.hull;
        if (!finite(motion.origin) || !finite(motion.velocity) || !finite(hull.mins) || !finite(hull.maxs)
            || hull.mins.x >= hull.maxs.x || hull.mins.y >= hull.maxs.y || hull.mins.z >= hull.maxs.z
            || !std::isfinite(motion.gravity) || !std::isfinite(motion.gravity_scale)
            || motion.gravity < 0.0f || motion.gravity_scale < 0.0f)
            return {};
        Vec3 velocity = motion.velocity;
        velocity.z -= motion.gravity * motion.gravity_scale * interval;
        const Vec3 travel = velocity * interval;
        const Vec3 end = motion.origin + travel + Vec3{ 0.0f, 0.0f, -probe };
        if (!finite(end)) return {};
        const auto hit = trace(motion.origin, end, hull);
        if (!hit || hit->start_solid || hit->all_solid || !finite(hit->normal)
            || !(hit->fraction > 0.0f && hit->fraction < 1.0f)
            || !(hit->normal.z >= standable)) return {};
        const float tick = dot(hit->normal, travel);
        const float segment = tick - probe * hit->normal.z;
        if (!(tick < 0.0f)) return {};
        const float ticks = hit->fraction * segment / tick;
        if (!(ticks > 0.0f && ticks < max_contact)) return {};
        return Contact{ ticks, -tick / hit->normal.z, -travel.z };
    }

    enum class Shape { in_tick, across, across_jump };
    struct Plan {
        Shape shape{}; float duck{}, stand{}, landing{}; bool landing_edge{};
        std::size_t steps() const { return shape == Shape::in_tick ? (landing_edge ? 6 : 5) : 2; }
    };

    inline float after_contact(float contact) {
        return std::max(std::min(snap(contact + landing_offset), highest_leading), contact + grid);
    }

    std::optional<Plan> plan_in_tick(Contact c, float floor, float penalty) {
        if (!(c.fall > 0.0f) || !(c.ticks > 0.0f && c.ticks < 1.0f + probe / c.fall)) return {};
        const float duck = std::max(floor, 0.0f);
        const float contact = snap(c.ticks);
        const float aimed = c.ticks - stand_clearance / c.fall;
        const float aimed_point = aimed > 0.0f ? std::floor(aimed / grid) * grid : 0.0f;
        const float stand = std::max(std::min(aimed_point, highest_leading), duck + grid);
        if (!(stand < contact) || !(stand <= highest_press)) return {};
        if (!((c.ticks - duck) * c.fall > probe)) return {};
        const float clearance = (c.ticks - stand) * c.fall;
        if (!(clearance > 0.0f && clearance < probe)) return {};
        const float landing = after_contact(c.ticks < 1.0f ? contact : stand);
        if (!(landing > stand) || !(landing <= highest_press)) return {};
        return Plan{ Shape::in_tick, duck, stand, landing, (landing - stand) * interval > penalty };
    }

    std::optional<Plan> plan_across(Contact c, float floor) {
        if (!(c.fall > 0.0f) || !(c.ticks >= 1.0f && c.ticks < 1.0f + probe / c.fall)) return {};
        const float duck = std::max(floor, 0.0f);
        if (!((c.ticks - duck) * c.fall > probe) || !(duck + grid <= 1.0f)) return {};
        if (!((c.ticks - 1.0f) * c.fall >= stand_clearance)) return {};
        return Plan{ Shape::across, duck, 1.0f, 0.0f, false };
    }

    struct Frame {
        std::uint32_t command{};
        bool readable{}, enabled{}, movement_allowed{}, grounded{}, falling{};
        bool input_captured{};
        bool jump_held{}, duck_held{};
        bool ducking{};
        float penalty{};
        float sim_time{};
        float duck_speed{}, last_duck_time{}, duck_interval{};
        std::optional<std::int64_t> last_jump_tick;
        std::optional<Contact> contact;
    };

    enum class Key : std::uint8_t { other, jump, duck };
    struct NativeStep { float when{}; Key key{}; };
    struct Step { int source{ -1 }; float when{}; Key key{}; bool down{}; };
    struct Batch { std::array<Step, capacity> steps{}; std::size_t count{}; };

    struct State {
        bool attempted_this_fall{};
        std::optional<std::int64_t> attempt_jump_tick;
        std::optional<std::uint32_t> across_command;
    };

    struct Decision { std::optional<Plan> plan; State after; };

    bool duck_ready(const Frame& f) {
        return std::isfinite(f.duck_speed) && std::isfinite(f.last_duck_time)
            && std::isfinite(f.duck_interval) && f.duck_interval >= 0.0f
            && f.duck_speed >= min_duck_speed
            && (f.ducking || f.sim_time >= f.last_duck_time + f.duck_interval);
    }

    Decision decide(const State& state, const Frame& f, float floor, std::size_t room) {
        Decision result{ {}, state };
        State& next = result.after;
        if (f.grounded || (f.last_jump_tick && state.attempt_jump_tick && *f.last_jump_tick != *state.attempt_jump_tick))
            next.attempted_this_fall = false;
        const bool owed = state.across_command == f.command;
        next.across_command.reset();
        if (!f.enabled || !f.movement_allowed || !f.jump_held || f.duck_held) return result;
        std::optional<Plan> plan;
        if (owed) plan = Plan{ Shape::across_jump };
        else if (!f.grounded && f.contact && (f.falling || f.contact->drop > 0.0f) && !next.attempted_this_fall
            && duck_ready(f) && std::isfinite(f.penalty) && f.penalty >= 0.0f) {
            if (f.contact->ticks >= 1.0f) plan = plan_across(*f.contact, floor);
            if (!plan) plan = plan_in_tick(*f.contact, floor, f.penalty);
        }
        if (!plan || plan->steps() > room) return result;
        if (plan->shape != Shape::across_jump) {
            next.attempted_this_fall = true;
            next.attempt_jump_tick = f.last_jump_tick;
        }
        if (plan->shape == Shape::across) next.across_command = f.command + 1;
        result.plan = plan;
        return result;
    }

    void emit(const Plan& plan, Batch& b) {
        auto push = [&](float when, Key key, bool down) { b.steps[b.count++] = { -1, when, key, down }; };
        switch (plan.shape) {
        case Shape::in_tick:
            push(plan.duck, Key::duck, true);
            push(plan.stand, Key::duck, false);
            push(plan.stand, Key::jump, false);
            push(plan.stand, Key::jump, true);
            if (plan.landing_edge) push(plan.landing, Key::jump, false);
            push(plan.landing, Key::jump, true);
            break;
        case Shape::across:
            push(plan.duck, Key::duck, true);
            push(1.0f, Key::duck, false);
            break;
        case Shape::across_jump:
            push(0.0f, Key::jump, false);
            push(0.0f, Key::jump, true);
            break;
        }
    }

    bool merge(std::span<const NativeStep> native, const std::optional<Plan>& plan, Batch& output) {
        Batch batch{};
        if (native.size() > capacity) return false;
        for (std::size_t i = 0; i < native.size(); ++i) {
            const NativeStep step = native[i];
            if (!std::isfinite(step.when) || step.when < 0.0f || step.when > 1.0f) return false;
            if (plan && step.key != Key::other) continue;
            batch.steps[batch.count++] = { static_cast<int>(i), step.when, step.key, false };
        }
        if (plan) {
            if (batch.count + plan->steps() > capacity) return false;
            emit(*plan, batch);
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

    enum class Result { written, skipped, bad_snapshot, no_room, write_failed, restore_required };

    class Controller {
    public:
        void reset() { state_ = {}; command_.reset(); decision_ = {}; owns_ = false; }
        bool owns_jump() const { return owns_; }

        template<class Trace, class Writer>
        Result run_with_prediction(Frame frame, const Motion& motion,
            std::span<const NativeStep> native, Trace&& trace, Writer&& write) {
            if ((!command_ || frame.command != *command_) && !frame.input_captured && frame.readable && frame.enabled
                && frame.movement_allowed && frame.jump_held && !frame.duck_held && !frame.grounded)
                frame.contact = predict_contact(motion, trace);
            return run(frame, native, write);
        }

        template<class Writer>
        Result run(const Frame& frame, std::span<const NativeStep> native, Writer&& write) {
            owns_ = false;
            if (frame.input_captured) return Result::skipped;
            if (command_ && frame.command < *command_) return Result::restore_required;
            if (!frame.readable) return Result::bad_snapshot;
            if (!command_ || frame.command != *command_) {
                float floor = 0.0f;
                for (const NativeStep& step : native) floor = std::max(floor, step.when);
                const std::size_t room = native.size() < capacity ? capacity - native.size() : 0;
                command_ = frame.command;
                decision_ = decide(state_, frame, floor, room);
            }
            std::optional<Plan> plan = decision_.plan;
            if (!frame.jump_held || frame.duck_held) plan.reset();
            Batch batch{};
            if (!merge(native, plan, batch)) return Result::no_room;
            if (!write(batch)) return Result::write_failed;
            owns_ = plan.has_value();
            state_ = decision_.after;
            return Result::written;
        }

    private:
        State state_{};
        Decision decision_{};
        std::optional<std::uint32_t> command_;
        bool owns_{};
    };
} // namespace jumpbug