#include "evaluate-tables.h"
#include "engine-main.h"
#include "attacks.h"

uint evaluate_pawn_structure(const board &b, u64 my_pawns)
{
	uint penalty = 0;
	u8 pawn_files = 0;
	for (uint file = 0; file < 8; ++file) {
		u64 pawns_on_file = my_pawns & FILE_MASKS[file];
		uint pawn_cnt = std::__popcount(pawns_on_file);

		if (pawn_cnt > 0)
			pawn_files |= 1u << file;

		if (pawn_cnt > 1) // penalty for doubled pawns
			penalty += (pawn_cnt - 1) * DOUBLED_PAWN_PENALTY;
	}

	// count pawn islands
	uint island_cnt = std::__popcount((unsigned)(pawn_files & ~(pawn_files << 1u) & ~(pawn_files >> 1u)));
	penalty += island_cnt * PAWN_ISLANDS_PENALTY;

	return penalty;
}

uint evaluate_mobility(const board &b, piece_color color, u64 occ, u64 enemy_king_ring) {
	uint mobility_bonus = 0;
	u64 friendly = b.colors[color];
	u64 targets = ~friendly; // empty squares or enemy pieces
	uint king_attackers = 0;

	u64 knights = b.pieces[KNIGHT] & friendly;
	while (knights) {
		uint pos = __builtin_ctzll(knights);
		u64 attacks = get_knight_attacks(pos) & targets;
		if (attacks & enemy_king_ring) // bonus for attacking enemy king
			king_attackers += 1;
		mobility_bonus += __builtin_popcountll(attacks) * KNIGHT_MOBILITY_FACTOR;
		knights &= (knights - 1);
	}

	u64 bishops = b.pieces[BISHOP] & friendly;
	while (bishops) {
		uint pos = __builtin_ctzll(bishops);
		u64 attacks = get_bishop_attacks(pos, occ) & targets;
		if (attacks & enemy_king_ring)
			king_attackers += 1;
		mobility_bonus += __builtin_popcountll(attacks) * BISHOP_MOBILITY_FACTOR;
		bishops &= (bishops - 1);
	}

	u64 rooks = b.pieces[ROOK] & friendly;
	while (rooks) {
		uint pos = __builtin_ctzll(rooks);
		u64 attacks = get_rook_attacks(pos, occ) & targets;
		if (attacks & enemy_king_ring)
			king_attackers += 2;
		mobility_bonus += __builtin_popcountll(attacks) * ROOK_MOBILITY_FACTOR;
		rooks &= (rooks - 1);
	}

	return mobility_bonus + king_attackers * KING_VULNR_PENALTY;
}

// attacking near king is handled by mobility
uint king_safety(const board &b, piece_color color)
{
	uint penalty = 0;

	uint king = __builtin_ctzll(b.pieces[KING] & b.colors[color]);
	uint king_file = king % 8;
	uint king_rank = king / 8;

	for (uint i = 0; i < 3; ++i) {
		uint file = king_file - 1 + i;
		if (file > 7) // catches both underflow and overflow
			continue;
		u64 enemies = b.colors[1 - color] & FILE_MASKS[file];

		uint enemy_pawns_cnt = std::__popcount(enemies & b.pieces[PAWN]);
		if (!enemy_pawns_cnt) // no pawns
			penalty += KING_ON_FILE_PENALTY; // penalty for semi-open files near king

		u64 enemy_pieces = enemies & ~b.pieces[PAWN]; // and large pieces near king

		enemy_pieces &= KING_SAFETY_RANK_MASK[king_rank];

		while (enemy_pieces) { // proximity and material based penalty
			uint sq = __builtin_ctzll(enemy_pieces);
			uint enemy_rank = sq / 8;
		
			uint rank_dist = std::abs((int)king_rank - (int)enemy_rank);
			piece_type pt = b.piece(sq);
		
			penalty += KING_PROXIMITY_PENALTY[pt][rank_dist];
		
			enemy_pieces &= (enemy_pieces - 1);
		}
	}

	return penalty;
}

short material_adj(const board &b, u64 my_pawns, piece_color color)
{
	uint pawn_cnt = std::__popcount(my_pawns);
	// knights are valuable with many pawns, rooks with little pawns
	int knight_cnt = std::__popcount(b.pieces[KNIGHT] & b.colors[color]);
	int rook_cnt = std::__popcount(b.pieces[ROOK] & b.colors[color]);

	return knight_cnt * KNIGHT_ADJ[pawn_cnt] + rook_cnt * ROOK_ADJ[pawn_cnt];
}

short evaluate_board(const board &b)
{
	short score = pst_score(&b);
	u64 occ = b.colors[WHITE] | b.colors[BLACK];

	u64 white_king_ring = get_king_attacks(__builtin_ctzll(b.pieces[KING] & b.colors[WHITE]));
	u64 black_king_ring = get_king_attacks(__builtin_ctzll(b.pieces[KING] & b.colors[BLACK]));

	score += evaluate_mobility(b, WHITE, occ, black_king_ring); // bonus
	score -= evaluate_mobility(b, BLACK, occ, white_king_ring);

	u64 white_pawns = b.pieces[PAWN] & b.colors[WHITE];
	score += material_adj(b, white_pawns, WHITE); // bonus or penalty
	score -= evaluate_pawn_structure(b, white_pawns); // penalty

	u64 black_pawns = b.pieces[PAWN] & b.colors[BLACK];
	score -= material_adj(b, black_pawns, BLACK);
	score += evaluate_pawn_structure(b, black_pawns);

	const uint safety_threshold = (PIECE_WEIGHTS_MG[KING] + PIECE_WEIGHTS_MG[QUEEN] + PIECE_WEIGHTS_MG[KNIGHT]) / 64;
	// king safety
	if (b.phase > safety_threshold) { // queens still on board
		score -= king_safety(b, WHITE);
		score += king_safety(b, BLACK);
	}



	return score;
}
