# Chess Engine
Code under core/

## Architecture

### Search

Searching using iterative deepening over alpha-beta minimax algorithm.
Sorts legal move with lazy selection sort and scores them using as heuristics:
 - last PV move
 - 2 killer moves from sibling branches on same ply
 - MVV/LVA
 - PST tables as default

For quiet move prunning:
 - Aspiration Windows
 - Null Move Prunning
 - History Heuristic
 - Late Move Reductions


At the start of each call to alpha-beta it tries to retrieve the evaluation from the transposition table, saves evaluation at end of function.

At the end of each depth alpha-beta calls quiescence negamax search to search for captures.

### Attack Generation

Utilizes PEXT instructions and lookup tables for rook, bishop and knight. Used for pseudo-move generation.

### Data Structures

Zobrist hashing for transposition table. Bitboard for white and black pieces + mailbox for fast piece lookups.

### Evaluation

Incremental evaluation for piece values and PST tables only for middlegame. Then on the whole board mobility and pawn structures.

## Performance Characteristics

Search throughput: 8 million legal moves simulated per second (single thread).

From starting position:
| depth | time | nodes       | exp factor | NPS |
|-------|------|-------------|------------|-----|
| 10	| 0.13 |     946'386 | 2.5	  | 7.3 |
| 11	| 0.39 |   2'929'940 | 3	  | 7.5 |
| 12	| 1.1  |   8'641'203 | 2.9	  | 7.9 |
| 13	| 9.36 |  74'266'874 | 8.6	  | 7.9 |
| 14	| >5000 | >40bn      | >160       | 8?  |

Prunning still needs more work
