# Istos
Name is inspired by the fastest fish: sailfish, which is in the istiophorus family.

Code in core/

In this repo there are only test programs for the engine, a standard UCI interface is not public at the moment.

The engine is licensed under the GNU GPL v3.

Version 0.8.9

## Architecture

### Search

Searching using iterative deepening over alpha-beta negamax algorithm.
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

When depth reaches 0, alpha-beta calls quiescence search to search for captures, which reduces the Horizon Effect.

Repetitions are handled with move history stack storing full key and checking N-2x plies back for 1 repetition.

### Attack Generation

For pseudo-legal move generation: 
 - PEXT bitboards for rooks and bishops
 - Simple lookup tables for attacks of knights, kings and pawns

Fully legal bulk pawn moves generation (bit shift on bitmask of all pawns) 

### Data Structures

High density lock-less Transposition table:
 - 8 byte entry, 8 entries per cluster (fits in cache line)
 - Effectively 49-bit key for 260K clusters:
   - high 31 bits of 64-bit zobrist key written on entry
   - low 18 bits used for indexing (depends on entries allocated)
 - Atomic read/write operations
 - Depth-based replacement

Bitboard for white and black pieces + mailbox for fast piece lookups.

### Evaluation

Incremental evaluation for piece values and PST with middlegame/endgame interpolation.
Then on the whole board:
 - Mobility (extra bonus for attacking enemy king)
 - Pawn Structure (doubled pawns, pawn islands)
 - King Safety (semi-open files, heavy piece proximity)

PSTs were adapted from Stockfish's legacy HCE evaluation.

## Performance Characteristics

Search throughput: 9 million legal moves simulated per second
Branching factor: 1.4x to 2.2x (more nodes per ply searched)
The engine's elo is estimated (calibrated after tournament with 5 different engines * 24 game pairs) be ~2650

