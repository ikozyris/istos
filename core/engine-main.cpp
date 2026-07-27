#include "engine-main.h"
#include "attacks.h"

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

bool is_valid_move(const struct move m, const struct board &b, piece_color turn)
{
	if (m.from >= 64 || m.to >= 64) [[unlikely]]
		return false;

	piece_type type = b.piece(m.from);
	if (type == EMPTY) [[unlikely]]
		return false;

	piece_color color = b.color(m.from);
	if (color != turn) [[unlikely]]
		return false;

	// TODO: one global table with move undo
	// simulate move inline without hashing
	struct board next_b; // copy of table
	memcpy(&next_b, &b, sizeof(b.colors) + sizeof(b.pieces));

	u64 from_mask = 1ull << m.from;
	u64 to_mask = 1ull << m.to;

	if (type == PAWN && m.to == b.en_passant_sq) {
		uint ep_capture_sq = (color == WHITE) ? m.to + 8 : m.to - 8;
		u64 ep_mask = 1ull << ep_capture_sq;
		piece_color enemy_col = (piece_color)(1 - color);
		next_b.pieces[PAWN] &= ~ep_mask;
		next_b.colors[enemy_col] &= ~ep_mask;
	}

	piece_type cap_type = b.piece(m.to);
	if (cap_type != EMPTY) {
		piece_color enemy_col = (piece_color)(1 - color);
		next_b.pieces[cap_type] &= ~to_mask;
		next_b.colors[enemy_col] &= ~to_mask;
	}

	next_b.pieces[type] &= ~from_mask;
	next_b.pieces[type] |= to_mask;
	next_b.colors[color] &= ~from_mask;
	next_b.colors[color] |= to_mask;

	return !is_in_check(&next_b, color, next_b.colors[WHITE] | next_b.colors[BLACK]);
}
