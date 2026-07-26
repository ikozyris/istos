#pragma once
#include "engine-main.h"
#include "evaluate-tables.h"

struct pawn_moves {
	u64 push1;
	u64 push2;
	u64 capL;
	u64 capR;
};
// bulk pawn move generator
inline struct pawn_moves get_pawns_moves(const struct board *b, piece_color turn, u64 my_pieces, u64 enemy_pieces) {
	u64 occ = my_pieces | enemy_pieces;
	u64 pawns = b->pieces[PAWN] & my_pieces;

	u64 w_pawns = turn == WHITE ? pawns : __builtin_bswap64(pawns);
	u64 w_occ = turn == WHITE ? occ : __builtin_bswap64(occ);
	u64 w_enemy = turn == WHITE ? enemy_pieces : __builtin_bswap64(enemy_pieces);

	u64 push1 = (w_pawns >> 8) & ~w_occ;
	u64 push2 = ((push1 & 0x0000FF0000000000ull) >> 8) & ~w_occ;
	u64 capL = ((w_pawns & ~FILE_MASKS[0]) >> 9) & w_enemy;
	u64 capR = ((w_pawns & ~FILE_MASKS[7]) >> 7) & w_enemy;

	if (b->en_passant_sq != NO_EP) {
		u64 ep = 1ull << b->en_passant_sq;
		u64 w_ep = turn == WHITE ? ep : __builtin_bswap64(ep);
		capL |= ((w_pawns & ~FILE_MASKS[0]) >> 9) & w_ep;
		capR |= ((w_pawns & ~FILE_MASKS[7]) >> 7) & w_ep;
	}

	if (turn == BLACK) {
		push1 = __builtin_bswap64(push1);
		push2 = __builtin_bswap64(push2);
		capL = __builtin_bswap64(capL);
		capR = __builtin_bswap64(capR);
	}

	struct pawn_moves res = {push1, push2, capL, capR};
	return res;
}

struct pext_bbt {
	u64 mask;     // mask of pieces on board
	u64 *attacks; // offset into large attack lookup table
};

extern struct pext_bbt rook_pext[64];
extern struct pext_bbt bishop_pext[64];

extern u64 knight_attacks[64];
extern u64 king_attacks[64];
extern u64 pawn_attacks[2][64];

inline u64 get_rook_attacks(uint pos, u64 occ) {
	return rook_pext[pos].attacks[_pext_u64(occ, rook_pext[pos].mask)];
}
inline u64 get_bishop_attacks(uint pos, u64 occ) {
	return bishop_pext[pos].attacks[_pext_u64(occ, bishop_pext[pos].mask)];
}
inline u64 get_king_attacks(uint pos) {
	return king_attacks[pos];
}
inline u64 get_knight_attacks(uint pos) {
	return knight_attacks[pos];
}
inline u64 get_pawn_attacks(uint pos, piece_color color) {
	return pawn_attacks[color][pos];
}
void init_attacks();

// by_color is attacker
inline bool is_square_attacked(const board *b, uint pos, piece_color by_color, u64 occ)
{
	u64 enemies = b->colors[by_color];

	u64 enemy_knights = b->pieces[KNIGHT] & enemies;
	if (enemy_knights && (get_knight_attacks(pos) & enemy_knights))
		return true;

	u64 enemy_kings = b->pieces[KING] & enemies;
	if (enemy_kings && (get_king_attacks(pos) & enemy_kings))
		return true;

	piece_color defending_color = (by_color == WHITE) ? BLACK : WHITE;
	u64 enemy_pawns = b->pieces[PAWN] & enemies;
	if (enemy_pawns && (get_pawn_attacks(pos, defending_color) & enemy_pawns))
		return true;

	u64 orthogonal_sliders = (b->pieces[ROOK] | b->pieces[QUEEN]) & enemies;
	if (orthogonal_sliders && (get_rook_attacks(pos, occ) & orthogonal_sliders))
		return true;

	u64 diagonal_sliders = (b->pieces[BISHOP] | b->pieces[QUEEN]) & enemies;
	if (diagonal_sliders && (get_bishop_attacks(pos, occ) & diagonal_sliders))
		return true;

	return false;
}

// color_in_check is defender
inline bool is_in_check(const board *b, piece_color color_in_check, u64 occ)
{
	uint king_pos = std::__countr_zero(b->pieces[KING] & b->colors[color_in_check]);
	if (king_pos >= 64)
		return false;

	piece_color enemy_color = (color_in_check == WHITE) ? BLACK : WHITE;
	return is_square_attacked(b, king_pos, enemy_color, occ);
}
