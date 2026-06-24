#include "evaluate-tables.h"
#include "engine-main.h"
#include "attacks.h"

uint evaluate_pawn_structure(const board &b, piece_color col)
{
	uint penalty = 0;
	u64 my_pawns = b.pieces[PAWN] & b.colors[col];

	u8 pawn_files = 0;
	for (uint file = 0; file < 8; ++file) {
		u64 pawns_on_file = my_pawns & FILE_MASKS[file];
		uint pawn_cnt = std::__popcount(pawns_on_file);

		if (pawn_cnt > 1) { // penalty for doubled pawns
			penalty += (pawn_cnt - 1) * DOUBLED_PAWN_PENALTY;
			pawn_files |= 1u << file;
		}
	}

	// count pawn islands
	uint island_cnt = std::__popcount((unsigned)(pawn_files & ~(pawn_files << 1u) & ~(pawn_files >> 1u)));
	penalty += island_cnt * PAWN_ISLANDS_PENALTY;

	uint pawn_cnt = std::__popcount(my_pawns);
	// knights are valuable with many pawns, rooks with little pawns
	uint knight_cnt = std::__popcount(b.pieces[KNIGHT] & b.colors[col]);
	uint rook_cnt = std::__popcount(b.pieces[ROOK] & b.colors[col]);
	penalty += knight_cnt * KNIGHT_ADJ[pawn_cnt] + rook_cnt * ROOK_ADJ[pawn_cnt];

	return penalty; 
}

uint evaluate_mobility(const board &b, piece_color color, u64 occ) {
	uint mobility_bonus = 0;
	u64 friendly = b.colors[color];
	u64 targets = ~friendly; // empty squares or enemy pieces

	u64 knights = b.pieces[KNIGHT] & friendly;
	while (knights) {
		uint pos = __builtin_ctzll(knights);
		u64 attacks = get_knight_attacks(pos) & targets;
		mobility_bonus += __builtin_popcountll(attacks) * KNIGHT_MOBILITY_FACTOR;
		knights &= (knights - 1);
	}

	u64 bishops = b.pieces[BISHOP] & friendly;
	while (bishops) {
		uint pos = __builtin_ctzll(bishops);
		u64 attacks = get_bishop_attacks(pos, occ) & targets;
		mobility_bonus += __builtin_popcountll(attacks) * BISHOP_MOBILITY_FACTOR;
		bishops &= (bishops - 1);
	}

	u64 rooks = b.pieces[ROOK] & friendly;
	while (rooks) {
		uint pos = __builtin_ctzll(rooks);
		u64 attacks = get_rook_attacks(pos, occ) & targets;
		mobility_bonus += __builtin_popcountll(attacks) * ROOK_MOBILITY_FACTOR;
		rooks &= (rooks - 1);
	}

	return mobility_bonus;
}

short evaluate_board(const board &b)
{
	short score = pst_score(&b);
	u64 occ = b.colors[WHITE] | b.colors[BLACK];

	score += evaluate_mobility(b, WHITE, occ);
	score -= evaluate_mobility(b, BLACK, occ);

	score -= evaluate_pawn_structure(b, WHITE);
	score += evaluate_pawn_structure(b, BLACK);

	return score;
}
