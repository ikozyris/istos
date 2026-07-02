#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../common.h"
#include "bridge.cpp"

void set_starting_position()
{
	const constexpr char *start_layout =
	       //abcdefgh	
		"rnbqkbnr"  // 8
		"pppppppp"
		"        "  // 6
		"        "
		"        "  // 4
		"        "
		"PPPPPPPP"
		"RNBQKBNR"; // 1
	       //abcdefgh

	static_assert(start_layout[64] == 0);
	
	for (uint i = 0; i < 64; ++i)
		board[i] = start_layout[i];
	turn = WHITE;
	sync_from_tui(g_real_board);
	init_castling(g_real_board, 3, 3);
}


int main()
{
	engine_init();
	set_starting_position();
	printf("\ta\t%u\n", g_real_board.phase);
	uint depth = 18;
	struct move a = get_best_move(g_real_board, depth, static_cast<piece_color>(turn));
	printf("best move: %u -> %u, nodes %lu\n", a.from, a.to, nodes);
}
