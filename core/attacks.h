#pragma once
#include "engine-main.h"

struct pext_bbt {
	u64 mask;     // mask of pieces on board
	u64 *attacks; // offset into large attack lookup table
};

extern struct pext_bbt rook_pext[64];
extern struct pext_bbt bishop_pext[64];

extern u64 knight_attacks[64];
extern u64 king_attacks[64];
extern u64 pawn_attacks[2][64];

u64 get_piece_attacks(enum piece_type piece, piece_color col, uint pos, u64 occ);
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

// Returns true if 'pos' is attacked by the given color
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

// Returns true if the king of the given color is attacked by any enemy piece.
inline bool is_in_check(const board *b, piece_color color_in_check, u64 occ)
{
	uint king_pos = std::__countr_zero(b->pieces[KING] & b->colors[color_in_check]);
	if (king_pos >= 64)
		return false;

	piece_color enemy_color = (color_in_check == WHITE) ? BLACK : WHITE;
	return is_square_attacked(b, king_pos, enemy_color, occ);
}
