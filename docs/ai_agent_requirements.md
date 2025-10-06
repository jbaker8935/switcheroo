# Heuristic AI Agent Requirements (llvm-mos C)

## Scope

- Specify the behaviour of a heuristic (non-neural) agent that plays the row-connection board game described below.
- Target implementation language: ISO C compiled with the llvm-mos toolchain (8-bit MOS architecture constraints).
- Agent must operate standalone; no dependency on the existing C++ puzzle generator or its data structures.

## Game Rules & Starting State

- Board: 8 rows (1–8) by 4 columns (A–D).
- Piece setup:
  - Player A (White) normal pieces: A1, A2, B1, B2, C1, C2, D1, D2.
  - Player B (Black) normal pieces: A7, A8, B7, B8, C7, C8, D7, D8.
- Turn order: Player A moves first unless an external option explicitly assigns the first move to Player B.
- Movement:
  - A piece may move to any of the eight adjacent cells (orthogonal or diagonal).
  - Moving to an empty cell performs an Empty Cell Move.
  - Moving into an opponent’s **normal** piece performs a Swap Move: the pieces exchange positions, and both become **swapped**.
  - Swapped pieces cannot be targeted by future swaps but may initiate swaps against normal opponent pieces.
- Swap Rules (runtime option):
  1. **Classic:** Any empty-cell move clears all swapped pieces on the board.
  2. **Clears Own:** Empty-cell moves clear only the mover’s swapped pieces.
  3. **Swapped Clears:** Empty-cell moves by swapped pieces clear all swapped pieces; empty moves by normal pieces clear none.
  4. **Swapped Clears Own:** Empty moves by swapped pieces clear only the mover’s swapped pieces; normal pieces never trigger clearing.
- Win condition: Form a continuous chain of the player’s pieces linking any square on row 2 to any square on row 7 (inclusive).

## Platform & Architectural Constraints

- Code written in portable C99-compatible syntax; avoid compiler extensions beyond llvm-mos support.
- Memory footprint target: ≤ 32 KB total (code + data) with configurable tables stored in ROM when possible.
- Avoid dynamic allocation; rely on static or stack-based buffers of fixed maximum size.
- Execution should run within per-move CPU budgets typical for 8-bit hardware (configurable but assume < 500 ms per move at 6 MHz).

## Functional Requirements

1. **Move Generation & Legality**
   - Enumerate all legal empty-cell and swap moves for the active player given the current swap rule.
   - Track swapped status flags per piece; ensure swap initiation/target eligibility rules are enforced.
   - Apply clearing actions immediately after move execution according to the active swap rule.

2. **State Representation**
   - Maintain a compact board encoding (e.g., 32-cell array with owner + swapped bit per entry).
   - Provide efficient routines for cloning and undoing moves to support look-ahead search.
   - Track connectivity information to row 2/row 7 for both players to accelerate win checks.

3. **Early-Game Heuristic Play**
   - Prioritise advancing central files (columns B and C) and establishing staggered formations that project toward row 7 (for Player A) or row 2 (for Player B).
   - Penalise leaving isolated front pieces without adjacent friendly support.
   - Avoid early swaps unless they establish mutually supporting swapped clusters or disrupt the opponent’s central control.

4. **Mid/Late-Game Tactical Search**
   - Use deterministic look-ahead (depth-limited minimax or negamax with alpha–beta pruning).
   - Minimum search depth: 4 plies; dynamically extend when tactical threats (potential immediate wins/losses) are detected.
   - Include forward pruning controls to keep search tractable on constrained hardware (e.g., beam search or killer-move heuristics when node budget is exceeded).
   - Detect forced wins/losses by verifying if a side can complete or block the row 2–7 connection within the remaining plies.

5. **Heuristic Evaluation Function**
   - Combine the following weighted features:
     - **Connection Progress:** Count of distinct continuous paths from owned squares toward the target row band; emphasise paths with multiple branching options.
     - **Bridge Potential:** Bonus for pairs of friendly pieces separated by one cell that can be linked in one move.
     - **Swap Pressure:** Value friendly swapped pieces that threaten to clear large clusters (rule dependent); penalise isolated swapped pieces vulnerable to clearing.
     - **Blocking Coverage:** Score for occupying or attacking key opponent channels toward their target row.
     - **Mobility:** Number and quality (forward vs backward/sideways) of legal moves.
   - Normalise evaluation to a signed 16-bit range suitable for 8-bit arithmetic.
   - Provide rule-specific weight tables (Classic vs Clears Own vs Swapped Clears variants).

6. **Move Ordering & Search Enhancements**
   - Order moves using heuristics: winning moves first, swaps that create double threats, central advances, defensive blocks, then remaining moves.
   - Implement iterative deepening and principal variation reuse when time permits.
   - Maintain a small transposition cache (optional, e.g., 64 entries) using Zobrist hashing adapted to 8-bit constraints.

7. **Fallback Behaviour**
   - If search budget expires before completing the intended depth, return the best move from the deepest fully evaluated ply.
   - Guarantee a legal move is always produced, even under severe time pressure (e.g., choose the highest heuristic move at depth 1).

## Configuration & Parameters

- Tuneable settings (compile-time or small configuration block):
  - Default search depth and maximum extension depth.
  - Node or time budget per move (in CPU cycles or milliseconds).
  - Weights for evaluation features per swap rule.
  - Toggle for experimental features (e.g., transposition cache).
- Provide sane defaults allowing the agent to run without external configuration.

## Non-Functional Requirements

- **Determinism:** Identical inputs and configuration must yield identical move choices (no random tie-breaking).
- **Performance:** On representative llvm-mos hardware, achieve at least 20k evaluated positions per second at depth 4 in Classic mode.
- **Memory Safety:** No buffer overflows; implement bounds checks for all array accesses.
- **Portability:** Code must compile with the llvm-mos `clang` toolchain and a reference desktop C compiler (for testing).
- **Maintainability:** Functions should remain under 200 lines with clear modular decomposition (move generation, evaluation, search, configuration).

## Testing & Validation

- Unit tests (runnable on host platform) covering:
  - Move generation for all swap rules, including edge cases with swapped and normal pieces.
  - Evaluation outputs for handcrafted positions (opening balance, imminent win, forced block).
  - Search correctness on small tactical puzzles (verify agent finds the known winning/defensive move within budget).
- Integration tests:
  - Full-game simulations from the starting position for each swap rule, ensuring no illegal moves and verifying the agent does not lose to trivial tactics within the tested depth.
  - Regression suite to ensure future heuristic tweaks maintain or improve win rates against baseline scripts.
- Provide debugging hooks that can emit move traces and evaluation breakdowns when compiled in diagnostic mode.

## Deliverables

- `src/ai_agent.c`: Core implementation (board model, heuristics, search).
- `include/ai_agent.h`: Public API exposing move selection and configuration structures.
- `tests/ai_agent_tests.c`: Host-side test harness runnable under standard C compiler.
- `docs/heuristic_tuning.md`: Guidance on adjusting weights per rule and interpreting diagnostics.
- Build scripts or makefile snippets illustrating compilation with llvm-mos (`clang --target=mos`).
