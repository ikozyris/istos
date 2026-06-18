#include "../common.h"
#include "engine-main.h"

using ms = std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

struct TimeRecord {
	u64 timeMs = 0;
	uint depth = 0;
	move pv;
};

class TimeManager {
public:
	TimeManager(uint maxDepth)
		: maxDepth(maxDepth),
		  records(maxDepth + 1),
		  searchStart(),
		  remainingMs(0),
		  incrementMs(0),
		  movesToGo(40),
		  safetyReserveMs(200),
		  lastDepth(0),
		  totalUsedMs(0) {}

	// call before search begins
	void setClock(u64 timeLeft, u64 increment, uint mtg = 40) {
		remainingMs = timeLeft;
		incrementMs = increment;
		movesToGo = (mtg == 0 ? 1u : mtg);
	}

	// call when search starts
	void startSearch() {
		searchStart = Clock::now();
	}

	// call at the end of each completed depth (iterative deepening)
	void finishSearch(uint depth, move pv) {
		if (searchStart == Clock::time_point{}) {
			// no valid start — treat as zero-used
			records[depth].timeMs = 0;
		} else {
			u64 used = elapsedSinceStartMs();
			records[depth].timeMs = used;
			totalUsedMs += used;
			// deduct used time from remaining; ensure we don't underflow
			if (used >= remainingMs) remainingMs = 0;
			else remainingMs -= used;
		}
		records[depth].depth = depth;
		records[depth].pv = pv;
		lastDepth = depth;
		movesToGo--;
	}

	// Decide whether to continue to nextDepth (called before starting nextDepth)
	bool shouldContinue(uint nextDepth) {
		if (nextDepth == 0 || nextDepth > maxDepth) return false;

		// If no meaningful time left, stop (after we reached some usable depth)
		if (nextDepth >= 3 && remainingMs <= safetyReserveMs + 10) return false;

		u64 allocated = allocateTimeForMove();
		u64 projected = projectedNextDepthCost(nextDepth);

		if (projected + safetyReserveMs > allocated) return false; // don't rub out of time

		if (lastDepth >= 7 && pvStable(0.5)) return false; // stable PV on reasonable depth

		return true;
	}


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

	u64 elapsedSinceStartMs() const {
		if (searchStart == Clock::time_point{})
			return 0;
		return (u64)std::chrono::duration_cast<ms>(Clock::now() - searchStart).count();
	}

	// Allocate time for this move based on remaining time and increment.
	// Does not mutate remainingMs — caller should call finishSearch to deduct time.
	u64 allocateTimeForMove() const {
		uint mtg = (movesToGo == 0 ? 1u : movesToGo);
		u64 base = remainingMs / mtg;

		// For Fischer increments, increment is added to usable time in many engines.
		// Conservative approach: allow a portion of increment (here whole increment).
		u64 alloc = base + incrementMs;

		// Cap: not more than 25% of remaining (to preserve long-term safety),
		// but never less than a small floor (10 ms).
		u64 cap = std::max(remainingMs / 4, 10ul);
		return std::min(alloc, cap);
	}

	// Project cost for reaching nextDepth using recent iterative-deepening timings.
	u64 projectedNextDepthCost(uint nextDepth) const {
		// If we don't have useful data, return a small default based on increment
		if (lastDepth < 2)
			return std::max(10ul, incrementMs / 2);

		// Use average actual time of recorded depths 1..lastDepth but only of non-zero entries.
		u64 sum = 0;
		uint count = 0;
		for (uint d = 1; d <= lastDepth && d <= maxDepth; ++d)
			if (records[d].timeMs > 0) {
				sum += records[d].timeMs;
				++count;
			}

		if (count == 0)
			return std::max(10ul, incrementMs / 2);
		u64 avg = sum / count;

		// Estimate growth factor from last two completed depths (prefer ratio), fallback to 2x.
		double growth = 2.0;
		if (lastDepth >= 2 && records[lastDepth - 1].timeMs > 0 && records[lastDepth].timeMs > 0) {
			double r = double(records[lastDepth].timeMs) / double(records[lastDepth - 1].timeMs);
			// clamp growth into reasonable bounds
			if (r > 8.0) r = 8.0;
			if (r < 1.0) r = 1.0;
			growth = r;
		}

		// delta depth
		uint delta = (nextDepth > lastDepth) ? (nextDepth - lastDepth) : 0u;
		// apply growth^delta to avg, but cap exponent to avoid overflow
		double factor = 1.0;
		for (uint i = 0; i < delta && i < 6; ++i)
			factor *= growth;

		// compute projection with overflow protection
		double proj_d = double(avg) * factor;
		u64 proj = (proj_d > double(std::numeric_limits<u64>::max())) ? std::numeric_limits<u64>::max() : (u64)proj_d;

		// cap projection to half of remaining (don't risk more than half of remaining)
		u64 maxProj = remainingMs / 2;
		return std::min(proj, maxProj);
	}

	// Determine if PV is stable across recent depths.
	bool pvStable(double thresholdFraction) const {
		if (lastDepth < 2) return false;
		// Use the PV from the deepest completed depth as reference
		const move &ref = records[lastDepth].pv;
		if (ref.from == 0 && ref.to == 0) return false; // empty move

		uint matches = 0;
		uint denom = 0;
		// consider depths 1..lastDepth
		for (uint d = 1; d <= lastDepth; ++d) {
			if (records[d].pv.from == 0 && records[d].pv.to == 0) continue;
			++denom;
			if (records[d].pv.from == ref.from && records[d].pv.to == ref.to) ++matches;
		}
		if (denom == 0) return false;
		double frac = double(matches) / double(denom);
		return frac >= thresholdFraction;
	}
};
