#include "../common.h"
#include "engine-main.h"

struct TimeRecord {
	u64 timeMs = 0;
	uint depth = 0;
	move pv;
	short eval = 0;
};

class TimeManager {
public:
	TimeManager(uint maxDepth)
		: maxDepth(maxDepth),
		  records(maxDepth + 1),
		  remainingMs(0),
		  incrementMs(0),
		  movesToGo(40),
		  safetyReserveMs(200),
		  lastDepth(0),
		  totalUsedMs(0),
		  searchStarted(false) {}

	// call before search begins
	void setClock(u64 timeLeft, u64 increment, uint mtg = 40) {
		remainingMs = timeLeft;
		incrementMs = increment;
		movesToGo = (mtg == 0 ? 1u : mtg);
	}

	// call when search starts
	void startSearch() {
		searchStart = Clock::now();
		searchStarted = true;
	}

	// call at the end of each completed depth (iterative deepening)
	void finishSearch(uint depth, move pv, short eval) {
		if (depth > maxDepth) return;

		u64 used = elapsedSinceStartMs();
		records[depth].timeMs = used;
		records[depth].depth = depth;
		records[depth].pv = pv;
		records[depth].eval = eval;

		lastDepth = depth;
		totalUsedMs += used;

		if (used >= remainingMs) remainingMs = 0;
		else remainingMs -= used;

		if (movesToGo > 0) --movesToGo;
	}

	// Decide whether to continue to nextDepth
	bool shouldContinue(uint nextDepth) const {
		if (nextDepth == 0 || nextDepth > maxDepth) return false;

		// If no meaningful time left, stop (after we reached some usable depth)
		if (nextDepth >= 4 && remainingMs <= safetyReserveMs + 10)
			return false;

		u64 budget = budgetForCurrentMove();
		u64 projected = projectedNextDepthCost(nextDepth);
		bool unstable = evalStable(35);

		// predicted next iteration would go into reserve
		if (projected + safetyReserveMs > budget) {
			if (unstable && remainingMs > projected + (safetyReserveMs / 2)) {
				// allow going into half of reserve for unstable positions
			} else {
				return false;
			}
		}

		// stable PV at reasonable depth
		if (lastDepth >= 14 && pvStable(0.5) && !unstable)
			return false;

		return true;
	}

	u64 getRemainingMs() const { return remainingMs; }
	uint getLastDepth() const { return lastDepth; }
	u64 getTotalUsedMs() const { return totalUsedMs; }

private:
	uint maxDepth;
	std::vector<TimeRecord> records;
	Clock::time_point searchStart;
	u64 remainingMs;
	u64 incrementMs;
	uint movesToGo;
	u64 safetyReserveMs;
	uint lastDepth;
	u64 totalUsedMs;
	bool searchStarted;

	u64 elapsedSinceStartMs() const {
		if (!searchStarted)
			return 0;
		return (u64)std::chrono::duration_cast<ms>(Clock::now() - searchStart).count();
	}

	// Budget available for this move; this is not a prediction.
	u64 budgetForCurrentMove() const {
		uint mtg = (movesToGo == 0 ? 1u : movesToGo);

		// Base share of remaining time
		u64 base = remainingMs / mtg;

		// Add increment conservatively
		u64 alloc = base + incrementMs;

		// Keep a reserve and avoid spending too much on one move
		u64 cap = std::max(remainingMs / 4, 10ul);
		return std::min(alloc, cap);
	}

	// Predict cost of reaching nextDepth from recent iteration timings.
	u64 projectedNextDepthCost(uint nextDepth) const {
		if (lastDepth < 2 || records[lastDepth].timeMs == 0)
			return std::max(10ul, incrementMs / 2);

		u64 last = records[lastDepth].timeMs;
		double growth = estimateGrowth();

		uint delta = nextDepth - lastDepth;

		double proj = double(last);
		for (uint i = 0; i < delta && i < 6; ++i)
			proj *= growth;

		if (proj < 0.0) proj = 0.0;
		if (proj > double(std::numeric_limits<u64>::max()))
			return std::numeric_limits<u64>::max();

		return (u64)proj;
	}

	// Estimate growth from recent completed depths.
	double estimateGrowth() const {
		if (lastDepth < 2) return 2.0;

		u64 a = records[lastDepth - 1].timeMs;
		u64 b = records[lastDepth].timeMs;

		if (a == 0 || b == 0) return 2.0;

		double r = double(b) / double(a);
		r += 0.2; // better safe than sorry
		if (r < 1.1) r = 1.1;
		if (r > 2.5) r = 10.0; // super-exponential at very high depths

		return r;
	}

	bool pvStable(double thresholdFraction) const {
		if (lastDepth < 2)
			return false;

		// use PV from latest completed depth as reference
		const move ref = records[lastDepth].pv;
		if (ref.from == 0 && ref.to == 0)
			return false;

		uint matches = 0;
		uint denom = 0;

		// ignore first move
		for (uint d = 2; d <= lastDepth; ++d) {
			const move pv = records[d].pv;
			if (pv.from == 0 && pv.to == 0)
				continue;
			++denom;
			if (pv.from == ref.from && pv.to == ref.to)
				++matches;
		}

		if (denom == 0)
			return false;

		double frac = double(matches) / double(denom);
		return frac >= thresholdFraction;
	}

	bool evalStable(short margin) const {
		if (lastDepth < 2) return false;
		short prev = records[lastDepth - 1].eval;
		short curr = records[lastDepth].eval;
		short diff = prev > curr ? prev - curr : curr - prev;
		return diff > margin;
	}
};
