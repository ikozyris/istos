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

Search throughput: more than 8 million legal moves simulated per second (single thread).

From starting position:
| depth | time | nodes       | exp factor | NPS |
|-------|------|-------------|------------|-----|
| 10	| 0.05 |     413,252 | 2	  | 8.3 |
| 11	| 0.14 |   1,136,388 | 2.75	  | 8.5 |
| 12	| 1.1  |   2,557,844 | 2.25	  | 8.3 |
| 13	| 0.69 |   5,642,759 | 2.2	  | 8.2 |
| 14	| 5.9  |  46,110,124 | 8.17       | 7.8 |
| 15	| >24000| >200bn     |		  | 8?	|

Prunning still needs more work
