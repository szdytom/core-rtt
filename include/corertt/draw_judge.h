#ifndef CORERTT_DRAW_JUDGE_H
#define CORERTT_DRAW_JUDGE_H

#include "corertt/world.h"
#include <cstdint>
#include <memory>

namespace cr {

class RuleDrawJudge {
public:
	virtual ~RuleDrawJudge() = default;

	virtual bool shouldDraw(const World &world) noexcept = 0;
};

std::unique_ptr<RuleDrawJudge> createNoopRuleDrawJudge();

std::unique_ptr<RuleDrawJudge> createMaxTicksRuleDrawJudge(
	std::uint32_t max_ticks
);

std::unique_ptr<RuleDrawJudge> createDynamicTurnLimitRuleDrawJudge();

} // namespace cr

#endif // CORERTT_DRAW_JUDGE_H
