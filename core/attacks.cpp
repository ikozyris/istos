#include "engine-main.h"
#include "attacks.h"

// the first blocker is included in occupancy table

// total number of permutations needed for all 64 squares
const uint ROOK_TABLE_SIZE = 102400;
const uint BISHOP_TABLE_SIZE = 5248;

struct pext_bbt rook_pext[64];
struct pext_bbt bishop_pext[64];

// attack lookup tables
u64 rook_attacks[ROOK_TABLE_SIZE];
u64 bishop_attacks[BISHOP_TABLE_SIZE];
u64 knight_attacks[64];
u64 king_attacks[64];
u64 pawn_attacks[2][64];

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
	uint rook_idx = 0;
	uint bishop_idx = 0;

	for (uint pos = 0; pos < 64; pos++) { // rooks and bishops
		rook_pext[pos].mask = mask_rook(pos);
		rook_pext[pos].attacks = rook_attacks + rook_idx;

		u64 subset = 0;
		do {
			uint pext_idx = _pext_u64(subset, rook_pext[pos].mask);
			rook_pext[pos].attacks[pext_idx] = generate_rook_attacks_slow(pos, subset);

			subset = (subset - rook_pext[pos].mask) & rook_pext[pos].mask; // generate all submasks
		} while (subset);

		rook_idx += 1ul << std::__popcount(rook_pext[pos].mask);

		bishop_pext[pos].mask = mask_bishop(pos);
		bishop_pext[pos].attacks = bishop_attacks + bishop_idx;

		subset = 0;
		do {
			uint pext_idx = _pext_u64(subset, bishop_pext[pos].mask);
			bishop_pext[pos].attacks[pext_idx] = generate_bishop_attacks_slow(pos, subset);

			subset = (subset - bishop_pext[pos].mask) & bishop_pext[pos].mask;
		} while (subset);

		bishop_idx += 1ul << std::__popcount(bishop_pext[pos].mask);
	}

	for (uint pos = 0; pos < 64; pos++) { // knight, king, pawn 
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

		u64 k_attacks = 0;
		if (f > 0) {
			k_attacks |= (1ul << (pos - 1));
			if (pos >= 9) k_attacks |= (1ul << (pos - 9));
			if (pos < 57) k_attacks |= (1ul << (pos + 7));
		} if (f < 7) {
			k_attacks |= (1ul << (pos + 1));
			if (pos >= 7) k_attacks |= (1ul << (pos - 7));
			if (pos < 55) k_attacks |= (1ul << (pos + 9));
		}
		if (pos >= 8) k_attacks |= (1ul << (pos - 8));
		if (pos < 56) k_attacks |= (1ul << (pos + 8));
		king_attacks[pos] = k_attacks;

		u64 p_attacks_w = 0;
		u64 p_attacks_b = 0;
		if (pos >= 8) {
			if (f > 0) p_attacks_w |= (1ul << (pos - 9));
			if (f < 7) p_attacks_w |= (1ul << (pos - 7));
		} if (pos < 56) {
			if (f > 0) p_attacks_b |= (1ul << (pos + 7));
			if (f < 7) p_attacks_b |= (1ul << (pos + 9));
		}
		pawn_attacks[WHITE][pos] = p_attacks_w;
		pawn_attacks[BLACK][pos] = p_attacks_b;
	}
}

// dispatch attack generation by piece type
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
