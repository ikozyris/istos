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
	return h;
}

// separate functions help isolating read/writes
inline void atomic_write_8(tt_entry* dest, const tt_entry& src) { *dest = src;  }
inline tt_entry atomic_read_8(const tt_entry* src) { return *src; }
inline tt_entry *tt_bucket(u64 key) { return &TT[key % TT_BUCKETS]; }

void tt_store(u64 orig_key, move best_move, short score, u8 depth, tt_flag flag) {
	tt_entry *bucket = tt_bucket(orig_key); // query lower bits key
	tt_entry tmp_entry;

	u32 key = (orig_key >> 33) & TT_KEY_MASK; // use higher bits for comparison to use the whole key

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		tmp_entry = atomic_read_8(&bucket[i]);

		if (tmp_entry.get_key() == key) {
			u8  old_depth = tmp_entry.get_depth();
			move next_move = tmp_entry.get_move();
			u8  next_depth = old_depth;
			short next_score = tmp_entry.get_score();
			tt_flag next_flag = tmp_entry.get_flag();

			// save for move ordering
			if (flag != TT_UPPER || depth >= old_depth)
				next_move = best_move;

			// overwrite if new data is deeper
			if (flag == TT_EXACT || depth >= old_depth) {
				next_score = score;
				next_depth = depth;
				next_flag = flag;
			}

			tmp_entry.set(key, next_move, next_depth, next_flag, next_score);
			atomic_write_8(&bucket[i], tmp_entry);
			return;
		}
	}

	// key not found - remove key with least depth
	uint replace_idx = 0;
	int min_depth = 9999;

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		tmp_entry = atomic_read_8(&bucket[i]);

		if (tmp_entry.get_flag() == TT_NONE) {
			replace_idx = i;
			break; // found empty slot
		}
		if (tmp_entry.get_depth() < min_depth) {
			min_depth = tmp_entry.get_depth();
			replace_idx = i; // track shallowest for eviction
		}
	}

	// write new entry data to chosen slot
	tmp_entry.set(key, best_move, depth, flag, score);
	atomic_write_8(&bucket[replace_idx], tmp_entry);
}

short tt_probe(u64 orig_key, uint depth, short alpha, short beta, move &out_move) {
	tt_entry *bucket = tt_bucket(orig_key); // query low bits
	tt_entry tmp_entry;

	u32 key = (orig_key >> 33) & TT_KEY_MASK; // compare high bits

	for (uint i = 0; i < TT_BUCKET_SIZE; ++i) {
		tmp_entry = atomic_read_8(&bucket[i]);

		if (tmp_entry.get_key() == key) {
			tt_flag entry_flag = tmp_entry.get_flag();
			if (entry_flag == TT_NONE)
				return TT_MAGIC_FAIL_EVAL;

			out_move = tmp_entry.get_move();

			if (tmp_entry.get_depth() < depth) // insufficient depth
				return TT_MAGIC_FAIL_EVAL;

			short entry_score = tmp_entry.get_score();

			if (entry_flag == TT_EXACT)
				return entry_score;
			if (entry_flag == TT_UPPER && entry_score <= alpha)
				return entry_score;
			if (entry_flag == TT_LOWER && entry_score >= beta)
				return entry_score;

			return TT_MAGIC_FAIL_EVAL;
        	}
	}
	return TT_MAGIC_FAIL_EVAL;
}

// warning: make local copy before calling
uint extract_pv(struct board &b, uint max_ply, struct move *pv)
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
