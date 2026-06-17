#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../common.h"
#include "../core/attacks.h"
#include "bridge.cpp"

int parse_square(const char *sq)
{
	if (sq[0] < 'a' || sq[0] > 'h' || sq[1] < '1' || sq[1] > '8')
		return -1;

	int col = sq[0] - 'a';
	int row = '8' - sq[1];

	return row * 8 + col;
}

void get_square_name(int sq, char *buf)
{
	int row = sq / 8;
	int col = sq % 8;
	buf[0] = 'a' + col;
	buf[1] = '8' - row;
	buf[2] = '\0';
}

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

	for (int i = 0; i < 64; ++i) {
		board[i] = start_layout[i];
	}
	turn = true; // White to move at setup
	sync_from_tui(g_real_board);
	init_castling(g_real_board, 3, 3);
}

// Validates the piece type character
bool is_valid_piece_char(char c)
{
	const char *valid = "KQRBNPkqrbnp. ";
	return strchr(valid, c) != NULL;
}

bool has_legal_moves(piece_color col)
{
	for (int from = 0; from < 64; ++from) {
		if (g_real_board.piece(from) != EMPTY && g_real_board.color(from) == col) {
			for (int to = 0; to < 64; ++to) {
				struct move m = { (u8)from, (u8)to };
				if (is_valid_move(m, g_real_board, col)) {
					return true;
				}
			}
		}
	}
	return false;
}

// -----------------------------------------------------------------------------
// BOARD VISUALIZATION
// -----------------------------------------------------------------------------
void print_board_state(int selected_sq)
{
	bool valid_moves[64] = { false };

	if (selected_sq != -1) {
		for (int i = 0; i < 64; ++i) {
			if (is_valid_move1((unsigned int)selected_sq, (unsigned int)i)) {
				valid_moves[i] = true;
			}
		}
	}

	printf("\n    a  b  c  d  e  f  g  h\n");
	printf("  +------------------------+\n");

	for (int r = 0; r < 8; ++r) {
		printf("%d |", 8 - r);

		for (int c = 0; c < 8; ++c) {
			int index = r * 8 + c;
			char p = board[index];
			bool is_move = valid_moves[index];
			bool is_selected = (index == selected_sq);

			if (p == '\0' || p == ' ') {
				if (is_move) {
					printf(" \033[32m*\033[0m ");
				} else {
					bool is_dark_square = (r + c) % 2 != 0;
					printf(is_dark_square ? " . " : "   ");
				}
			} else {
				if (is_selected) {
					printf(" \033[43;30m%c\033[0m ", p);
				} else if (is_move) {
					printf("[\033[31m%c\033[0m]", p);
				} else {
					if (p < 94)
						printf(" \033[1;37m%c\033[0m ", p);
					else
						printf(" \033[1;30m%c\033[0m ", p);
				}
			}
		}
		printf("| %d\n", 8 - r);
	}

	printf("  +------------------------+\n");
	printf("    a  b  c  d  e  f  g  h\n\n");
}

// -----------------------------------------------------------------------------
// GAME LOOP
// -----------------------------------------------------------------------------
void play_interactive_game()
{
	char input[256];
	int selected_sq = -1;
	uint engine_depth = 11;
	bool user_color_white = true;

	memset(board, ' ', 64);

	printf("==================================================\n");
	printf("             C CHESS ENGINE TERMINAL              \n");
	printf("==================================================\n");
	printf(" Gameplay Commands:\n");
	printf("   s e2          -> Select piece at e2\n");
	printf("   p e4          -> Play selected piece to e4\n");
	printf("   eval 5        -> Evaluate with depth 5\n");
	printf(" Setup Commands:\n");
	printf("   start         -> Reset board to classic opening state\n");
	printf("   set <P> <sq>  -> Place piece P (e.g., set P e4, set k e8)\n");
	printf("                    Use '.' or ' ' as P to clear a square\n");
	printf("   q             -> Quit\n");
	printf("==================================================\n");

	// Initial configuration
	printf("Play as White or Black? (w/b, default w): ");
	if (fgets(input, sizeof(input), stdin)) {
		input[strcspn(input, "\n")] = 0;
		if (strcmp(input, "b") == 0 || strcmp(input, "B") == 0) {
			user_color_white = false;
		}
	}

	set_starting_position();

	// If player is Black, engine immediately plays first move for White
	if (!user_color_white) {
		printf("Engine thinking (depth %u)...\n", engine_depth);
		struct move a = get_best_move(g_real_board, engine_depth, WHITE);
		if (a.from != 64 && a.to != 64) {
			char from_name[3], to_name[3];
			get_square_name(a.from, from_name);
			get_square_name(a.to, to_name);
			printf("Engine played: %s%s\n", from_name, to_name);
			execute_move(a.from, a.to);
		}
	}

	while (true) {
		print_board_state(selected_sq);

		if (selected_sq == -1) {
			printf("%s to move. Enter command: ", turn ? "White" : "Black");
		} else {
			printf("Piece selected. Play move (p e4) or cancel (c): ");
		}

		if (!fgets(input, sizeof(input), stdin))
			break;
		input[strcspn(input, "\n")] = 0;

		if (strcmp(input, "q") == 0) {
			printf("Exiting game...\n");
			break;
		} else if (strncmp(input, "eval ", 5) == 0) {
			uint depth = 0;
			sscanf(input + 4, "%u", &depth);
			struct move a = get_best_move(g_real_board, depth, static_cast<piece_color>(turn));
			printf("best move: %u -> %u\n", a.from, a.to);
			continue;
		}

		// Reset to Starting Position
		else if (strcmp(input, "start") == 0) {
			printf("Play as White or Black? (w/b, default w): ");
			if (fgets(input, sizeof(input), stdin)) {
				input[strcspn(input, "\n")] = 0;
				if (strcmp(input, "b") == 0 || strcmp(input, "B") == 0) {
					user_color_white = false;
				} else {
					user_color_white = true;
				}
			}
			set_starting_position();
			selected_sq = -1;
			printf("\033[32m[+] Board reset.\033[0m\n");

			if (!user_color_white) {
				printf("Engine thinking (depth %u)...\n", engine_depth);
				struct move a = get_best_move(g_real_board, engine_depth, WHITE);
				if (a.from != 64 && a.to != 64) {
					char from_name[3], to_name[3];
					get_square_name(a.from, from_name);
					get_square_name(a.to, to_name);
					printf("Engine played: %s%s\n", from_name, to_name);
					execute_move(a.from, a.to);
				}
			}
			continue;
		}

		// Create a piece at a position (e.g., "set P e4")
		else if (strncmp(input, "set ", 4) == 0 && strlen(input) >= 8) {
			char piece = input[4];

			if (!is_valid_piece_char(piece)) {
				printf("\033[31m[!] Invalid piece character. Use K,Q,R,B,N,P (caps=white, lower=black) or '.'\033[0m\n");
				continue;
			}

			int sq = parse_square(input + 6);
			if (sq != -1) {
				board[sq] = (piece == '.') ? ' ' : piece;
				selected_sq = -1;
				sync_from_tui(g_real_board);
				printf("\033[32m[+] Square updated.\033[0m\n");
			} else {
				printf("\033[31m[!] Invalid square coordinates.\033[0m\n");
			}
			continue;
		}

		// Handle 'Select' Command (e.g., "s e2")
		if (input[0] == 's' && input[1] == ' ' && strlen(input) >= 4) {
			int sq = parse_square(input + 2);
			if (sq != -1) {
				char p = board[sq];
				if (p == '\0' || p == ' ') {
					printf("\033[31m[!] No piece at that square.\033[0m\n");
				} else {
					selected_sq = sq;
				}
			} else {
				printf("\033[31m[!] Invalid square.\033[0m\n");
			}
		}
		// Handle 'Play' Command (e.g., "p e4")
		else if (input[0] == 'p' && input[1] == ' ' && strlen(input) >= 4) {
			if (selected_sq == -1) {
				printf("\033[31m[!] No piece selected. Use 's' first.\033[0m\n");
				continue;
			}

			int to_sq = parse_square(input + 2);
			if (to_sq != -1) {
				if (is_valid_move1((unsigned int)selected_sq, (unsigned int)to_sq)) {
					execute_move(selected_sq, to_sq);
					selected_sq = -1;

					// Check if engine color matches new turn
					piece_color current_turn = turn ? WHITE : BLACK;
					if ((user_color_white && current_turn == BLACK) || (!user_color_white && current_turn == WHITE)) {
						u64 white, black;
						get_occupancy(&white, &black, &g_real_board);
						if (!has_legal_moves(current_turn)) {
							print_board_state(-1);
							if (is_in_check(&g_real_board, current_turn, white | black)) {
								printf("\n*** CHECKMATE! You win! ***\n");
							} else {
								printf("\n*** STALEMATE! Game is a draw. ***\n");
							}
							break;
						}

						print_board_state(-1);
						printf("Engine thinking (depth %u)...\n", engine_depth);
						struct move a = get_best_move(g_real_board, engine_depth, current_turn);
						if (a.from != 64 && a.to != 64) {
							char from_name[3], to_name[3];
							get_square_name(a.from, from_name);
							get_square_name(a.to, to_name);
							printf("Engine played: %s%s\n", from_name, to_name);
							execute_move(a.from, a.to);

							// Check if player has moves left
							piece_color player_turn = turn ? WHITE : BLACK;
							get_occupancy(&white, &black, &g_real_board);
							if (!has_legal_moves(player_turn)) {
								print_board_state(-1);
								if (is_in_check(&g_real_board, player_turn, white | black)) {
									printf("\n*** CHECKMATE! Engine wins! ***\n");
								} else {
									printf("\n*** STALEMATE! Game is a draw. ***\n");
								}
								break;
							}
						} else {
							printf("\n*** Engine has no moves. Game over. ***\n");
							break;
						}
					}
				} else {
					printf("\033[31m[!] Illegal move.\033[0m\n");
				}
			} else {
				printf("\033[31m[!] Invalid square.\033[0m\n");
			}
		} else if (strcmp(input, "c") == 0) {
			selected_sq = -1;
			printf("Selection cancelled.\n");
		} else {
			printf("\033[31m[!] Unknown or malformed command.\033[0m\n");
		}
	}
}

int main()
{
	init_attacks();
	sync_to_tui(g_real_board, WHITE);
	play_interactive_game();
	return 0;
}
