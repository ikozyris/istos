#include "engine-main.h"
#include "attacks.h"
#include "evaluate-tables.h"
#include <algorithm>
#include <cstdio>

inline void pick_best_move(struct move *moves, int *scores, uint current_idx, uint move_count)
{
	uint best_idx = current_idx;
	for (uint j = current_idx + 1; j < move_count; ++j)
		if (scores[j] > scores[best_idx])
			best_idx = j;

	std::swap(moves[current_idx], moves[best_idx]);
	std::swap(scores[current_idx], scores[best_idx]);
}

// scores a move for the selection sort algorithm
int score_move(const board *b, struct move m, struct move hash_move, const struct move *killer_moves, piece_color turn)
{
	// Principal Variation from previous iteration
	if (m.from == hash_move.from && m.to == hash_move.to)
		return 30000;

	piece_type attacker = b->piece(m.from);
	piece_type victim = b->piece(m.to);

	// capture
	if (victim != EMPTY) {
		// prioritize taking valuable victim using weak attacker (MVV/LVA)
		int score = 20000 + (PIECE_WEIGHTS[victim] * 10) - PIECE_WEIGHTS[attacker];

		// recapture last moved piece
		// if (m.to == last_target_sq)
			// score += 1000;
		return score;
	}
	// en passant is also a capture
	else if (attacker == PAWN && m.to == b->en_passant_sq)
		return 20000 + (100 * 10) - 100;

	else if (attacker == PAWN && (m.to >= 56 || m.to <= 8)) // promotion
		return 30000;

	if (m.from == killer_moves[0].from && m.to == killer_moves[0].to)
		return 15000;

	if (m.from == killer_moves[1].from && m.to == killer_moves[1].to)
		return 10000;

	uint to = turn == WHITE ? m.to : m.to ^ 56;
	return (int)PST_BONUS[attacker][to]; // default for quiet moves
}

const int INF = 30000;
int evaluate_board(const board &b);

template <piece_color Turn, bool FullyLegal>
uint generate_and_score_moves(const board *b, struct move *moves, int *scores, u64 white, u64 black, struct move hash_move, const struct move *killer_moves)
{
	uint move_count = 0;
	u64 my_pieces = (Turn == WHITE) ? white : black;
	while (my_pieces) {
		uint from = __builtin_ctzll(my_pieces);
		u64 targets = get_piece_pseudo_moves(b, from, white, black);

		while (targets) {
			uint to = __builtin_ctzll(targets);
			struct move m = {(u8)from, (u8)to};

			if constexpr (FullyLegal) {
				if (!is_valid_move(m, *b, Turn)) {
					targets &= (targets - 1);
					continue;
				}
			}

			moves[move_count] = m;
			scores[move_count] = score_move(b, m, hash_move, killer_moves, Turn);
			move_count++;

			targets &= (targets - 1); // clear lowest set bit
		}
		my_pieces &= (my_pieces - 1);
	}
	return move_count;
}

template <piece_color Turn>
int alpha_beta(board b, uint depth, int alpha, int beta, uint ply, struct move (&killer_moves)[32][2])
{
	int original_alpha = alpha;
	int original_beta = beta;

	if (depth == 0)
		return evaluate_board(b);

	// TT Probe
	struct tt_entry &tte = TT[b.hash_key % TT_SIZE];
	struct move hash_move = {64, 64};
	if (tte.key == b.hash_key) {
		hash_move = tte.best_move;
		if (tte.depth >= depth) {
			if (tte.flag == EXACT)
				return tte.score;
			if (tte.flag == UPPERBOUND && tte.score <= alpha)
				return tte.score;
			if (tte.flag == LOWERBOUND && tte.score >= beta)
				return tte.score;
		}
	}

	u64 white, black;
	get_occupancy(&white, &black, &b);

	struct move pseudo_moves[224];
	int scores[224];

	// generate pseudo-legal moves
	uint move_count = generate_and_score_moves<Turn, false>(
		&b, pseudo_moves, scores, white, black, hash_move, killer_moves[ply]);

	uint legal_moves_searched = 0;
	int best_eval = (Turn == WHITE) ? -INF : INF;
	struct move best_move_this_node = {64, 64};

	// next turn is determined statically at compile time
	constexpr piece_color NextTurn = (Turn == WHITE) ? BLACK : WHITE;

	for (uint i = 0; i < move_count; ++i) {
		// lazy selection sort: find only the single best move left
		pick_best_move(pseudo_moves, scores, i, move_count);

		if (scores[i] <= 40 && i > 17) { // filter quiet moves
			if (depth == 1 && best_eval + 100 < alpha)
				continue;
			if (depth >= 2 && depth <= 3)
				depth--;
		}

		// avoid checking validity for every move
		if (!is_valid_move(pseudo_moves[i], b, Turn))
			continue;

		legal_moves_searched++;

		struct board next_b = b;
		make_move(&next_b, pseudo_moves[i]);

		// recursively call the opposite color's compiled function
		int eval = alpha_beta<NextTurn>(next_b, depth - 1, alpha, beta, ply + 1, killer_moves);

		if constexpr (Turn == WHITE) {
			if (eval > best_eval) {
				best_eval = eval;
				best_move_this_node = pseudo_moves[i];
			}
			alpha = std::max(alpha, eval);
		} else {
			if (eval < best_eval) {
				best_eval = eval;
				best_move_this_node = pseudo_moves[i];
			}
			beta = std::min(beta, eval);
		}

		if (beta <= alpha) {
			if (b.piece(pseudo_moves[i].to) == EMPTY) { // quiet but important moves
				if (killer_moves[ply][0].from == 64)
					killer_moves[ply][0] = pseudo_moves[i];
				else
					killer_moves[ply][1] = pseudo_moves[i];
			}
			break; // Cutoff
		}
	}

	if (legal_moves_searched == 0) {
		u64 occ = white | black;
		if (is_in_check(&b, Turn, occ)) // prefer shorter checkmates
			return (Turn == WHITE) ? -INF + (100 - depth) : INF - (100 - depth);
		return 0; // Stalemate
	}

	// TT Store
	struct tt_entry &store_tte = TT[b.hash_key % TT_SIZE];
	store_tte.key = b.hash_key;
	store_tte.best_move = best_move_this_node;
	store_tte.score = best_eval;
	store_tte.depth = depth;

	if (best_eval <= original_alpha)
		store_tte.flag = UPPERBOUND;
	else if (best_eval >= original_beta)
		store_tte.flag = LOWERBOUND;
	else
		store_tte.flag = EXACT;

	return best_eval;
}

struct move get_best_move(struct board b, uint target_depth, piece_color turn)
{
	static bool initialized = false;
	if (!initialized) {
		init_zobrist();
		TT = new tt_entry[TT_SIZE]();
		initialized = true;
	}

	struct move killer_moves[32][2];
	for (uint d = 0; d < 32; ++d) {
		killer_moves[d][0] = {64, 64};
		killer_moves[d][1] = {64, 64};
	}

	b.hash_key = compute_hash(b);
	if (turn == BLACK) b.hash_key ^= zobrist_turn;

	u64 white, black;
	get_occupancy(&white, &black, &b);

	struct move root_moves[224];
	int scores[224];
	struct move best_move_overall = {64, 64};

	// generate root fully-legal moves, with empty hash move
	uint move_count = (turn == WHITE) ?
			generate_and_score_moves<WHITE, true>(&b, root_moves, scores, white, black, best_move_overall, killer_moves[0]) :
			generate_and_score_moves<BLACK, true>(&b, root_moves, scores, white, black, best_move_overall, killer_moves[0]);

	best_move_overall = root_moves[0];
	int eval = 0;

	// iterative deepening
	for (uint cur_depth = 1; cur_depth <= target_depth; ++cur_depth) {
		int alpha = -INF;
		int beta = INF;
		struct move best_move_this_depth = root_moves[0];
		int best_eval = (turn == WHITE) ? -INF : INF;

		for (uint i = 0; i < move_count; ++i) {
			// sort based on scores array which after depth 1 has real evaluation
			pick_best_move(root_moves, scores, i, move_count);

			struct board next_b = b;
			make_move(&next_b, root_moves[i]);

			int current_eval = (turn == WHITE) ? alpha_beta<BLACK>(next_b, cur_depth - 1, alpha, beta, 1, killer_moves) :
							     alpha_beta<WHITE>(next_b, cur_depth - 1, alpha, beta, 1, killer_moves);

			scores[i] = current_eval; // update score array with real evaluation

			if (turn == WHITE) {
				if (current_eval > best_eval) {
					best_eval = current_eval;
					best_move_this_depth = root_moves[i];
				}
				alpha = std::max(alpha, current_eval);
			} else {
				if (current_eval < best_eval) {
					best_eval = current_eval;
					best_move_this_depth = root_moves[i];
				}
				beta = std::min(beta, current_eval);
			}
		}

		eval = best_eval;
		best_move_overall = best_move_this_depth;

		printf("depth %d: best move: %d to %d, eval: %d\n", cur_depth, best_move_overall.from,
		       best_move_overall.to, eval);
	}

	return best_move_overall;
}
