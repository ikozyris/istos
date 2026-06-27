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

enum tt_flag : u8 {
	TT_NONE,
	TT_EXACT,
	TT_LOWER,
	TT_UPPER
};

struct tt_entry {
	u64 key = 0;
	short score = 0;
	struct move best_move = {64, 64};
	u8 depth = 0;
	tt_flag flag = TT_NONE;
};
const uint TT_BUCKETS = 2097152;
const uint TT_BUCKET_SIZE = 4;
extern tt_entry *TT;
const short TT_MAGIC_FAIL_EVAL = 32000;

extern void init_zobrist();
extern u64 compute_hash(const struct board &b);
extern void tt_store(u64 key, move best_move, short score, u8 depth, tt_flag flag);
extern short tt_probe(u64 key, uint depth, short alpha, short beta, move &outMove);


struct board {
	u64 hash_key = 0;
	u64 colors[2]; // colors[WHITE] = bitboard of WHITE pieces
	u64 pieces[7]; // pieces[PAWN] = bitboard of PAWNs

	piece_type sq_board[64];

	short pst_mg_score;
	short pst_eg_score;
	u8 phase; // sum of piece values
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

			phase -= PIECE_WEIGHTS[old_type] / 128;
			if (old_col == WHITE) { // lazy evaluation
				pst_mg_score -= PIECE_WEIGHTS[old_type] + pst_mg[old_type][pos];
				pst_eg_score -= PIECE_WEIGHTS[old_type] + pst_eg[old_type][pos];
			} else {
				pst_mg_score += PIECE_WEIGHTS[old_type] + pst_mg[old_type][pos ^ 56];
				pst_eg_score += PIECE_WEIGHTS[old_type] + pst_eg[old_type][pos ^ 56];
			}
		}

		if (type != EMPTY) { // add new piece
			hash_key ^= zobrist_pieces[col][type][pos];

			pieces[type] |= mask;
			colors[col]  |= mask;


			// phase could overflow from promotions but in that case nobody cares since the
			// material gain outways anything
			phase += PIECE_WEIGHTS[type] / 128;
			if (col == WHITE) {
				pst_mg_score += PIECE_WEIGHTS[type] + pst_mg[type][pos];
				pst_eg_score += PIECE_WEIGHTS[type] + pst_eg[type][pos];
			} else {
				pst_mg_score -= PIECE_WEIGHTS[type] + pst_mg[type][pos ^ 56];
				pst_eg_score -= PIECE_WEIGHTS[type] + pst_eg[type][pos ^ 56];
			}
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

extern bool is_valid_move(const struct move m, const struct board &b, piece_color turn);
extern u64 get_piece_pseudo_moves(const board *b, uint pos, u64 white, u64 black);
extern void make_move(struct board *b, const struct move m);

// for UCI interface
extern void search_init(struct board b, piece_color turn, struct move root_moves[224], short scores[224], struct move killer_moves[32][2],
			short history[2][7][64], int &move_count, struct move &best_move_all);

extern void search_loop(uint cur_depth, piece_color turn, struct move root_moves[224], struct move killer_moves[32][2], short history[2][7][64],
			short scores[224], uint move_count, const struct board &b, short &eval, struct move &best_move_cur);
// for local tests
extern struct move get_best_move(struct board b, uint target_depth, piece_color turn);

inline short pst_score(const struct board *b) {
	uint phase = b->phase - 210; // kings alone sum 210
	return (b->pst_mg_score * phase + b->pst_eg_score * (46 - phase)) / 64;
}

extern u64 nodes;

extern void init_time_management(u64 total, u64 incr);
