#include "../core/engine-main.h"

char board[64];
bool turn; // white=0

struct board g_real_board;

// map [piece_color][piece_type] to character
static const char piece_to_char[2][7] = {
	{ ' ', 'K', 'P', 'N', 'B', 'R', 'Q' }, // WHITE
	{ ' ', 'k', 'p', 'n', 'b', 'r', 'q' } // BLACK
};

static bool char_to_piece(char c, piece_type& type, piece_color& col) {
    switch (c) {
	case 'K': type = KING;   col = WHITE; return true;
	case 'P': type = PAWN;   col = WHITE; return true;
	case 'N': type = KNIGHT; col = WHITE; return true;
	case 'B': type = BISHOP; col = WHITE; return true;
	case 'R': type = ROOK;   col = WHITE; return true;
	case 'Q': type = QUEEN;  col = WHITE; return true;
	
	case 'k': type = KING;   col = BLACK; return true;
	case 'p': type = PAWN;   col = BLACK; return true;
	case 'n': type = KNIGHT; col = BLACK; return true;
	case 'b': type = BISHOP; col = BLACK; return true;
	case 'r': type = ROOK;   col = BLACK; return true;
	case 'q': type = QUEEN;  col = BLACK; return true;
	default:  type = EMPTY;  col = WHITE; return false;
    }
}

void sync_from_tui(struct board &dest_board)
{
	for (uint i = 0; i < 64; ++i) {
		piece_type t;
		piece_color c;
		if (char_to_piece(board[i], t, c))
			dest_board.set_piece(t, c, i);
		else
			dest_board.set_piece(EMPTY, WHITE, i);
	}
}

void init_castling(struct board &b, uint black, uint white)
{
	b.castle_black = black;
	b.castle_white = white;
}


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
