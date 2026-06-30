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

// use intermediate vector registers for guaranteed atomicity
void atomic_write_16(tt_entry* dest, const tt_entry& src) {
	__m128i val = _mm_load_si128((const __m128i*)&src);
	_mm_store_si128((__m128i*)dest, val);
}

tt_entry atomic_read_16(const tt_entry* src) {
	__m128i val = _mm_load_si128((const __m128i*)src);
	
	tt_entry result;
	_mm_store_si128((__m128i*)&result, val);

	return result;
}

inline tt_entry *tt_bucket(u64 key) {
	return &TT[key % TT_BUCKETS];
}

void tt_store(u64 key, move best_move, short score, u8 depth, tt_flag flag) {
	tt_entry *bucket = tt_bucket(key);
	tt_entry tmp_entry;

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		tmp_entry = atomic_read_16(&bucket[i]);
		if (tmp_entry.key == key) {
			// save for move ordering
			if (flag != TT_UPPER || depth >= tmp_entry.depth)
				tmp_entry.best_move = best_move;
			
			// overwrite if new data is deeper
			if (flag == TT_EXACT || depth >= tmp_entry.depth) {
				tmp_entry.score = score;
				tmp_entry.depth = depth;
				tmp_entry.flag = flag;
			}
			atomic_write_16(&bucket[i], tmp_entry);
			return;
		}
	}

	// key not found - remove key with least depth
	uint replace_idx = 0;
	int min_depth = 9999;

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		tmp_entry = atomic_read_16(&bucket[i]);
		if (tmp_entry.flag == TT_NONE) {
			replace_idx = i;
			break; // found empty slot
		}
		if (tmp_entry.depth < min_depth) {
			min_depth = tmp_entry.depth;
			replace_idx = i; // track shallowest for eviction
		}
	}

	// write new entry data to chosen slot
	tmp_entry.key = key;
	tmp_entry.best_move = best_move;
	tmp_entry.score = score;
	tmp_entry.depth = depth;
	tmp_entry.flag = flag;
	atomic_write_16(&bucket[replace_idx], tmp_entry);
}

short tt_probe(u64 key, uint depth, short alpha, short beta, move &outMove) {
	tt_entry *bucket = tt_bucket(key);
	tt_entry tmp_entry;

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		tmp_entry = atomic_read_16(&bucket[i]);
		if (tmp_entry.key == key) {
			if (tmp_entry.flag == TT_NONE)
				return TT_MAGIC_FAIL_EVAL;

			outMove = tmp_entry.best_move;

			if (tmp_entry.depth < depth) // insufficient depth
				return TT_MAGIC_FAIL_EVAL;

			if (tmp_entry.flag == TT_EXACT)
				return tmp_entry.score;
			if (tmp_entry.flag == TT_UPPER && tmp_entry.score <= alpha)
				return tmp_entry.score;
			if (tmp_entry.flag == TT_LOWER && tmp_entry.score >= beta)
				return tmp_entry.score;

			return TT_MAGIC_FAIL_EVAL;
		}
	}
	return TT_MAGIC_FAIL_EVAL;
}

// pass by value to play moves
uint extract_pv(struct board b, uint max_ply, struct move *pv)
{
	uint i;
	for (i = 0; i < max_ply; ++i) {
		short res = tt_probe(b.hash_key, 0, 32000, -32000, pv[i]);
		if (res == TT_MAGIC_FAIL_EVAL)
			break;
		make_move(&b, pv[i]);
	}
	return i;
}
