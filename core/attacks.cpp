#include "engine-main.h"

// the first blocker is included in occupancy table

#include <immintrin.h> // Required for _pext_u64
#include <assert.h>

// The exact total number of permutations needed for all 64 squares.
const uint ROOK_TABLE_SIZE = 102400;
const uint BISHOP_TABLE_SIZE = 5248;

// PEXT masks and offsets
u64 rook_masks[64];
u64 bishop_masks[64];
int rook_offs[64];
int bishop_offs[64];

// attack lookup tables
u64 rook_attacks[ROOK_TABLE_SIZE];
u64 bishop_attacks[BISHOP_TABLE_SIZE];
u64 knight_attacks[64];

u64 mask_rook(uint pos) {
	u64 mask = 0;
	int r = pos / 8, f = pos % 8; // not uint to avoid underflow on r-1 / f-1
	for (int i = r + 1; i <= 6; i++) mask |= (1ul << (i * 8 + f));
	for (int i = r - 1; i >= 1; i--) mask |= (1ul << (i * 8 + f));
	for (int i = f + 1; i <= 6; i++) mask |= (1ul << (r * 8 + i));
	for (int i = f - 1; i >= 1; i--) mask |= (1ul << (r * 8 + i));
	return mask;
}

u64 mask_bishop(uint pos) {
	u64 mask = 0;
	int r = pos / 8, f = pos % 8;
	for (int i = r + 1, j = f + 1; i <= 6 && j <= 6; i++, j++) mask |= (1ul << (i * 8 + j));
	for (int i = r + 1, j = f - 1; i <= 6 && j >= 1; i++, j--) mask |= (1ul << (i * 8 + j));
	for (int i = r - 1, j = f + 1; i >= 1 && j <= 6; i--, j++) mask |= (1ul << (i * 8 + j));
	for (int i = r - 1, j = f - 1; i >= 1 && j >= 1; i--, j--) mask |= (1ul << (i * 8 + j));
	return mask;
}

u64 generate_rook_attacks_slow(uint pos, u64 occ) {
	u64 attacks = 0;
	int r = pos / 8, f = pos % 8;
	for (int i = r + 1; i <= 7; i++) { attacks |= (1ul << (i * 8 + f)); if (occ & (1ul << (i * 8 + f))) break; }
	for (int i = r - 1; i >= 0; i--) { attacks |= (1ul << (i * 8 + f)); if (occ & (1ul << (i * 8 + f))) break; }
	for (int i = f + 1; i <= 7; i++) { attacks |= (1ul << (r * 8 + i)); if (occ & (1ul << (r * 8 + i))) break; }
	for (int i = f - 1; i >= 0; i--) { attacks |= (1ul << (r * 8 + i)); if (occ & (1ul << (r * 8 + i))) break; }
	return attacks;
}

u64 generate_bishop_attacks_slow(uint pos, u64 occ) {
	u64 attacks = 0;
	int r = pos / 8, f = pos % 8;
	for (int i = r + 1, j = f + 1; i <= 7 && j <= 7; i++, j++) { attacks |= (1ul << (i * 8 + j)); if (occ & (1ul << (i * 8 + j))) break; }
	for (int i = r + 1, j = f - 1; i <= 7 && j >= 0; i++, j--) { attacks |= (1ul << (i * 8 + j)); if (occ & (1ul << (i * 8 + j))) break; }
	for (int i = r - 1, j = f + 1; i >= 0 && j <= 7; i--, j++) { attacks |= (1ul << (i * 8 + j)); if (occ & (1ul << (i * 8 + j))) break; }
	for (int i = r - 1, j = f - 1; i >= 0 && j >= 0; i--, j--) { attacks |= (1ul << (i * 8 + j)); if (occ & (1ul << (i * 8 + j))) break; }
	return attacks;
}

void init_attacks() {
	int rook_idx = 0;
	int bishop_idx = 0;

	for (uint pos = 0; pos < 64; pos++) {
		rook_masks[pos] = mask_rook(pos);
		rook_offs[pos] = rook_idx;

		u64 subset = 0;
		do {
			uint pext_idx = _pext_u64(subset, rook_masks[pos]);
			rook_attacks[rook_offs[pos] + pext_idx] = generate_rook_attacks_slow(pos, subset);

			subset = (subset - rook_masks[pos]) & rook_masks[pos]; // generate all submasks
		} while (subset);

		rook_idx += 1ul << std::__popcount(rook_masks[pos]);

		bishop_masks[pos] = mask_bishop(pos);
		bishop_offs[pos] = bishop_idx;

		subset = 0;
		do {
			uint pext_idx = _pext_u64(subset, bishop_masks[pos]);
			bishop_attacks[bishop_offs[pos] + pext_idx] = generate_bishop_attacks_slow(pos, subset);

			subset = (subset - bishop_masks[pos]) & bishop_masks[pos];
		} while (subset);

		bishop_idx += 1ul << std::__popcount(bishop_masks[pos]);
	}
	for (uint pos = 0; pos < 64; pos++) {
		u64 attacks = 0;
		uint r = pos / 8; // rank
		uint f = pos % 8; // file

		// 8 coordinate deltas for a knight jump
		int dr[] = {-2, -2, -1, -1,  1,  1,  2,  2};
		int dc[] = {-1,  1, -2,  2, -2,  2, -1,  1};

		for (uint i = 0; i < 8; i++) {
			int tr = r + dr[i];
			int tc = f + dc[i];

			// verify validity of target
			if (tr >= 0 && tr < 8 && tc >= 0 && tc < 8)
			attacks |= (1ul << (tr * 8 + tc));
		}

		knight_attacks[pos] = attacks;
	}
}

u64 get_rook_attacks(uint pos, u64 occ) {
	return rook_attacks[rook_offs[pos] + _pext_u64(occ, rook_masks[pos])];
}

u64 get_bishop_attacks(uint pos, u64 occ) {
	return bishop_attacks[bishop_offs[pos] + _pext_u64(occ, bishop_masks[pos])];
}

// Returns all squares a king on pos can reach (one step in all eight directions).
// Column guards prevent wrapping across board edges.
u64 get_king_attacks(uint pos)
{
	u64 attacks = 0;
	uint col = pos & 7;
	if (col > 0) { // not on column a
		attacks |= (1ul << (pos - 1)); // left
		if (pos >= 9)
			attacks |= (1ul << (pos - 9)); // up-left
		if (pos < 57)
			attacks |= (1ul << (pos + 7)); // down-left
	}
	if (col < 7) { // not on column h
		attacks |= (1ul << (pos + 1)); // right
		if (pos >= 7)
			attacks |= (1ul << (pos - 7)); // up-right
		if (pos < 55)
			attacks |= (1ul << (pos + 9)); // down-right
	}
	if (pos >= 8)
		attacks |= (1ul << (pos - 8)); // up
	if (pos < 56)
		attacks |= (1ul << (pos + 8)); // down
	return attacks;
}

u64 get_knight_attacks(uint pos)
{
	return knight_attacks[pos];
}

// returns the two diagonal, not forward push squares.
u64 get_pawn_attacks(uint pos, piece_color color)
{
	u64 attacks = 0;
	uint col = pos & 7;
	if (col == WHITE) {
		if (pos >= 8) {
			if (col > 0)
				attacks |= (1ul << (pos - 9)); // up-left
			if (col < 7)
				attacks |= (1ul << (pos - 7)); // up-right
		}
	} else {
		if (pos < 56) {
			if (col > 0)
				attacks |= (1ul << (pos + 7)); // down-left
			if (col < 7)
				attacks |= (1ul << (pos + 9)); // down-right
		}
	}
	return attacks;
}

// Dispatch attack generation by piece type.
u64 get_piece_attacks(enum piece_type piece, piece_color col, uint pos, u64 occ)
{
	switch (piece) {
	case ROOK:
		return get_rook_attacks(pos, occ);
	case BISHOP:
		return get_bishop_attacks(pos, occ);
	case QUEEN:
		return get_rook_attacks(pos, occ) | get_bishop_attacks(pos, occ);
	case KING:
		return get_king_attacks(pos);
	case KNIGHT:
		return get_knight_attacks(pos);
	case PAWN:
		return get_pawn_attacks(pos, col);
	default:
		return 0;
	}
}
