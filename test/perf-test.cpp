#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../common.h"
#include "bridge.cpp"

int main()
{
	engine_init();
	set_starting_position();
	printf("\ta\t%u\n", g_real_board.phase);
	uint depth = 18;
	struct move a = get_best_move(g_real_board, depth, static_cast<piece_color>(turn));
	printf("best move: %u -> %u, nodes %lu\n", a.from, a.to, nodes);
}
