#include "../core/engine-main.h"

// --- TUI Compatibility Layer Global State ---
char board[64];
bool turn;

// We need a global instance of your new board struct to track the real state
struct board g_real_board;

// --- Converter Tables ---
// Maps [piece_color][piece_type] to character
static const char piece_to_char[2][7] = {
	{ ' ', 'K', 'P', 'N', 'B', 'R', 'Q' }, // WHITE
	{ ' ', 'k', 'p', 'n', 'b', 'r', 'q' } // BLACK
};

// Maps character back to piece type and color
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

// --- Sync Functions ---

// Call this right after modifying g_real_board to update the TUI variables
void sync_to_tui(const struct board &src_board, piece_color current_turn)
{
	for (int i = 0; i < 64; ++i) {
		piece_type t = src_board.piece(i);
		piece_color c = src_board.color(i);
		board[i] = piece_to_char[static_cast<u64>(c)][static_cast<u64>(t)];
	}
	turn = (current_turn == WHITE);
}

// Call this if you ever manually modify the TUI 'board' char array
// and need to push those changes back into your depth-search struct.
void sync_from_tui(struct board &dest_board)
{
	for (int i = 0; i < 64; ++i) {
		piece_type t;
		piece_color c;
		if (char_to_piece(board[i], t, c)) {
			dest_board.set_piece(t, c, i);
		} else {
			dest_board.set_piece(EMPTY, WHITE, i);
		}
	}
}


// TUI interface wrapper for move validation
extern bool is_valid_move1(u8 from, u8 to)
{
	struct move m = { from, to };
	piece_color current_turn = turn ? WHITE : BLACK;
	
	u64 white, black;
	get_occupancy(&white, &black, &g_real_board);
	u64 pseudo_moves = get_piece_pseudo_moves(&g_real_board, m.from, white, black);
	if (!(pseudo_moves & (1ul << m.to))) return false;
	
	return is_valid_move(m, g_real_board, current_turn);
}

// TUI interface wrapper for move execution
extern void execute_move(u8 from, u8 to)
{
	struct move m = { from, to };

	// 1. Execute the move on the real, structured board
	make_move(&g_real_board, m);

	// 2. Flip the side to move
	piece_color next_turn = turn ? BLACK : WHITE;

	// 3. Sync everything back to the TUI primitive globals so it renders perfectly
	sync_to_tui(g_real_board, next_turn);
}

void init_castling(struct board &b, uint black, uint white)
{
	b.castle_black = black;
	b.castle_white = white;
}
