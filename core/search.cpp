#include "engine-main.h"
#include "attacks.h"
#include "evaluate-tables.h"

const uint MAX_DEPTH = 63;

char LMR_table[MAX_DEPTH][256];
void init_LMR() {
	for (uint d = 3; d < MAX_DEPTH; ++d)
		for (uint m = 5; m < 256; ++m)
			LMR_table[d][m] = 1 + std::log(d) * std::log(m) / 2.25;
}

inline void pick_best_move(struct move *moves, short *scores, uint current_idx, uint move_count)
{
	uint best_idx = current_idx;
	for (uint j = current_idx + 1; j < move_count; ++j)
		if (scores[j] > scores[best_idx])
			best_idx = j;

	std::swap(moves[current_idx], moves[best_idx]);
	std::swap(scores[current_idx], scores[best_idx]);
}

// scores a move for the selection sort algorithm
short score_move(const board *b, struct move m, struct move hash_move, const struct move *killer_moves, piece_color turn, const short history[7][64])
{
	// Principal Variation from previous iteration
	if (m.from == hash_move.from && m.to == hash_move.to)
		return 29000;

	piece_type attacker = b->piece(m.from);
	piece_type victim = b->piece(m.to);

	// capture
	if (victim != EMPTY) {
		// prioritize taking valuable victim using weak attacker (MVV/LVA)
		short score = 20000 + (PIECE_WEIGHTS_MG[victim] * 10) - PIECE_WEIGHTS_MG[attacker];

		// recapture last moved piece
		// if (m.to == last_target_sq)
			// score += 1000;
		return score;
	}
	// en passant is also a capture
	else if (attacker == PAWN && m.to == b->en_passant_sq)
		return 20000 + (100 * 10) - 100;

	else if (attacker == PAWN && (m.to >= 56 || m.to < 8)) // promotion
		return 28000;

	if (m.from == killer_moves[0].from && m.to == killer_moves[0].to)
		return 15000;

	if (m.from == killer_moves[1].from && m.to == killer_moves[1].to)
		return 10000;

	uint from_sq = turn == WHITE ? m.from : m.from ^ 56;
	uint to_sq   = turn == WHITE ? m.to   : m.to ^ 56;
	short pst_delta = PST_MG[attacker][to_sq] - PST_MG[attacker][from_sq]; // for endgame, history is more important
	return pst_delta + history[attacker][m.to]; // default for quiet moves
}

const short INF = 30000;
short evaluate_board(const board &b);

template <piece_color Turn, bool FullyLegal, bool OnlyCaptures>
uint generate_and_score_moves(const board *b, struct move *moves, short *scores, u64 white, u64 black, struct move hash_move, const struct move *killer_moves, const short history[7][64])
{
	uint move_count = 0;
	u64 my_pieces = (Turn == WHITE) ? white : black;
	while (my_pieces) {
		uint from = __builtin_ctzll(my_pieces);
		u64 targets = get_piece_pseudo_moves(b, from, white, black);

		if constexpr (OnlyCaptures) {
			targets &= (Turn == WHITE) ? black : white;
		}

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
			scores[move_count] = score_move(b, m, hash_move, killer_moves, Turn, history);
			move_count++;

			targets &= (targets - 1); // clear lowest set bit
		}
		my_pieces &= (my_pieces - 1);
	}
	return move_count;
}

const struct move dummy_killers[2] = {{64,64}, {64,64}};
struct move dummy_hash_move = {64, 64};
const short dummy_history[7][64] = {};

template <piece_color Turn>
short quiescence(const board &b, short alpha, short beta) {
	short stand_pat = evaluate_board(b); // current evaluation if we don't do any captures

	if constexpr (Turn == BLACK) {
		stand_pat = -stand_pat;
	}
	stand_pat += TEMPO_BONUS;

	if (stand_pat >= beta)
		return beta;
	if (stand_pat > alpha)
		alpha = stand_pat;


	u64 white, black;
	get_occupancy(&white, &black, &b);
	struct move moves[40];
	short scores[40];
	uint move_cnt = generate_and_score_moves<Turn, true, true>(&b, moves, scores, white, black, dummy_hash_move, dummy_killers, dummy_history);

	for (uint i = 0; i < move_cnt; ++i) {	// captures only
		pick_best_move(moves, scores, i, move_cnt);
		struct move m = moves[i];

		if (stand_pat + PIECE_WEIGHTS_MG[b.piece(m.to)] + 200 < alpha)
			continue;

		board b2 = b;
		make_move(&b2, m);
		short score = -quiescence<Turn == WHITE ? BLACK : WHITE>(b2, -beta, -alpha);

		if (score >= beta)
			return beta;
		if (score > alpha)
			alpha = score;
	}

	return alpha;
}

template <piece_color Turn, bool AllowNull = true>
short alpha_beta(const board &b, uint depth, short alpha, short beta, uint ply, struct move killer_moves[MAX_DEPTH][2], short history[2][7][64])
{
	short original_alpha = alpha;
	short original_beta = beta;

	if (depth == 0) {
		if constexpr (Turn == WHITE) {
			return quiescence<WHITE>(b, alpha, beta);
		} else {
			return -quiescence<BLACK>(b, -beta, -alpha);
		}
	}

	struct move hash_move = {64, 64};
	short res = tt_probe(b.hash_key, depth, alpha, beta, hash_move);
	if (res != TT_MAGIC_FAIL_EVAL)
		return res;

	u64 white, black;
	get_occupancy(&white, &black, &b);

	u64 occ = white | black;
	bool checked = is_in_check(&b, Turn, occ);

	short stand_eval = evaluate_board(b);
	if (depth <= 3 && !checked) { // reverse futility pruning
		short margin = 120 * depth;
		if (stand_eval - margin >= beta)
			return stand_eval;
	}

	// Null Move Pruning
	if constexpr (AllowNull) {
		if (depth >= 3 && !checked) {
			board null_b = b;
			null_b.hash_key ^= zobrist_turn;
			if (null_b.en_passant_sq != NO_EP) {
				null_b.hash_key ^= zobrist_ep[null_b.en_passant_sq];
				null_b.en_passant_sq = NO_EP;
				null_b.hash_key ^= zobrist_ep[NO_EP];
			}

			constexpr piece_color NullTurn = (Turn == WHITE) ? BLACK : WHITE;
			int R = 2 + depth / 6;
			int r_depth = std::max(0, (int)depth - 1 - R);
			short null_score;
			if constexpr (Turn == WHITE)
				null_score = alpha_beta<NullTurn, false>(null_b, r_depth, beta - 1, beta, ply + 1, killer_moves, history);
			else
				null_score = alpha_beta<NullTurn, false>(null_b, r_depth, alpha, alpha + 1, ply + 1, killer_moves, history);

			if constexpr (Turn == WHITE) {
				if (null_score >= beta)
					return beta;
			} else {
				if (null_score <= alpha)
					return alpha;
			}
		}
	}

	struct move pseudo_moves[224];
	short scores[224];

	// generate pseudo-legal moves
	uint move_count = generate_and_score_moves<Turn, false, false>(
		&b, pseudo_moves, scores, white, black, hash_move, killer_moves[ply], history[Turn]);

	uint legal_moves_searched = 0;
	short best_eval = (Turn == WHITE) ? -INF : INF;
	struct move best_move = {64, 64};

	// next turn is determined statically at compile time
	constexpr piece_color NextTurn = (Turn == WHITE) ? BLACK : WHITE;

	for (uint i = 0; i < move_count; ++i) {
		// lazy selection sort: find only the single best move left
		pick_best_move(pseudo_moves, scores, i, move_count);

		if (depth <= 2 && scores[i] < 9000) { // futility pruning
			short margin = (depth == 1) ? 300 : 500;
			if (stand_eval + margin <= alpha)
				continue;
		}

		// avoid checking validity for every move
		if (!is_valid_move(pseudo_moves[i], b, Turn))
			continue;

		legal_moves_searched++;

		struct board next_b = b;
		make_move(&next_b, pseudo_moves[i]);

		short eval;

		// Late Move Reductions: reduce depth for late quiet moves
		if (scores[i] < 9000 && legal_moves_searched > 4 && depth >= 3) {
			uint m_idx = std::min(255u, legal_moves_searched);
			uint d_idx = std::min(63u, depth);
			int r = LMR_table[d_idx][m_idx];
			int r_depth = std::max(0, (int)depth - 1 - r);
			if constexpr (Turn == WHITE) // null window
				eval = alpha_beta<NextTurn>(next_b, r_depth, alpha, alpha + 1, ply + 1, killer_moves, history);
			else
				eval = alpha_beta<NextTurn>(next_b, r_depth, beta - 1, beta, ply + 1, killer_moves, history);

			// re-search at full depth if reduced search improved (this is likely a good move)
			bool needs_re_search;
			if constexpr (Turn == WHITE)
				needs_re_search = (eval > alpha);
			else
				needs_re_search = (eval < beta);

			if (needs_re_search)
				eval = alpha_beta<NextTurn>(next_b, depth - 1, alpha, beta, ply + 1, killer_moves, history);
		} else if (legal_moves_searched == 1) {
			eval = alpha_beta<NextTurn>(next_b, depth - 1, alpha, beta, ply + 1, killer_moves, history);
		} else {
			// PVS: null window search for non-PV moves
			if constexpr (Turn == WHITE) // null window
				eval = alpha_beta<NextTurn>(next_b, depth - 1, alpha, alpha + 1, ply + 1, killer_moves, history);
			else
				eval = alpha_beta<NextTurn>(next_b, depth - 1, beta - 1, beta, ply + 1, killer_moves, history);

			bool needs_re_search;
			if constexpr (Turn == WHITE)
				needs_re_search = (eval > alpha && eval < beta);
			else
				needs_re_search = (eval < beta && eval > alpha);

			if (needs_re_search)
				eval = alpha_beta<NextTurn>(next_b, depth - 1, alpha, beta, ply + 1, killer_moves, history);
		}

		if constexpr (Turn == WHITE) {
			if (eval > best_eval) {
				best_eval = eval;
				best_move = pseudo_moves[i];
			}
			alpha = std::max(alpha, eval);
		} else {
			if (eval < best_eval) {
				best_eval = eval;
				best_move = pseudo_moves[i];
			}
			beta = std::min(beta, eval);
		}

		if (beta <= alpha) {
			if (b.piece(pseudo_moves[i].to) == EMPTY) { // quiet but important moves
				if (killer_moves[ply][0].from == 64)
					killer_moves[ply][0] = pseudo_moves[i];
				else
					killer_moves[ply][1] = pseudo_moves[i];

				// update history heuristic
				piece_type pt = b.piece(pseudo_moves[i].from);
				history[Turn][pt][pseudo_moves[i].to] += depth * depth;
				if (history[Turn][pt][pseudo_moves[i].to] > 8192) {
					for (uint c = 0; c < 2; ++c)
						for (uint p = 0; p < 7; ++p)
							for (uint s = 0; s < 64; ++s)
								history[c][p][s] /= 2;
				}
			}
			break; // Cutoff
		}
	}

	if (legal_moves_searched == 0) {
		u64 occ = white | black;
		if (is_in_check(&b, Turn, occ)) // prefer shorter checkmates
			best_eval = (Turn == WHITE) ? (-INF + ply*ply) : (INF - ply*ply);
		else
			best_eval = 0; // Stalemate
	}

	tt_flag flag;
	if (best_eval <= original_alpha)
		flag = TT_UPPER;
	else if (best_eval >= original_beta)
		flag = TT_LOWER;
	else
		flag = TT_EXACT;
	tt_store(b.hash_key, best_move, best_eval, depth, flag);

	return best_eval;
}

void tt_clear() {
	if (TT) std::fill(TT, TT + TT_BUCKETS * TT_BUCKET_SIZE, tt_entry{});
}

void engine_init() {
	init_zobrist();
	init_LMR();
	init_attacks();
	TT = (tt_entry*)aligned_alloc(64, TT_BUCKETS * TT_BUCKET_SIZE * sizeof(tt_entry));
	tt_clear();
}

void search_init(struct board& b, piece_color turn, struct move root_moves[224], short scores[224], struct move killer_moves[MAX_DEPTH][2],
		short history[2][7][64], uint &move_count, struct move& best_move)
{
	for (uint d = 0; d < MAX_DEPTH; ++d) {
		killer_moves[d][0] = {64, 64};
		killer_moves[d][1] = {64, 64};
	}

	b.hash_key = compute_hash(b);
	if (turn == BLACK)
		b.hash_key ^= zobrist_turn;

	u64 white, black;
	get_occupancy(&white, &black, &b);

	// generate root fully-legal moves, with empty hash move
	move_count = (turn == WHITE) ?
			generate_and_score_moves<WHITE, true, false>(&b, root_moves, scores, white, black, best_move, killer_moves[0], history[WHITE]) :
			generate_and_score_moves<BLACK, true, false>(&b, root_moves, scores, white, black, best_move, killer_moves[0], history[BLACK]);

	best_move = root_moves[0];
}

void search_loop(uint cur_depth, piece_color turn, struct move root_moves[224], struct move killer_moves[MAX_DEPTH][2], short history[2][7][64],
		short scores[224], uint move_count, const struct board &b, short &eval, struct move &best_move)
{
	short aspiration_delta = 35;
aspiration_retry:
	// aspiration window: use narrow window around previous eval
	short alpha = (cur_depth > 3) ? eval - aspiration_delta : -INF;
	short beta  = (cur_depth > 3) ? eval + aspiration_delta :  INF;

	short best_eval = (turn == WHITE) ? -INF : INF;

	for (uint i = 0; i < move_count; ++i) {
		// sort based on scores array which after depth 1 has real evaluation
		pick_best_move(root_moves, scores, i, move_count);

		struct board next_b = b;
		make_move(&next_b, root_moves[i]);

		short current_eval = (turn == WHITE)
				? alpha_beta<BLACK>(next_b, cur_depth - 1, alpha, beta, 1, killer_moves, history)
				: alpha_beta<WHITE>(next_b, cur_depth - 1, alpha, beta, 1, killer_moves, history);

		scores[i] = current_eval; // update score array with real evaluation

		if (turn == WHITE) {
			if (current_eval > best_eval) {
				best_eval = current_eval;
				best_move = root_moves[i];
			}
			alpha = std::max(alpha, current_eval);
		} else {
			if (current_eval < best_eval) {
				best_eval = current_eval;
				best_move = root_moves[i];
			}
			beta = std::min(beta, current_eval);
		}
	}

	// aspiration window failed; redo search
	if (cur_depth > 3 && (best_eval <= eval - aspiration_delta || best_eval >= eval + aspiration_delta)) {
		if (best_eval > INF - 900 || best_eval < -INF + 900) { // skip aspiration entirely for mates
			alpha = -INF;
			beta  =  INF;
		} else {
			aspiration_delta *= 2;
#ifdef LOG
			printf("aspr_wnd retry: %d vs %d, a: %d b: %d\n", best_eval, eval, alpha, beta);
#endif
			goto aspiration_retry;
		}
	}

	eval = best_eval;
}

#define BOLD  "\x1b[1m"
#define RESET "\x1b[0m"
#define GRAY  "\x1b[90m"

constexpr bool timing = true;
struct move get_best_move(struct board b, uint target_depth, piece_color turn)
{
	struct move killer_moves[MAX_DEPTH][2];
	short history[2][7][64] = {};
	short eval = 0;
	struct move root_moves[224];
	short scores[224];
	uint move_count;
	struct move best_move;

	u64 prev_nodes = 20;

	search_init(b, turn, root_moves, scores, killer_moves, history, move_count, best_move);

	Clock::time_point searchStart = Clock::now();
	// iterative deepening
	for (uint cur_depth = 1; cur_depth <= target_depth; ++cur_depth) {
		best_move = root_moves[0];

		search_loop(cur_depth, turn, root_moves, killer_moves,
			history, scores, move_count, b,	eval, best_move);

		printf("depth %u: eval: " BOLD "%d" RESET ", nodes: %lu, expf: " BOLD "%.1f" RESET, cur_depth,
			eval, nodes, (double)nodes / (double)prev_nodes);

		if constexpr (timing) {
			Clock::time_point searchEnd = Clock::now();
			Clock::duration searchDuration = searchEnd - searchStart;
			double dur = (double)searchDuration.count() / 1e6;
			printf(", time: %.1fms, NPS: %.1f", dur, nodes/(dur*1e3));
		}
		putc('\n', stdout);
		prev_nodes = nodes;

		struct board tmp_b = b;
		make_move(&tmp_b, best_move);
		const uint lines = 10;
		struct move cur_pv[lines];
		cur_pv[0] = best_move;
		uint pv_cnt = extract_pv(tmp_b, std::min(cur_depth, lines-1), cur_pv + 1);
		uint i = 0;
		if (turn == BLACK) {
			printf("1. \t\t" GRAY "%u -> %u" RESET "\n", cur_pv[i].from, cur_pv[i].to);
			i = 1;
		}
		for (; i + 1 <= pv_cnt; i += 2)
			printf("%u. " BOLD "%u -> %u" RESET "\t" GRAY "%u -> %u" RESET "\n",
				(i+1)/2+1, cur_pv[i].from, cur_pv[i].to, cur_pv[i + 1].from, cur_pv[i + 1].to);
		putc('\n', stdout);
	}

	return best_move;
}
