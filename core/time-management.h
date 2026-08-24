#pragma once
#include "../common.h"
#include "engine-main.h"

#include <algorithm>
#include <chrono>
#include <vector>

struct TimeRecord {
	u64 timeMs = 0; // Cumulative time at this depth.
	u64 nodes = 0;  // Cumulative nodes at this depth.
	move pv{};
	short eval = 0;
};

class TimeManager {
public:
	explicit TimeManager(uint maxDepth = 64) :
		maxDepth(maxDepth),
		records(maxDepth + 1) {
	}

	// timeLeft is the clock before the current move.
	void setClock(u64 timeLeft, u64 increment, uint mtg = 0) {
		remainingMs = timeLeft;
		incrementMs = increment;
		movesToGo = mtg;
	}

	void startSearch() {
		searchStart = Clock::now();
		searching = true;
		lastDepth = 0;
		std::fill(records.begin(), records.end(), TimeRecord{});
		calculateLimits();
	}

	void finishDepth(uint depth, move pv, short eval, u64 nodes = 0) {
		if (!searching || depth == 0 || depth > maxDepth)
			return;
		records[depth] = { elapsedMs(), nodes, pv, eval };
		lastDepth = depth;
	}

	bool shouldContinue(uint nextDepth) const {
		if (nextDepth == 0 || nextDepth > maxDepth)
			return false;
		if (nextDepth <= 4) return true;

		const u64 elapsed = elapsedMs();
		if (elapsed >= hardMs)
			return false;

		if (nextDepth <= 5 && elapsed < hardMs / 4)
			return true;

		const int evalChange = evaluationChange();
		const float stability = pvStability();

		// stable; stop early, unstable; allow more
		float factor = 0.55f;
		if (stability >= 0.75f && evalChange <= 12)
			factor = 0.45f;
		else if (stability < 0.50f || evalChange >= 30)
			factor = 0.80f;

		const u64 softThreshold = max(5ul, (u64)(softMs * factor));
		if (elapsed >= softThreshold)
			return false;

		const u64 projected = projectedCost(nextDepth);
		const u64 ceiling = (factor > 0.7f) ? hardMs : softMs;
		if (elapsed + projected > ceiling)
			return false;

		return true;
	}

	u64 elapsedMs() const {
		if (!searching)
			return 0;

		return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - searchStart).count();
	}

	// Member variables
	uint maxDepth;
	std::vector<TimeRecord> records;
	Clock::time_point searchStart{};

	u64 remainingMs = 0;
	u64 incrementMs = 0;
	u64 safetyReserveMs = 100;

	uint movesToGo = 40;
	uint lastDepth = 0;

	u64 softMs = 50;
	u64 hardMs = 100;

	bool searching = false;

private:
	void calculateLimits() {
		safetyReserveMs = std::clamp(remainingMs / 25, 30ul, 500ul);

		if (remainingMs <= safetyReserveMs) {
			softMs = max(1ul, remainingMs / 3);
			hardMs = remainingMs;
			return;
		}

		const u64 spendable = remainingMs - safetyReserveMs;

		// default to 35 moves in sudden death
		const uint effectiveMtg = movesToGo > 0 ? min(movesToGo, 45u) : 35;
		const u64 base = spendable / effectiveMtg;

		softMs = base + (incrementMs * 3) / 4; // partial increment
		softMs = std::clamp(softMs, 10ul, max(20ul, spendable));

		hardMs = std::clamp(softMs * 2, softMs, max(50ul, (spendable * 3) / 2));
	}

	u64 projectedCost(uint nextDepth) const {
		if (nextDepth <= lastDepth)
			return 0;
		if (lastDepth <= 1)
			return 5;

		u64 lastIterationMs = records[lastDepth].timeMs - records[lastDepth - 1].timeMs;

		const float growth = estimateGrowth();
		float proj = (float)lastIterationMs;
		for (uint d = lastDepth; d < nextDepth && d < lastDepth + 4; ++d)
			proj *= growth;

		return (u64)proj;
	}

	// Effective branching factor from recent node counts
	float estimateGrowth() const {
		if (lastDepth <= 3)
			return 1.8f;

		float ratios[3];
		uint count = 0;

		const uint first = lastDepth - 2;
		for (uint d = first; d <= lastDepth; ++d) {
			const u64 prev = records[d - 1].nodes;
			const u64 curr = records[d].nodes;

			if (prev == 0 || curr == 0)
				continue;

			ratios[count++] = std::clamp((float)curr / prev, 1.2f, 4.0f);
		}

		if (count == 0)
			return 1.8f;

		std::sort(ratios, ratios + count);
		return ratios[count / 2];
	}

	float pvStability() const {
		if (lastDepth <= 5)
			return 0.0f;

		const move ref = records[lastDepth].pv;
		if (ref.from == 0 && ref.to == 0)
			return 0.0f;

		uint valid = 0;
		uint matching = 0;

		for (uint d = lastDepth - 4; d <= lastDepth; ++d) {
			const move pv = records[d].pv;
			if (pv.from == 0 && pv.to == 0)
				continue;

			++valid;
			if (pv.from == ref.from && pv.to == ref.to)
				++matching;
		}

		return valid > 0 ? (float)matching / valid : 0.0f;
	}

	int evaluationChange() const {
		if (lastDepth <= 4)
			return 0;
		return std::abs((int)records[lastDepth].eval - (int)records[lastDepth - 1].eval);
	}
};
