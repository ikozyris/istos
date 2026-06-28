# Chess Engine
Code under core/

In this repo there are only test programs for the engine, a standard UCI interface is avaiable on 
a separate [engine-uci-interface](https://git.svlachos.duckdns.org/spiros/engine-uci-interface) repo.

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
| 11	| 0.08 |     637,780 | 1.91	  | 8.15 |
| 12	| 0.15 |   1,181,691 | 1.85	  | 8.18 |
| 13	| 0.23 |   1,820,807 | 1.54	  | 8.17 |
| 14	| 0.45 |   3,629,571 | 1.99	  | 8.15 |
| 15	| 0.73 |   5,860,492 | 1.62	  | 8.13 |
| 16	| 1.21 |   9,746,154 | 1.66	  | 8.13 |
| 17	| 2.26 |  18,134,817 | 1.86	  | 8.12 |
| 18	| 4.51 |  36,316,828 | 2.00	  | 8.14 |
| 19	| 6.67 |  53,408,528 | 1.47	  | 8.11 |
| 20	| 10.60 |  85,070,958 | 1.59	  | 8.13 |
| 21	| 21.82 | 174,437,361 | 2.05	  | 8.11 |
| 22	| 31.94 | 256,067,677 | 1.47	  | 8.13 |
| 23	| 91.75 | 731,325,439 | 2.86	  | 8.11 |
| 24	| 135.02 | 1,077,128,525 | 1.47	  | 8.11 |
| 25	| 197.96 | 1,578,679,666 | 1.47	  | 8.11 |
| 26	| 349.11 | 2,787,792,198 | 1.77	  | 8.12 |
| 27	| 534.96 | 4,288,702,476 | 1.54	  | 8.13 |

These benchmarks are old and the engine is currently 15% faster (8.8 MN/s and 3% fewer nodes).
