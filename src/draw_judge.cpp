#include "corertt/draw_judge.h"
#include <utility>

namespace cr {

namespace {

class NoopRuleDrawJudgeImpl final : public RuleDrawJudge {
public:
	bool shouldDraw(const World &world) noexcept override {
		(void)world;
		return false;
	}
};

class MaxTicksRuleDrawJudgeImpl final : public RuleDrawJudge {
public:
	explicit MaxTicksRuleDrawJudgeImpl(std::uint32_t max_ticks) noexcept
		: _max_ticks(max_ticks) {}

	bool shouldDraw(const World &world) noexcept override {
		return _max_ticks != 0 && world.currentTick() >= _max_ticks;
	}

private:
	std::uint32_t _max_ticks;
};

class DynamicTurnLimitRuleDrawJudgeImpl final : public RuleDrawJudge {
public:
	bool shouldDraw(const World &world) noexcept override {
		const auto &turn_events = world.turnEvents();

		if (!_unit_left_base_seen && turn_events.unit_left_base) {
			_unit_left_base_seen = true;
			_max_turn += 500;
		}

		if (!_unit_entered_resource_zone_seen
		    && turn_events.unit_entered_resource_zone) {
			_unit_entered_resource_zone_seen = true;
			_max_turn += 500;
		}

		if (!_unit_upgraded_or_repaired_at_base_seen
		    && turn_events.unit_upgraded_or_repaired_at_base) {
			_unit_upgraded_or_repaired_at_base_seen = true;
			_max_turn += 2000;
		}

		if (!_unit_manufactured_seen && turn_events.unit_manufactured) {
			_unit_manufactured_seen = true;
			_max_turn += 2000;
		}

		if (!_unit_destroyed_seen && turn_events.unit_destroyed) {
			_unit_destroyed_seen = true;
			_max_turn += 2000;
		}

		if (!_base_entered_capture_condition_seen
		    && turn_events.base_entered_capture_condition) {
			_base_entered_capture_condition_seen = true;
			_max_turn += 2000;
		}

		return world.currentTick() >= _max_turn;
	}

private:
	std::uint32_t _max_turn = 1000;
	bool _unit_left_base_seen = false;
	bool _unit_entered_resource_zone_seen = false;
	bool _unit_upgraded_or_repaired_at_base_seen = false;
	bool _unit_manufactured_seen = false;
	bool _unit_destroyed_seen = false;
	bool _base_entered_capture_condition_seen = false;
};

} // namespace

std::unique_ptr<RuleDrawJudge> createNoopRuleDrawJudge() {
	return std::make_unique<NoopRuleDrawJudgeImpl>();
}

std::unique_ptr<RuleDrawJudge> createMaxTicksRuleDrawJudge(
	std::uint32_t max_ticks
) {
	return std::make_unique<MaxTicksRuleDrawJudgeImpl>(max_ticks);
}

std::unique_ptr<RuleDrawJudge> createDynamicTurnLimitRuleDrawJudge() {
	return std::make_unique<DynamicTurnLimitRuleDrawJudgeImpl>();
}

} // namespace cr
