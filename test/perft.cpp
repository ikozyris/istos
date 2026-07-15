#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "../core/engine-main.h"
#include "../common.h"

board g_board;
piece_color turn;

// copied from engine_uci_interface
void set_position_from_fen(board *b, const char *fen) {
    char pieces[65], turnChar, castlingState[5], enPassantSquare[3];
    int halfMoves, fullMoves;

    sscanf(fen, "%64s %c %4s %2s %d %d", pieces, &turnChar, castlingState, enPassantSquare, &halfMoves, &fullMoves);

    // set pieces in position
    int count = 0;
    for (int i = 0; i < strlen(pieces); i++) {
        const char c = pieces[i];
        if (!isalnum(c)) continue; // skip invalid characters
        if (isdigit(c)) {
            // if the character is a digit, mark this many places as empty
            const int num = c - '0';
            for (int j = 0; j < num; j++) {
                g_board.set_piece(EMPTY, WHITE, count++);
            }
            continue;
        }

        piece_color color = BLACK;
        piece_type type = EMPTY;
        if (isupper(c)) color = WHITE;
        bool valid = true;
        switch (tolower(c)) {
            case 'p':
                type = PAWN;
                break;
            case 'n':
                type = KNIGHT;
                break;
            case 'b':
                type = BISHOP;
                break;
            case 'r':
                type = ROOK;
                break;
            case 'q':
                type = QUEEN;
                break;
            case 'k':
                type = KING;
                break;
            default:
                valid = false;
                break;
        }
        if (!valid) continue;
        b->set_piece(type, color, count++);
    }

    if (turnChar == 'w') turn = WHITE;
    else if (turnChar == 'b') turn = BLACK;

    b->castle_white = 0;
    b->castle_black = 0;
    b->en_passant_sq = 64;
    if (strcmp(castlingState, "-")) {
        for (int i = 0; i < strlen(castlingState); i++) {
            const char c = castlingState[i];
            if (c == 'K') b->castle_white |= (1 << 1);
            else if (c == 'Q') b->castle_white |= (1 << 0);
            else if (c == 'k') b->castle_black |= (1 << 1);
            else if (c == 'q') b->castle_black |= (1 << 0);
        }
    }

    // set en passant square
    if (
        strcmp(enPassantSquare, "-") && // en passant square is not '-'
        (enPassantSquare[0] >= 'a' && enPassantSquare[0] <= 'h') && // first character is between a and h
        (enPassantSquare[1] >= '0' && enPassantSquare[1] <= '8')    // second character is between 0 and 8
    ) {
        const int idx = enPassantSquare[0] - 'a' + (8 - (enPassantSquare[1] - '0')) * 8;
        b->en_passant_sq = idx;
    }
}

void square_to_algebraic(int idx, char* const squareStr) {
    squareStr[0] = 'a' + (idx % 8);
    squareStr[1] = '0' + (8 - idx / 8);
    squareStr[2] = '\0';
}

void move_to_uci(struct move m, char* const moveStr, const struct board &b) {
    square_to_algebraic(m.from, moveStr);
    square_to_algebraic(m.to, &moveStr[2]);
    // check if move is promotion
    if (
        (m.to / 8 == 0 || m.to / 8 == 7) &&
        b.piece(m.from) == PAWN
    ) {
        moveStr[4] = 'q'; // TODO: change this when engine supports underpromotion
        moveStr[5] = '\0';
        return;
    }
    moveStr[4] = '\0';
}

uint64_t generate_legal_moves(const board *b, struct move* moveList)
{
    uint move_count = 0;
    u64 white = b->colors[WHITE], black = b->colors[BLACK];
    u64 my_pieces = (turn == WHITE) ? white : black;
    while (my_pieces) {
        uint from = __builtin_ctzll(my_pieces);
        u64 targets = get_piece_pseudo_moves(b, from, white, black);

        while (targets) {
            uint to = __builtin_ctzll(targets);
            struct move m = {(u8)from, (u8)to};

            if (!is_valid_move(m, *b, turn)) {
                targets &= (targets - 1);
                continue;
            }

            moveList[move_count] = m;
            move_count++;

            targets &= (targets - 1); // clear lowest set bit
        }
        my_pieces &= (my_pieces - 1);
    }
    return move_count;
}

uint64_t perft(int depth) {
    struct move moveList[256];
    int moveCount;
    uint64_t nodes = 0;
    static bool display = true;
    bool prevDisplay;
    char moveStr[6];

    moveCount = generate_legal_moves(&g_board, moveList);
    if (depth == 1) {
        if (display) {
            // print moves even if depth is 1
            for (int i = 0; i < moveCount; i++) {
                move_to_uci(moveList[i], moveStr, g_board);
                printf("%s: 1\n", moveStr);
            }
        }
        return moveCount;
    }
    for (int i = 0; i < moveCount; i++) {
        struct board prevBoard = g_board;

        prevDisplay = display;
        display = false;
        make_move(&g_board, moveList[i]);
        turn = (piece_color)(1 - turn);
        int moveNodes = perft(depth - 1);
        nodes += moveNodes;
        display = prevDisplay;

        if (display) {
            move_to_uci(moveList[i], moveStr, g_board);
            printf("%s: %d\n", moveStr, moveNodes);
        }

        g_board = prevBoard; // unmake move
        turn = (piece_color)(1 - turn);
    }

    return nodes;
}

int main(int argc, const char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <depth> [position FEN]", argv[0]);
        return -1;
    }
    int depth;
    const char* fen;

    sscanf(argv[1], "%d", &depth);
    if (argc == 2)
        fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    else
        fen = argv[2];
    set_position_from_fen(&g_board, fen);
    engine_init();

    const uint64_t nodes = perft(depth);
    printf("\nTotal nodes: %lu\n", nodes);
    return 0;
}
