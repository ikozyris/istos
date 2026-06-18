#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../common.h"
#include "../core/attacks.h"
#include "bridge.cpp"

// Resets the board to the classic chess starting position
void set_starting_position()
{
	const char *start_layout = "rnbqkbnr"
				   "pppppppp"
				   "        "
				   "        "
				   "        "
				   "        "
				   "PPPPPPPP"
				   "RNBQKBNR";

	for (int i = 0; i < 64; ++i)
		board[i] = start_layout[i];
	turn = WHITE;
	sync_from_tui(g_real_board);
	init_castling(g_real_board, 3, 3);
}


int main()
{
	init_attacks();
	set_starting_position();
	init_time_management(120'000, 1'000);
	uint depth = 11;
	struct move a = get_best_move(g_real_board, depth, static_cast<piece_color>(turn));
	printf("best move: %u -> %u, nodes %u", a.from, a.to, nodes);
}
