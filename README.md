# Chess Engine

## Architecture

### Search

Searching using iterative deepening over alpha-beta minimax algorithm.
Sorts legal move with lazy selection sort and scores them using as heuristics:
 - last PV move
 - 2 killer moves from sibling branches on same ply
 - MVV/LVA
 - PST tables as default

For quiet move prunning:
 - Aspiration Windows (88 centipawns currently, may need better evaluation function)
 - Null Move Prunning
 - History Heuristic
 - Late Move Reductions


At the start of each call to alpha-beta it tries to retrieve the evaluation from the transposition table,
 saves evaluation at end of function.

At the end of each depth alpha-beta calls quiescence negamax search to search for captures.

### Attack Generation

Utilizes PEXT instructions and lookup tables for rook, bishop and knight. Used for pseudo-move generation.

### Data Structures

Zobrist hashing for transposition table. Bitboard for white and black pieces + mailbox for fast piece lookups.

### Evaluation

Incremental evaluation for piece values and PST tables only for middlegame. Then on the whole board mobility and pawn structures.

## Performance Characteristics

Search throughput: ~8 million legal moves simulated per second (single thread).
Branching factor: super-exponential: 12/11 = 3, 13/12 = 15 :(

From starting position:
0.7s for depth 11 (5683887 nodes)
2.6s for depth 12 (21317984 nodes)
33s  for depth 13 (257424907 nodes)
>4m for depth 14
