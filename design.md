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
6. Exit conditions transition the phase to `GAME_PHASE_EXIT`, display the exit
  screen via `video_show_exit`, arm a three-second Timer0 alarm, poll kernel
  events to allow a key press to skip the delay, and after the pause (or key)
  persist state before running the Foenix soft-reset sequence.

## Module Design

### Application Shell (`main.c`, `system.c`)
- Owns the lifecycle: platform setup, game loop, periodic HUD refresh, and
  termination via `platform_soft_reset`.
- Feeds the input handler, renders HUD text, and coordinates optional hint
  trace dumps when diagnostics are enabled.
- `system.c` currently provides lightweight stubs that call into `f256lib`
  primitives; future ports can extend this layer.

### Timer and Alarm Services (`timer.c`, `timer.h`)
- Backs Timer0 configuration and exposes helper APIs so high-level modules can
  start, stop, and query alarm channels without touching hardware registers.
- Manages a fixed set of alarm channels that share the 30 Hz Timer0 tick while
  preserving independent countdown values for splash, puzzle, audio, and other
  subsystems.
- Services Timer0 pending interrupts once per tick, decrementing each active
  channel and leaving completed channels at zero until the caller re-arms the
  slot.
- Clears expiration state when a channel is configured to avoid stale alarm
  signals while maintaining the remaining time on other active channels.
- Relies on the main loop to call `timer_service` every frame so alarms advance
  even when no caller polls them explicitly.

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
- Clears the swap-unavailable HUD notice whenever a free play move completes so
  the message does not linger after play resumes.
- Provides the menu activation entry point so icons and keyboard shortcuts can
  trigger resets, puzzle navigation, hints, or difficulty changes.
- Sets AI difficulty to STANDARD by default when entering puzzle mode to provide
  an appropriate challenge level for puzzle solving.
- Preserves any manually selected puzzle difficulty across resets and puzzle
  navigation so subsequent AI turns reuse the lightweight or advanced profile
  the player chose without reinitialisation churn.
- Provides a `game_state_reset_move_history` helper so puzzle loads, free play
  resets, and layout navigation clear move history buffers and last-move
  metadata before HUD updates run.
- Routes audio cues through a `game_state_play_sound` helper so board resets,
  move execution, and win/loss outcomes trigger the appropriate `sound_id_t`
  when audio is enabled.

### Free Play History (`freeplay_history.c/.h`)
- Owns a four-entry ring buffer of `board_t` snapshots plus metadata capturing
  the current length, the viewed index, and whether achievements remain
  eligible.
- Exposes `freeplay_history_reset`, `freeplay_history_capture_live`,
  `freeplay_history_prepare_branch`, `freeplay_history_step_backward`, and
  `freeplay_history_step_forward` so `game_state` can drive snapshots without
  duplicating bookkeeping logic.
- Serialises snapshots as raw `board_t` copies and the associated
  `board_context_t` turn, last-move owner, and layout id to guarantee that
  navigation restores render and AI state consistently.
- Relies on the caller to guard puzzle mode so the module stays focused on
  buffer management for free play sessions.
- Depends on `game_state` to filter snapshot captures so only
  human-to-move positions enter the ring, preventing AI-turn states from
  appearing in navigation.
- During branching, the `game_state` module compares snapshot move counts
  to trim just the undone moves from `board_context.history`, keeping the
  move list aligned without duplicate player entries.
- Defers memory reuse until after a branch replaces future snapshots,
  ensuring that stale states are overwritten before being considered for
  achievements.
- Integrates with achievements by toggling a disqualification flag on
  branching moves and clearing the flag during resets, layout changes, or mode
  transitions.
- Records the viewed index so `text_display` can highlight the active entry
  without re-deriving it from `board_context.history` order.
- Uses the viewed board's move count to map the HUD caret to the move that
  produced the snapshot, falling back to the Start entry when the baseline
  state is active.
- Reapplies recorded winning-path highlights when navigation returns to a
  victory snapshot, invoking `refresh_win_path` without replaying audio cues.

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
- `print_move_history` reads the viewed index from the history
  module to add a caret marker to the active entry, append a "Start" row after
  recorded moves while the baseline snapshot remains, and keep puzzle rows
  untouched.

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
- On hardware builds, attempts to stream an external `puzzle_data.bin` into the
  far-memory catalog during file I/O initialisation so newly authored catalogs
  override the embedded asset without requiring a rebuild.
- Tracks an 8-byte signature embedded at the start of the catalog header and
  exposes it for persistence so clients can detect when the active puzzle set
  changes.

### Achievements (`achievements.c/.h`)
- Maintains achievement progress in a struct-of-arrays layout that packs unlock
  flags into a bitmask and stores per-achievement counters and detail bitsets
  as contiguous arrays sized for 6502-era memory budgets.
- Snapshots puzzle catalog totals per swap rule at startup by iterating the
  far-memory catalog, caching solved counts so rule-wide and catalog-wide
  completion can be detected without rescanning.
- Performs the catalog refresh during `game_state_init` and preserves the
  cached totals throughout the session so presentation layers can rely on the
  denominators without guarding against mid-session resets.
- Backfills per-rule and catalog totals during deserialisation when legacy save
  data lacks those values, recomputing counts from the active catalog without
  discarding recorded solve progress.
- Exposes update hooks for freeplay wins, puzzle hints, puzzle loads, and
  puzzle completions; each hook updates the SoA counters and conditionally sets
  unlock bits when thresholds are met.
- Guards the puzzle completion hook with the attempt-active flag so the main
  loop can remain in the game-over state without double counting puzzle
  progress across frames.
- Allows repeat puzzle clears to re-evaluate Win-in-3 and Win-in-4 achievements
  when the player finishes within the move budget without hints while guarding
  the unique solved counters against double counting.
- Drives the thirty-second puzzle benchmark by starting Timer0 for 900 ticks on
  puzzle load and consuming the shared timer pulses through a per-frame tick
  handler invoked from the main loop.
- Provides compact (versioned) serialisation routines that pack the SoA state
  into a byte buffer for persistence and repopulate the state on load without
  heap allocation.

### File I/O (`file_io.c/.h`)
- Provides load and save functionality for persistent game state using the local file "f256_switch.dat".
- On application startup, checks for file existence and loads puzzle solve status and achievement progress if present.
- On application exit, serializes current puzzle solve status and achievement progress to the file, overwriting any existing data.
- Uses binary format with puzzle data followed by achievement data for compact storage.
- Implements platform-specific I/O: stdio for host testing, no-op stubs for F256 hardware.
- Handles I/O errors gracefully, continuing execution with default state on load failures and skipping save on write failures.
- Integrates with main application lifecycle through init and shutdown hooks.
- Persists the puzzle catalog signature alongside solve bits and achievements
  and clears puzzle progress when the stored signature no longer matches the
  catalog loaded at startup.

### Artificial Intelligence (`ai_agent.c/.h`)
- Provides deterministic heuristic move ranking that evaluates goal-row
  progress, swap pressure, blocking coverage, mobility, and piece development
  while tracking immediate and forced-win signals.
- Implements a development/occupancy heuristic derived from analysis of 100
  Win-in-2 puzzle positions (from White's perspective):
  - **Row distribution targets** (based on puzzle analysis):
    - Row 1 (absolute back): avg 0.27 pieces (73% have 0, target: empty)
    - Row 2 (second rank): avg 1.57 pieces (1-2 is acceptable)
    - Rows 3-8 (advanced): avg 6.16 pieces (86% have 6+, target: 6+)
  - Penalizes pieces on absolute back rank (-60 per piece, extra -80 if 2+)
  - Mild penalty for 3+ pieces on second rank
  - Rewards pieces on victory/advanced rows (+20 per piece, +40 bonus at 6+)
  - Move ordering strongly favors leaving absolute back rank (+800) over
    second rank (+300) - reflecting that absolute back should be empty
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
- Tags immediate-loss candidates during evaluation so the normal selector
  refuses them while the blunder pipeline can still purposefully choose them
  when the configured probability hits.
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
