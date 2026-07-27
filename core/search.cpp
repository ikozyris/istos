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
		short score = 20000 + (PIECE_WEIGHTS[victim] * 10) - PIECE_WEIGHTS[attacker];

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

struct move_gen_ctx { // better than copying all these on each call
	const board *b;
	struct move *moves;
	short *scores;
	uint *move_count;
	u64 enemy_pieces;
	const struct move *killer_moves;
	const short (*history)[64];
	struct move hash_move;
	piece_color turn;
	bool fully_legal;
	bool only_captures;
};

void _add_moves(uint from, u64 targets, struct move_gen_ctx *ctx)
{
	if (ctx->only_captures)
		targets &= ctx->enemy_pieces;

	while (targets) {
		uint to = __builtin_ctzll(targets);
		targets &= (targets - 1);
		struct move m = {(u8)from, (u8)to};

		if (ctx->fully_legal && !is_valid_move(m, *ctx->b, ctx->turn))
			continue;

		ctx->moves[*ctx->move_count] = m;
		ctx->scores[*ctx->move_count] = score_move(ctx->b, m, ctx->hash_move, ctx->killer_moves, ctx->turn, ctx->history);
		(*ctx->move_count)++;
	}
}

template <bool FullyLegal, bool OnlyCaptures>
uint generate_and_score_moves(const board *b, struct move *moves, short *scores, struct move hash_move, const struct move *killer_moves, const short history[7][64], piece_color turn)
{
	uint move_count = 0;
	u64 my_pieces = b->colors[turn];
	u64 enemy_pieces = b->colors[1 - turn];
	u64 occ = my_pieces | enemy_pieces;

	struct move_gen_ctx ctx = { b, moves, scores, &move_count, enemy_pieces, killer_moves, 
		history, hash_move, turn, FullyLegal, OnlyCaptures }; // TODO: benchmark if this is slow

	u64 knights = b->pieces[KNIGHT] & my_pieces;
	while (knights) {
		uint from = __builtin_ctzll(knights);
		_add_moves(from, get_knight_attacks(from) & ~my_pieces, &ctx);
		knights &= (knights - 1);
	}

	u64 bishops = b->pieces[BISHOP] & my_pieces;
	while (bishops) {
		uint from = __builtin_ctzll(bishops);
		_add_moves(from, get_bishop_attacks(from, occ) & ~my_pieces, &ctx);
		bishops &= (bishops - 1);
	}

	u64 rooks = b->pieces[ROOK] & my_pieces;
	while (rooks) {
		uint from = __builtin_ctzll(rooks);
		_add_moves(from, get_rook_attacks(from, occ) & ~my_pieces, &ctx);
		rooks &= (rooks - 1);
	}

	u64 queens = b->pieces[QUEEN] & my_pieces;
	while (queens) {
		uint from = __builtin_ctzll(queens);
		_add_moves(from, (get_rook_attacks(from, occ) | get_bishop_attacks(from, occ)) & ~my_pieces, &ctx);
		queens &= (queens - 1);
	}

	u64 kings = b->pieces[KING] & my_pieces;
	while (kings) {
		uint from = __builtin_ctzll(kings);
		u64 attacks = get_king_attacks(from);
		if constexpr (!OnlyCaptures)
			attacks |= get_king_castling_moves(b, from, turn, occ);
		_add_moves(from, attacks & ~my_pieces, &ctx);
		kings &= (kings - 1);
	}

	u64 pawns = b->pieces[PAWN] & my_pieces;
	struct pawn_moves pm = get_pawns_moves(b, turn, my_pieces, enemy_pieces);
	int d_push1 = turn == WHITE ? 8 : -8;
	int d_push2 = turn == WHITE ? 16 : -16;
	int d_capL = turn == WHITE ? 9 : -7;
	int d_capR = turn == WHITE ? 7 : -9;

	if constexpr (!OnlyCaptures) {
		while (pm.push1) {
			uint to = __builtin_ctzll(pm.push1);
			_add_moves(to + d_push1, 1ull << to, &ctx);
			pm.push1 &= (pm.push1 - 1);
		}
		while (pm.push2) {
			uint to = __builtin_ctzll(pm.push2);
			_add_moves(to + d_push2, 1ull << to, &ctx);
			pm.push2 &= (pm.push2 - 1);
		}
	}
	while (pm.capL) {
		uint to = __builtin_ctzll(pm.capL);
		_add_moves(to + d_capL, 1ull << to, &ctx);
		pm.capL &= (pm.capL - 1);
	}
	while (pm.capR) {
		uint to = __builtin_ctzll(pm.capR);
		_add_moves(to + d_capR, 1ull << to, &ctx);
		pm.capR &= (pm.capR - 1);
	}
	return move_count;
}

const struct move dummy_killers[2] = {{64,64}, {64,64}};
struct move dummy_hash_move = {64, 64};
const short dummy_history[7][64] = {};

uint generate_legal_moves(const board *b, struct move *moves, piece_color turn) {
	short scores[256];
	return generate_and_score_moves<true, false>(b, moves, scores, dummy_hash_move, dummy_killers, dummy_history, turn);
}

short quiescence(const board &b, short alpha, short beta, piece_color turn) {
	short stand_pat = evaluate_board(b); // current evaluation if we don't do any captures

	if (turn == BLACK)
		stand_pat = -stand_pat;
	stand_pat += TEMPO_BONUS;

	if (stand_pat >= beta)
		return beta;
	if (stand_pat > alpha)
		alpha = stand_pat;

	struct move moves[40];
	short scores[40];
	uint move_cnt = generate_and_score_moves<true, true>(&b, moves, scores, dummy_hash_move, dummy_killers, dummy_history, turn);

	for (uint i = 0; i < move_cnt; ++i) {	// captures only
		pick_best_move(moves, scores, i, move_cnt);
		struct move m = moves[i];

		if (stand_pat + PIECE_WEIGHTS[b.piece(m.to)] + 200 < alpha)
			continue;

		board b2 = b;
		make_move(&b2, m);
		short score = -quiescence(b2, -beta, -alpha, (piece_color)(1 - turn));

		if (score >= beta)
			return beta;
		if (score > alpha)
			alpha = score;
	}

	return alpha;
}

template <bool AllowNull = true>
short alpha_beta(const board &b, uint depth, short alpha, short beta, uint ply, struct move killer_moves[MAX_DEPTH][2],
		short history[2][7][64], piece_color turn)
{
	short original_alpha = alpha;

	if (depth == 0)
		return quiescence(b, alpha, beta, turn);

	struct move hash_move = {64, 64};
	short res = tt_probe(b.hash_key, depth, alpha, beta, hash_move);
	if (res != TT_MAGIC_FAIL_EVAL)
		return res;

	u64 occ = b.colors[WHITE] | b.colors[BLACK];
	bool checked = is_in_check(&b, turn, occ);

	short stand_eval = evaluate_board(b);
	if (turn == BLACK)
		stand_eval = -stand_eval; // evaluation function is absolute

	if (depth <= 3 && !checked) { // reverse futility pruning
		short margin = 120 * depth;
		if (stand_eval - margin >= beta)
			return stand_eval;
	}

	piece_color next_turn = (piece_color)(1 - turn);

	// Null Move Pruning
	if constexpr (AllowNull) {
		if (depth >= 3 && !checked && b.phase > 214) {
			board null_b = b;
			null_b.hash_key ^= zobrist_turn;
			if (null_b.en_passant_sq != NO_EP) {
				null_b.hash_key ^= zobrist_ep[null_b.en_passant_sq];
				null_b.en_passant_sq = NO_EP;
				null_b.hash_key ^= zobrist_ep[NO_EP];
			}

			int R = 2 + depth / 6;
			int r_depth = std::max(0, (int)depth - 1 - R);

			short null_score = -alpha_beta<false>(null_b, r_depth, -beta, -beta + 1, ply + 1, killer_moves, history, next_turn);

			if (null_score >= beta)
				return beta;
		}
	}

	struct move pseudo_moves[224];
	short scores[224];

	// generate pseudo-legal moves
	uint move_count = generate_and_score_moves<false, false>(&b, pseudo_moves, scores,
		hash_move, killer_moves[ply], history[turn], turn);

	uint legal_moves_searched = 0;
	short best_eval = -INF;
	struct move best_move = {64, 64};

	for (uint i = 0; i < move_count; ++i) {
		// lazy selection sort: find only the single best move left
		pick_best_move(pseudo_moves, scores, i, move_count);

		if (depth <= 2 && scores[i] < 9000) { // futility pruning
			const short margin = (depth == 1) ? 300 : 500;
			if (stand_eval + margin <= alpha)
				continue;
		}

		// avoid checking validity for every move
		if (!is_valid_move(pseudo_moves[i], b, turn))
			continue;

		legal_moves_searched++;

		struct board next_b = b;
		make_move(&next_b, pseudo_moves[i]);

		short eval;

		// Late Move Reductions: reduce depth for late quiet moves (+ very high history moves)
		if (scores[i] < 4000 && legal_moves_searched > 4 && depth >= 3) {
			uint m_idx = std::min(255u, legal_moves_searched);
			uint d_idx = std::min(63u, depth);
			int r = LMR_table[d_idx][m_idx];
			int r_depth = std::max(0, (int)depth - 1 - r);

			eval = -alpha_beta(next_b, r_depth, -alpha - 1, -alpha, ply + 1, killer_moves, history, next_turn);

			if (eval > alpha)
				eval = -alpha_beta(next_b, depth - 1, -beta, -alpha, ply + 1, killer_moves, history, next_turn);
		} else if (legal_moves_searched > 1) {
			// PVS: null window search for non-PV moves
			eval = -alpha_beta(next_b, depth - 1, -alpha - 1, -alpha, ply + 1, killer_moves, history, next_turn);

			if (eval > alpha && eval < beta)
				eval = -alpha_beta(next_b, depth - 1, -beta, -alpha, ply + 1, killer_moves, history, next_turn);
		} else
			eval = -alpha_beta(next_b, depth - 1, -beta, -alpha, ply + 1, killer_moves, history, next_turn);

		if (eval > best_eval) {
			best_eval = eval;
			best_move = pseudo_moves[i];
		}
		alpha = std::max(alpha, eval);

		if (alpha >= beta) {
			if (scores[i] < 9000) { // quiet but important moves
				if (*(u16*)&pseudo_moves[i] != *(u16*)&killer_moves[ply][0]) { // no duplicates
					killer_moves[ply][1] = killer_moves[ply][0];
					killer_moves[ply][0] = pseudo_moves[i];
				}

				// update history heuristic
				piece_type pt = b.piece(pseudo_moves[i].from);
				history[turn][pt][pseudo_moves[i].to] += depth * depth;
				if (history[turn][pt][pseudo_moves[i].to] > 8192) {
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
		if (is_in_check(&b, turn, occ)) // prefer shorter checkmates
			best_eval = -INF + ply;
		else
			best_eval = 0; // Stalemate
	}

	tt_flag flag;
	if (best_eval <= original_alpha)
		flag = TT_UPPER;
	else if (best_eval >= beta)
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

	uint sz =  TT_BUCKETS * TT_BUCKET_SIZE * sizeof(tt_entry);
	TT = (tt_entry*)mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | 
			MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);
	if (TT == MAP_FAILED) {
#ifdef LOG
		printf("Requested size = %u MiB\n", sz / (1024*1024));
		puts(RED "Failed to alloc with mmap" RESET);
#endif
		TT = (tt_entry*)aligned_alloc(64, sz);
	}
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

	// generate root fully-legal moves, with empty hash move
	move_count = generate_and_score_moves<true, false>(&b, root_moves, scores, best_move, killer_moves[0], history[turn], turn);

	best_move = root_moves[0];
}

void search_loop(uint cur_depth, piece_color turn, struct move root_moves[224], struct move killer_moves[MAX_DEPTH][2], short history[2][7][64],
		short scores[224], uint move_count, const struct board &b, short &eval, struct move &best_move)
{
	short eval_turn = (turn == WHITE) ? eval : -eval;
	short aspiration_delta = 25;
	if (cur_depth <= 6)
		aspiration_delta = 100;

aspiration_retry:
	// aspiration window: use narrow window around previous eval
	short alpha = eval_turn - aspiration_delta;
	short beta  = eval_turn + aspiration_delta;

	short best_eval = -INF;

	for (uint i = 0; i < move_count; ++i) {
		// sort based on scores array which after depth 1 has real evaluation
		pick_best_move(root_moves, scores, i, move_count);

		struct board next_b = b;
		make_move(&next_b, root_moves[i]);

		short current_eval = -alpha_beta(next_b, cur_depth - 1, -beta, -alpha, 1, killer_moves, history, (piece_color)(1 - turn));

		scores[i] = current_eval; // update score array with real evaluation

		if (current_eval > best_eval) {
			best_eval = current_eval;
			best_move = root_moves[i];
		}
		alpha = std::max(alpha, current_eval);
	}

	// aspiration window failed; redo search
	if (best_eval <= eval_turn - aspiration_delta || best_eval >= eval_turn + aspiration_delta) {
		if (best_eval > MATE_ABS_THRESHOLD || best_eval < -MATE_ABS_THRESHOLD) { // skip aspiration entirely for mates
			alpha = -INF;
			beta  =  INF;
		} else {
			aspiration_delta *= 2;
#ifdef LOG
			printf(RED "aspr_wnd retry: %d vs %d, a: %d b: %d\n" RESET, best_eval, eval_turn, alpha, beta);
#endif
			goto aspiration_retry;
		}
	}

	eval = (turn == WHITE) ? best_eval : -best_eval;
}

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
		const uint lines = 4;
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
