# Chess Engine
Code in core/

In this repo there are only test programs for the engine, a standard UCI interface is avaiable on 
a separate [engine-uci-interface](https://git.svlachos.duckdns.org/spiros/engine-uci-interface) repo.

The engine is licensed under the GNU GPL v3.

Version 0.8

## Architecture

### Search

Searching using iterative deepening over alpha-beta minimax algorithm.
Sorts legal move with lazy selection sort and scores them using as heuristics:
 - last PV move
 - 2 killer moves from sibling branches on same ply
 - MVV/LVA
 - Move History + PST tables as default

For quiet move pruning:
 - Aspiration Windows
 - Null Move Pruning (dynamic depth reduction)
 - Late Move Reductions
 - PVS (null window search)
 - (Reverse) Futility Pruning

At the start of each call alpha-beta it tries to retrieve the evaluation from the transposition table, saves evaluation at end of function.

When depth reaches 0, alpha-beta calls quiescence negamax search to search for captures, which reduces the Horizon Effect.

### Attack Generation

Utilizes PEXT instructions and lookup tables for rook, bishop and knight. Used for pseudo-move generation.

### Data Structures

High density lock-less Transposition table:
 - 8 byte entry, 8 entries per cluster (fits in cache line)
 - effectively 49-bit key for 2M clusters:
   - high 28 bits of 64-bit zobrist key
   - low 21 bits for indexing low 
 - atomic read/write operations
 - depth-based replacement

Bitboard for white and black pieces + mailbox for fast piece lookups.

### Evaluation

Incremental evaluation for piece values and PST with middlegame/endgame interpolation. Then on the whole board mobility and pawn structures.
PSTs were adapted from Stockfish's legacy HCE evaluation.

## Performance Characteristics

Search throughput: 7 million legal moves simulated per second (single thread).
Branching factor: 1.5x to 2.5x (more nodes per ply searched)
