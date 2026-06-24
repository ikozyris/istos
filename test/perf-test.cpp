#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../common.h"
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
	set_starting_position();
	printf("\ta\t%u\n", g_real_board.phase);
	init_time_management(120'000, 1'000);
	uint depth = 10;
	struct move a = get_best_move(g_real_board, depth, static_cast<piece_color>(turn));
	printf("best move: %u -> %u, nodes %lu\n", a.from, a.to, nodes);
}
