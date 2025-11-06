---
post_title: "F256 Switcharoo Architecture and Design"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-design"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Game Design", "Software Architecture"]
ai_note: "Drafted with AI assistance based on provided materials. Updated to reflect puzzle mode default difficulty change to Standard."
summary: "Comprehensive architecture and design blueprint for the F256 Switcharoo game."
post_date: 2025-10-30
---

F256 Switcharoo's C implementation was reviewed on 2025-10-30 to document the
current architecture on Foenix F256K2 hardware and its host-side harnesses.
This design captures the realised structure, data flows, and remaining gaps so
future work can extend the code base confidently.

## System Goals and Constraints
- Deliver deterministic human-vs-AI play with fast feedback on Foenix F256K2
  hardware.
- Keep RAM usage within the 48 KB budget outside the AI overlay window.
- Mirror behaviour between hardware and host builds for regression parity.
- Stream assets and puzzle data from far memory to preserve low-memory headroom.
- Minimise dependencies to `f256lib` primitives for portability across Foenix
  variants.

## Architectural Layers

| Layer | Responsibilities | Key Modules |
| --- | --- | --- |
| Application Shell | Process startup, event pump, shutdown/reset | `src/main.c`, `src/system.c` |
| Game State Orchestration | Phase control, menu toggles, puzzle transitions | `src/game_state.c`, `src/game_state.h` |
| Board and Rules | Board representation, move validation, swap rules, win checks | `src/board.c`, `src/board.h` |
| Artificial Intelligence | Heuristic move selection, difficulty tuning, diagnostics | `src/ai_agent.c`, `src/ai_agent.h` |
| Presentation | Sprite updates, board highlights, text HUD | `src/render.c`, `src/video.c`, `src/text_display.c` |
| Input | Mouse/keyboard event translation, focus management | `src/input.c`, `src/input_handler.c`, `src/mouse_pointer.c` |
| Puzzle Services | High-memory streaming, solve tracking, diagnostics | `src/puzzle_data.c`, `src/puzzle_data.h` |

## Lightweight Screen Management

To support extensible and maintainable screen flow (see `ScreenFlow.md`), a lightweight screen state enum and variable (`g_screen_state`) are used in `main.c` and guarded in `input_handler.c`.

- **Screen transitions** are handled in the main loop after input events, making it easy to add new screens and transition rules.
- **Input guards** in `input_handler_process_event` ensure only relevant inputs are processed for the current screen, reducing accidental input handling and improving clarity.
- **Extensibility**: To add a new screen, update the enum, add transition logic in the main loop, and add input guards in the handler.

This approach is lightweight, suitable for 6502 targets, and keeps input polling centralized in the main loop.

## Runtime Flow
1. `main()` initialises Foenix video, sprite, and text layers via `video_init`
   and primes the mouse cursor macros.
2. `game_state_init` seeds board layout, phase flags, menu enable state, AI
   configuration, and HUD text.
3. The main loop polls hardware events through `input_translate_event`, pushes
   them through `input_handler_process_event`, and updates game state via
   `game_state_update`.
4. During AI phases `ai_agent_find_best_move` evaluates moves using a dynamic
   depth strategy and optional diagnostics; results are executed by
   `game_state_execute_selected_move`.
5. Rendering runs every tick through `render_update`, projecting board state to
   sprite attributes, highlight overlays, and win-path colours while text rows
   are refreshed via `text_display` helpers.
6. Exit conditions transition the phase to `GAME_PHASE_EXIT`, prompting the
   Foenix soft-reset sequence and releasing hardware resources.

## Module Design

### Application Shell (`main.c`, `system.c`)
- Owns the lifecycle: platform setup, game loop, periodic HUD refresh, and
  termination via `platform_soft_reset`.
- Feeds the input handler, renders HUD text, and coordinates optional hint
  trace dumps when diagnostics are enabled.
- `system.c` currently provides lightweight stubs that call into `f256lib`
  primitives; future ports can extend this layer.

### Game State Orchestration (`game_state.c/.h`)
- Maintains the composite `game_state_t` containing board, menu, selection,
  AI, and puzzle context.
- Drives the finite set of phases (`TITLE`, `PLAYING`, `AI_THINKING`,
  `AI_MOVING`, `EXIT`) and transitions based on menu input, move outcomes, and
  puzzle availability.
- When switching from free play to puzzle mode, selects the first unsolved
  puzzle for the active swap-rule filter before reloading puzzle state.
- Normalises turn flow by calling `board_execute_move`, swapping turns,
  scheduling AI work, and updating menu enable flags.
- Provides the menu activation entry point so icons and keyboard shortcuts can
  trigger resets, puzzle navigation, hints, or difficulty changes.
- Sets AI difficulty to STANDARD by default when entering puzzle mode to provide
  an appropriate challenge level for puzzle solving.
- Preserves any manually selected puzzle difficulty across resets and puzzle
  navigation so subsequent AI turns reuse the lightweight or advanced profile
  the player chose without reinitialisation churn.

### Board and Move Validation (`board.c/.h`)
- Encapsulates the 8x4 board grid, piece states, and swap flags.
- Implements legal-move enumeration for empty moves and swaps per rule variant
  (Classic, Clears Own, Swapped Clears, Swapped Clears Own).
- Updates history via a fixed-size move ring buffer for live boards while
  exposing `board_execute_move_without_history` so simulations skip history
  churn.
- Checks for win conditions using per-row connectivity and populates
  `win_path_t` so the renderer can highlight the victory path.
- Executes the connectivity search with a 24-bit frontier mask so the queue
  reuse avoids frame-time `memset` churn on the 65C02.
- Precomputes per-cell bit masks for the victory span so repeated win checks
  avoid runtime shifting on the 65C816 path.
- Tracks `white_swapped_count`, `black_swapped_count`, and aggregate
  `swapped_count` so swap-aware heuristics can query the board state without
  rescanning all cells.
- Seeds win detection from row index one and targets row index six (rows two
  through seven in 1-based terms) so forced-win analysis aligns with the
  official puzzle definitions.

### Input and Focus Management (`input.c`, `input_handler.c`, `mouse_pointer.c`)
- Translates PS/2 mouse and keyboard events into internal `input_event_t`
  messages with scaled coordinates for the 320x240 playfield.
- Maintains keyboard focus state for cursor navigation when the player uses the
  arrow keys and Enter/Space to select pieces and destinations.
- Handles deselection with Escape or same-cell clicks and funnels legal moves to
  `game_state_execute_selected_move`.
- Controls the hardware cursor visibility and centering through Foenix
  register writes.

### Presentation (`render.c/.h`, `video.c`, `text_display.c/.h`)
- `render_update` diff-checks cached board state against the live board to push
  only needed sprite attribute changes for pieces, highlights, and soft cursor
  overlays.
- `video.c` embeds board and sprite assets, configures palette entries, and
  provides helpers to recolour winning paths or reset board tiles.
- `text_display.c` owns the HUD: current player, move history, difficulty,
  puzzle metadata, hints, and win banners using formatted text rows.
- Presentation currently employs static palettes and does not yet implement the
  themed or disabled-icon art envisioned in earlier drafts.

### Puzzle Streaming (`puzzle_data.c/.h`)
- Streams a fixed-record catalog (`assets/generated/puzzle_data.bin`) embedded
  at far-memory address `0x30000` using `EMBED`.
- Supplies `get_puzzle_collection`, `get_puzzle_by_index`, and
  `mark_puzzle_solved` to the game state without duplicating large buffers in
  low memory.
- Maintains per-swap-rule counts and maps filtered indices on demand so
  catalogs larger than 256 entries remain fully addressable without dedicated
  per-rule index arrays.
- Emits on-screen diagnostics when catalog loads fail or indices fall outside
  the available count so hardware debugging is straightforward.
- Applies puzzle layouts through `apply_puzzle_position`, synchronising swap
  rules and AI configuration with the puzzle metadata.

### Artificial Intelligence (`ai_agent.c/.h`)
- Provides deterministic heuristic move ranking that evaluates goal-row
  progress, swap pressure, blocking coverage, and mobility while tracking
  immediate and forced-win signals.
- Applies candidate moves on cloned boards via
  `board_execute_move_without_history` so simulations avoid polluting move
  history while cached counters stay in sync.
- Filters candidate moves so immediate wins for the opponent—including
  own-goal positions that finish an opponent chain—are rejected and, on
  Standard and Expert, forced-win concessions are avoided unless the
  difficulty's blunder rule selects them.
- Supports Standard host regressions through `AI_AGENT_HOST_TEST`, bypassing
  MMU swaps while preserving move ordering.
- Generates candidate moves via a single adjacency scan with LUT-backed ownership
  checks, eliminating repeated `board_*` helper calls and improving 65C02
  execution efficiency.
- Sorts move ordering through an indirect index array so the evaluation stage
  reuses candidate buffers without performing multi-field data shuffles.
- Annotates each candidate with cached immediate-win and forced-win flags so
  the scoring pass can reuse the earlier analysis without reissuing board
  queries.
- Skips recomputation of opponent reply wins during evaluation when move flags
  report the earlier detection results from candidate generation.
- Reuses the cached candidate set to answer forced-loss queries so immediate
  loss checks avoid rebuilding and reapplying every legal move.
- Short-circuits move generation when an immediate win is detected and reuses
  the post-move board state during evaluation so tactical checks (immediate
  and forced wins) are computed only once per candidate.
- Disables forcing checks during the subsequent scoring pass, relying on the
  earlier detection results to apply penalties without repeating expensive
  analysis.
- Counts pieces via a nibble popcount lookup table instead of shift-based
  loops, reducing per-candidate evaluation cost on the host and target builds.
- Forced-win and immediate-win helpers iterate opponent replies directly from
  the SOA enumerator so they inspect every legal move even when the count
  exceeds the heuristic buffer length.
- Forced-win detection now seeds its analysis exclusively from swap moves by
  the threatening side, reflecting the early-game tactical focus while still
  evaluating every opponent reply for those candidates.
- Captures optional move diagnostics and hint traces to aid tuning and exposes
  host-callable getters for debugging.
- Exposes helpers for inevitability analysis so host tests can flag positions
  where every reply grants the opponent an immediate win.
- Supports optional blunder behaviour controlled by configuration: Learning
  and Easy difficulties can intentionally allow an opponent immediate win,
  while Standard can allow an opponent forcing line. A per-turn percentage
  controls whether a blunder overrides the selected move, and Expert ignores
  the feature entirely.
- Supports difficulty-specific randomness by sampling from the top-ranked move
  list using a seeded LCG; Learning/Easy favour larger candidate sets with
  higher epsilon, Standard applies a small epsilon across two best moves, and
  Expert always plays the highest-ranked move.

#### Difficulty Experiment Harness (Host)
- Implemented as a standalone host executable under `tests/` that links
  directly against `board.c` and `ai_agent.c` with `AI_AGENT_HOST_TEST` stubs.
- Uses a lightweight competitor profile (difficulty, random epsilon, label) to
  drive alternating-colour self-play on kStartingLayout2 for a configurable
  number of games and half-move caps.
- Shares the deterministic linear-congruential RNG from the tuning harness so
  experiment seeds are reproducible across runs and platforms.
- Collects per-competitor win/loss/draw counts, cumulative half-moves, and
  advancement/frontier metrics to make difficulty deltas easy to compare.
- Reports random-move utilisation per competitor so epsilon baselines can be
  correlated directly with upset rates when analysing difficulty gaps.

### Diagnostics and Messaging
- Menu actions that are disallowed (swap after moving, swap in puzzle mode)
  print explicit text feedback through `text_display`.
- AI failure to find a move surfaces "AI HAS NO MOVES" before yielding the
  turn back to the player.
- Puzzle streaming and hint-generation routines log their activity to HUD rows
  when diagnostics are enabled.

## Data and Memory Layout
- Core state (`board_t`, move history, selection) resides in low memory.
- AI overlay is copied into the 0xA000 window before evaluation; host builds
  bypass the copy.
- Puzzle catalog stays in far memory; only the active puzzle identifier,
  piece buffer, and solution words occupy low-memory buffers while filtered
  lookups reuse the far-memory stream for index mapping.
- Help text is embedded as a read-only string constant in `src/help.c` for display during help screens.

## State Management
- Phase machine (`game_state.c`) governs title, playing, AI thinking, AI moving,
  and exit transitions.
- Selection state tracks the highlighted piece, cached legal moves, and whether
  keyboard focus is active.
- Menu enable flags are recomputed after every move and puzzle change, keeping
  reset and difficulty toggles in sync with mode-specific rules.

## Testing and Tooling
- `tests/ai_agent_tests.c` drives deterministic AI self-play to validate
  heuristic move ordering, blunder behaviour, and hint scoring.
- Layout-specific regression cases in `tests/ai_agent_tests.c` verify the
  Expert profile rejects `kStartingLayout2` move sequences that hand Player
  White a forced win by inspecting candidate annotations and final choices.
- Host builds expose `ai_agent_move_creates_forced_immediate_win` and
  `ai_agent_move_allows_opponent_immediate_win` so regression cases can
  enumerate immediate tactical outcomes for every legal move.
- Move generation penalises candidate moves that leave an immediate win for
  the opponent, ensuring the heuristic selector avoids the blunders surfaced
  in the kStartingLayout2 regression.
- `tests/example2_simple_test.c` validates AI hints against a known puzzle
  solution.
- `tests/hint_trace_host.c` captures and inspects hint diagnostics in host
  builds.
- Asset and puzzle pipelines rely on scripts in `scripts/` to regenerate
  embedded binaries; design assumes those scripts are run before packaging.

## Known Gaps and Future Work
- Audio system, volume controls, and hover cues remain unimplemented.
- Hover highlighting for disabled icons and full menu overlay polish are
  pending.
- Undo, tooltips, and broader accessibility options are not present.
- Hint trace export is currently manual; automated persistence at exit is
  desired.
- Error handling beyond HUD messaging (e.g., centralised recovery or logging)
  has not been built.
