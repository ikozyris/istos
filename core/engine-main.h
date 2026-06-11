#pragma once
#include "../common.h"
#include "evaluate-tables.h"

enum piece_type : u8 { EMPTY = 0, KING, PAWN, KNIGHT, BISHOP, ROOK, QUEEN };
enum piece_color : u8 { WHITE = 0, BLACK };

const u8 TYPE_MASK = (1u << 3u) - 1;
const u8 PIECE_MASK = (1u << 4u) - 1;
const u8 NO_EP = 64; // no en passant available

struct move {
	u8 from;
	u8 to;
};

extern u64 zobrist_pieces[2][7][64]; // color, type, position
extern u64 zobrist_turn;
extern u64 zobrist_castling[16];
extern u64 zobrist_ep[65];

extern void init_zobrist();
extern u64 compute_hash(const struct board &b);

enum tt_flag : u8 {EXACT = 0, LOWERBOUND, UPPERBOUND};

struct tt_entry {
	u64 key;
	int score;
	struct move best_move;
	u8 depth;
	u8 flag;
};
const uint TT_SIZE = 2097152;
extern tt_entry *TT;

struct board {
	u64 hash_key = 0;
	u64 colors[2]; // colors[WHITE] = bitboard of WHITE pieces
	u64 pieces[7]; // pieces[PAWN] = bitboard of PAWNs

	piece_type sq_board[64];

	int pst_score;
	u8 en_passant_sq = NO_EP; // 0-63 for the target square, 64 for none
	// 1st bit (1) queen side, 2nd bit (2) kingside
	u8 castle_white : 2;
	u8 castle_black : 2;

	inline piece_type piece(uint pos) const {
		return sq_board[pos];
	}

	inline piece_color color(uint pos) const {
		u64 mask = 1ull << pos;
		return (colors[WHITE] & mask) ? WHITE : BLACK;
	}

	void set_piece(piece_type type, piece_color col, uint pos) {
		u64 mask = 1ull << pos;
		piece_type old_type = sq_board[pos];

		if (old_type != EMPTY) { // remove existing piece
			piece_color old_col = (colors[WHITE] & mask) ? WHITE : BLACK;
			hash_key ^= zobrist_pieces[old_col][old_type][pos];

			pieces[old_type] &= ~mask;
			colors[old_col]  &= ~mask;

			// lazy evaluation
			if (old_col == WHITE)
				pst_score -= PIECE_WEIGHTS[old_type] + PST_BONUS[old_type][pos];
			else
				pst_score += PIECE_WEIGHTS[old_type] + PST_BONUS[old_type][pos ^ 56];
		}

		if (type != EMPTY) { // add new piece
			hash_key ^= zobrist_pieces[col][type][pos];

			pieces[type] |= mask;
			colors[col]  |= mask;

			if (col == WHITE)
				pst_score += PIECE_WEIGHTS[type] + PST_BONUS[type][pos];
			else
				pst_score -= PIECE_WEIGHTS[type] + PST_BONUS[type][pos ^ 56];
		}

		sq_board[pos] = type;
	}
};

// Helper to calculate occupancies from the struct
inline void get_occupancy(u64 *white, u64 *black, const board *b)
{
	*white = b->colors[WHITE];
	*black = b->colors[BLACK];
}

extern bool is_valid_move(struct move m, struct board b, piece_color turn);
u64 get_piece_pseudo_moves(const board *b, uint pos, u64 white, u64 black);
extern void make_move(struct board *b, struct move m);

inline int min(int a, int b) { return a < b ? a : b; }
inline int max(int a, int b) { return a > b ? a : b; }
extern struct move get_best_move(struct board b, uint target_depth, piece_color turn);

extern uint nodes;
