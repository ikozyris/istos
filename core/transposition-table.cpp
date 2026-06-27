#include "engine-main.h"

tt_entry *TT = nullptr;
u64 zobrist_pieces[2][7][64];
u64 zobrist_turn;
u64 zobrist_castling[16];
u64 zobrist_ep[65];

static u64 rand64()
{
	static u64 seed = 1070372ul;
	seed ^= seed >> 12u;
	seed ^= seed << 25u;
	seed ^= seed >> 27u;
	return seed * 2685821657736338717ul;
}

void init_zobrist()
{
	for (uint c = 0; c < 2; ++c)
		for (uint p = 0; p < 7; ++p)
			for (uint s = 0; s < 64; ++s)
				zobrist_pieces[c][p][s] = rand64();

	zobrist_turn = rand64();
	for (uint i = 0; i < 16; ++i)
		zobrist_castling[i] = rand64();
	for (uint i = 0; i < 65; ++i)
		zobrist_ep[i] = rand64();
}

u64 compute_hash(const struct board &b)
{
	u64 h = 0;
	for (uint i = 0; i < 64; ++i) {
		piece_type p = b.piece(i);
		if (p != EMPTY) {
			piece_color c = b.color(i);
			h ^= zobrist_pieces[c][p][i];
		}
	}
	h ^= zobrist_castling[b.castle_white | (b.castle_black << 2)];
	h ^= zobrist_ep[b.en_passant_sq];
	h ^= zobrist_turn;
	return h;
}

inline tt_entry *tt_bucket(u64 key) {
	return &TT[key % TT_BUCKETS];
}

void tt_store(u64 key, move best_move, short score, u8 depth, tt_flag flag) {
	tt_entry *bucket = tt_bucket(key);

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		if (bucket[i].key == key) {
			// save for move ordering
			if (flag != TT_UPPER || depth >= bucket[i].depth || bucket[i].flag == TT_NONE)
				bucket[i].best_move = best_move;
			
			// overwrite if new data is deeper
			if (flag == TT_EXACT || depth >= bucket[i].depth) {
				bucket[i].score = score;
				bucket[i].depth = depth;
				bucket[i].flag = flag;
			}
			return;
		}
	}

	// key not found - remove key with least depth
	uint replace_idx = 0;
	int min_depth = 9999;

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		if (bucket[i].flag == TT_NONE) {
			replace_idx = i;
			break; // found empty slot
		}
		if (bucket[i].depth < min_depth) {
			min_depth = bucket[i].depth;
			replace_idx = i; // track shallowest for eviction
		}
	}

	// write new entry data to chosen slot
	bucket[replace_idx].key = key;
	bucket[replace_idx].best_move = best_move;
	bucket[replace_idx].score = score;
	bucket[replace_idx].depth = depth;
	bucket[replace_idx].flag = flag;
}

short tt_probe(u64 key, uint depth, short alpha, short beta, move &outMove) {
	tt_entry *bucket = tt_bucket(key);

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		if (bucket[i].key == key) {
			if (bucket[i].flag == TT_NONE)
				return TT_MAGIC_FAIL_EVAL;

			outMove = bucket[i].best_move;

			if (bucket[i].depth < depth) // insufficient depth
				return TT_MAGIC_FAIL_EVAL;

			if (bucket[i].flag == TT_EXACT)
				return bucket[i].score;
			if (bucket[i].flag == TT_UPPER && bucket[i].score <= alpha)
				return bucket[i].score;
			if (bucket[i].flag == TT_LOWER && bucket[i].score >= beta)
				return bucket[i].score;

			return TT_MAGIC_FAIL_EVAL;
		}
	}
	return TT_MAGIC_FAIL_EVAL;
}
