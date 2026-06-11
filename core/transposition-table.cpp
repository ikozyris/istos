#include "engine-main.h"

tt_entry *TT = nullptr;
u64 zobrist_pieces[2][7][64];
u64 zobrist_turn;
u64 zobrist_castling[16];
u64 zobrist_ep[65];

static u64 rand64() {
	static u64 seed = 1070372ul;
	seed ^= seed >> 12u;
	seed ^= seed << 25u;
	seed ^= seed >> 27u;
	return seed * 2685821657736338717ul;
}

void init_zobrist() {
	for (uint c = 0; c < 2; ++c) {
		for (uint p = 0; p < 7; ++p) {
			for (uint s = 0; s < 64; ++s) {
				zobrist_pieces[c][p][s] = rand64();
			}
		}
	}
	zobrist_turn = rand64();
	for (uint i = 0; i < 16; ++i) zobrist_castling[i] = rand64();
	for (uint i = 0; i < 65; ++i) zobrist_ep[i] = rand64();
}

u64 compute_hash(const struct board &b) {
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
