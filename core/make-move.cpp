#include "engine-main.h"

inline int iabs(int a) { return a >= 0 ? a : -a; }
u64 nodes;
void make_move(struct board *b, const struct move m)
{
	piece_type type = b->piece(m.from);
	piece_color col = b->color(m.from);

	b->hash_key ^= zobrist_ep[b->en_passant_sq];
	b->hash_key ^= zobrist_castling[b->castle_white | (b->castle_black << 2)];

	// en passant evaluation
	if (type == PAWN && iabs((int)m.from - (int)m.to) == 16) {
		b->en_passant_sq = (m.from + m.to) / 2;
	} else if (type == PAWN && m.to == b->en_passant_sq) {
		b->set_piece(EMPTY, WHITE, col == WHITE ? m.to + 8 : m.to - 8);
		b->en_passant_sq = NO_EP;
	} else
		b->en_passant_sq = NO_EP;

	// castling execution (moving the Rook)
	if (type == KING && iabs((int)m.from - (int)m.to) == 2) {
		if (m.to == 62) { // White Kingside
			b->set_piece(ROOK, WHITE, 61);
			b->set_piece(EMPTY, WHITE, 63);
		} else if (m.to == 58) { // White Queenside
			b->set_piece(ROOK, WHITE, 59);
			b->set_piece(EMPTY, WHITE, 56);
		} else if (m.to == 6) { // Black Kingside
			b->set_piece(ROOK, BLACK, 5);
			b->set_piece(EMPTY, WHITE, 7);
		} else if (m.to == 2) { // Black Queenside
			b->set_piece(ROOK, BLACK, 3);
			b->set_piece(EMPTY, WHITE, 0);
		}
	}

	// revoke castling rights
	if (type == KING) {
		if (col == WHITE)
			b->castle_white = 0;
		else
			b->castle_black = 0;
	}

	// rooks moved or got captured
	if (m.from == 63 || m.to == 63)
		b->castle_white &= ~2; // kingside bit off
	if (m.from == 56 || m.to == 56)
		b->castle_white &= ~1; // queenside bit off
	if (m.from == 7 || m.to == 7)
		b->castle_black &= ~2;
	if (m.from == 0 || m.to == 0)
		b->castle_black &= ~1;

	if (type == PAWN && (m.to >= 56 || m.to < 8)) // TODO: underpromotion
		type = QUEEN;

	// execute piece move
	b->set_piece(type, col, m.to);
	b->set_piece(EMPTY, WHITE, m.from);

	b->hash_key ^= zobrist_ep[b->en_passant_sq];
	b->hash_key ^= zobrist_castling[b->castle_white | (b->castle_black << 2)];
	b->hash_key ^= zobrist_turn;

	nodes++;
}
