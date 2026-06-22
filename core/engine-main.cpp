#include "engine-main.h"
#include "attacks.h"

u64 get_pawn_moves(uint pos, piece_color col, u64 occ, u64 enemy, u8 ep_sq)
{
	u64 moves = 0;
	uint c = pos & 7;

	if (col == WHITE) {
		if (pos >= 8) {
			uint f1 = pos - 8;
			if (!(occ & (1ul << f1))) {
				moves |= (1ul << f1);
				if (pos >= 48 && pos < 56) {
					uint f2 = pos - 16;
					if (!(occ & (1ul << f2)))
						moves |= (1ul << f2);
				}
			}
			if (c > 0) {
				uint cap = pos - 9;
				if ((enemy & (1ul << cap)) || cap == ep_sq)
					moves |= (1ul << cap);
			}
			if (c < 7) {
				uint cap = pos - 7;
				if ((enemy & (1ul << cap)) || cap == ep_sq)
					moves |= (1ul << cap);
			}
		}
	} else {
		if (pos < 56) {
			uint f1 = pos + 8;
			if (!(occ & (1ul << f1))) {
				moves |= (1ul << f1);
				if (pos >= 8 && pos < 16) {
					uint f2 = pos + 16;
					if (!(occ & (1ul << f2)))
						moves |= (1ul << f2);
				}
			}
			if (c > 0) {
				uint cap = pos + 7;
				if ((enemy & (1ul << cap)) || cap == ep_sq)
					moves |= (1ul << cap);
			}
			if (c < 7) {
				uint cap = pos + 9;
				if ((enemy & (1ul << cap)) || cap == ep_sq)
					moves |= (1ul << cap);
			}
		}
	}
	return moves;
}

u64 get_king_castling_moves(const board *b, uint pos, piece_color col, u64 occ)
{
	u64 moves = 0;
	piece_color enemy_color = (col == WHITE) ? BLACK : WHITE;

	if (col == WHITE && pos == 60) {
		if ((b->castle_white & 2) && !(occ & ((1ul << 61) | (1ul << 62)))) { // Kingside
			if (!is_square_attacked(b, 60, enemy_color, occ) &&
			    !is_square_attacked(b, 61, enemy_color, occ) &&
			    !is_square_attacked(b, 62, enemy_color, occ)) {
				moves |= (1ul << 62);
			}
		}
		if ((b->castle_white & 1) && !(occ & ((1ul << 57) | (1ul << 58) | (1ul << 59)))) { // Queenside
			if (!is_square_attacked(b, 60, enemy_color, occ) &&
			    !is_square_attacked(b, 59, enemy_color, occ) &&
			    !is_square_attacked(b, 58, enemy_color, occ)) {
				moves |= (1ul << 58);
			}
		}
	} else if (col == BLACK && pos == 4) {
		if ((b->castle_black & 2) && !(occ & ((1ul << 5) | (1ul << 6)))) { // Kingside
			if (!is_square_attacked(b, 4, enemy_color, occ) &&
			    !is_square_attacked(b, 5, enemy_color, occ) &&
			    !is_square_attacked(b, 6, enemy_color, occ)) {
				moves |= (1ul << 6);
			}
		}
		if ((b->castle_black & 1) && !(occ & ((1ul << 1) | (1ul << 2) | (1ul << 3)))) { // Queenside
			if (!is_square_attacked(b, 4, enemy_color, occ) &&
			    !is_square_attacked(b, 3, enemy_color, occ) &&
			    !is_square_attacked(b, 2, enemy_color, occ)) {
				moves |= (1ul << 2);
			}
		}
	}
	return moves;
}

// generate moves without looking for pins
u64 get_piece_pseudo_moves(const board *b, uint pos, u64 white, u64 black)
{
	piece_type type = b->piece(pos);
	if (type == EMPTY)
		return 0;

	piece_color col = b->color(pos);
	u64 friendly = (col == WHITE) ? white : black;
	u64 enemy = (col == WHITE) ? black : white;

	if (type == PAWN)
		return get_pawn_moves(pos, col, black | white, enemy, b->en_passant_sq);

	u64 attacks = get_piece_attacks(type, col, pos, black | white);

	if (type == KING)
		attacks |= get_king_castling_moves(b, pos, col, black | white);

	return attacks & ~friendly;
}

// board is passed by value to allow simulating move
bool is_valid_move(const struct move m, const struct board &b, piece_color turn)
{
	if (m.from >= 64 || m.to >= 64)
		return false;

	piece_type type = b.piece(m.from);
	if (type == EMPTY)
		return false;

	piece_color col = b.color(m.from);
	if (col != turn)
		return false;

	// TODO: one global table with move undo
	// simulate move inline without hashing
	struct board next_b; // copy of table
	next_b.colors[WHITE] = b.colors[WHITE];
	next_b.colors[BLACK] = b.colors[BLACK];
	for (uint i = 0; i < 7; i++)
		next_b.pieces[i] = b.pieces[i];

	u64 from_mask = 1ull << m.from;
	u64 to_mask = 1ull << m.to;

	if (type == PAWN && m.to == b.en_passant_sq) {
		uint ep_capture_sq = (col == WHITE) ? m.to + 8 : m.to - 8;
		u64 ep_mask = 1ull << ep_capture_sq;
		piece_color enemy_col = (col == WHITE) ? BLACK : WHITE;
		next_b.pieces[PAWN] &= ~ep_mask;
		next_b.colors[enemy_col] &= ~ep_mask;
	}

	piece_type cap_type = b.piece(m.to);
	if (cap_type != EMPTY) {
		piece_color enemy_col = (col == WHITE) ? BLACK : WHITE;
		next_b.pieces[cap_type] &= ~to_mask;
		next_b.colors[enemy_col] &= ~to_mask;
	}

	next_b.pieces[type] &= ~from_mask;
	next_b.pieces[type] |= to_mask;
	next_b.colors[col] &= ~from_mask;
	next_b.colors[col] |= to_mask;

	return !is_in_check(&next_b, col, next_b.colors[WHITE] | next_b.colors[BLACK]);
}
