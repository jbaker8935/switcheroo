---
post_title: "F256 Switcharoo Architecture and Design"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-design"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Game Design", "Software Architecture"]
ai_note: "Drafted with AI assistance based on provided materials."
summary: "Comprehensive architecture and design blueprint for the F256 Switcharoo game."
post_date: 2025-09-27
---

## Overview

This document translates the requirements for F256 Switcharoo into a concrete
architecture targeted at the Foenix F256K2 platform. Design choices reference
hardware capabilities in `f256jr_ref.pdf` and the LLVM-MOS `f256lib.pdf`
software stack. The goal is to balance responsive game play, readable code, and
hardware efficiency.

## Goals and Constraints

- Deliver a deterministic turn-based experience with immediate visual feedback.
- Maintain portability across Foenix F256 variants supported by `f256lib` video
  and input abstractions.
- Keep peak memory usage within 48 KB for code and assets to fit within banked
  RAM constraints described in the platform reference.
- Sustain 30+ FPS rendering, even while AI evaluation is in progress.

## High-Level Architecture

| Layer | Responsibilities | Key Dependencies |
| --- | --- | --- |
| Application Shell | Initialization, main loop orchestration, session flow | `f256lib` runtime, hardware reset vectors |
| Game Domain | Board model, move validation, scoring, history tracking | Domain structs, rule engine |
| AI Engine | Move generation, heuristic scoring, difficulty scaling | Transposition table, evaluation heuristics |
| Presentation | Rendering, sprites, overlays, audio cues | VICKY II registers (bitmap/sprite), font assets |
| Input | Mouse, keyboard, menu navigation | `f256lib` input drivers |
| Support Services | Asset loading, memory banks, logging | `f256lib` filesystem, platform timers |

## Execution Flow

1. Boot strap via `crt0` entry and perform zero-page setup using LLVM-MOS
   defaults.
2. Initialize `f256lib` subsystems: video (bitmap layer 2, sprite layers 0-2),
   input (mouse, keyboard), audio (tone generator for feedback with preloaded
   cue envelopes), and timers. Trigger the startup cue once initialization
   completes.
3. Load assets from ROM or cartridge: board bitmap tiles, 24x24 piece sprites,
   16x16 menu icons, font glyphs.
4. Present title screen and wait for user to start the session.
5. Enter main loop with fixed-timestep updates (e.g., 60 Hz tick) separated from
   render calls to keep audiovisual consistency.
6. On each tick: process input, update selection/hover state, run AI when
   applicable, mutate game state, enqueue render commands, and flush frame.
7. Detect win/exit conditions, play the appropriate victory or defeat jingle,
  lock in the winning-path highlight (one cell per row, rows 2-7, using CLUT color changes) until the next reset, update the session
  scoreboard bitmap, and transition overlays as needed.

## Video Subsystem

- The `video_init` routine in `platform/video.c` configures a double-buffered
  320x240 bitmap pipeline. Bitmap page 0 is enabled as the primary front buffer
  while page 1 is allocated as an optional back buffer that remains hidden
  until the renderer is ready to swap.
- Initialization resets the VICKY palette tables via `graphicsReset`, clears
  all bitmap pages, and hides every sprite using `spriteReset` to guarantee a
  deterministic starting state.
- A curated CLUT is programmed through `video_apply_palette`, providing
  baseline colors for the board light/dark squares, UI panel, highlights, and
  typography. Slot 0 remains transparent-only so sprites and overlays can rely
  on hardware transparency, while background, border, UI, and text colors
  occupy slots 1-4 and board cells consume slots 5-36. Predefined palettes
  exist for three UI themes
  (`VIDEO_THEME_DEFAULT`, `VIDEO_THEME_HIGH_CONTRAST`,
  `VIDEO_THEME_COLORBLIND`), and the video subsystem exposes
  `video_apply_theme` so the menu configuration can switch palettes without
  touching individual color entries.
- `video_load_assets` copies the generated asset manifest and stages the board
  bitmap that was precomputed by `scripts/generate_assets.py`. The bitmap is
  embedded via the `EMBED` macro in a dedicated overlay region, avoiding large
  RAM buffers while still honoring runtime palette swaps.
- Asset ingestion uploads placeholder sprites, highlight frames, menu icons,
  and the embedded board bitmap into TinyVICKY VRAM during
  initialization so real hardware immediately renders populated bitmap and
  sprite layers. Sprite memory is carved out of far RAM, assets are copied via
  the MMU swap window, and `spriteDefine` preconfigures IDs for board pieces,
  menu icons, and highlight overlays with default off-screen placements.

## Game State Model

### Core Structures

```
struct Cell {
  Piece piece;   // enum { None, WhiteNormal, WhiteSwapped, BlackNormal, BlackSwapped }
};

struct Board {
  Cell cells[8][4];
  uint8_t turn;              // enum { White, Black }
  uint8_t move_count;        // Tracks turns for UI enable/disable logic
};

struct SessionStats {
  uint8_t white_wins;
  uint8_t black_wins;
};

struct UserPreferences {
  uint8_t difficulty_level;     // 0=Learning, 1=Easy, 2=Standard, 3=Expert
  uint8_t color_scheme;         // 0=Default, 1=Colorblind1, 2=Colorblind2
  bool ai_explanations_enabled;
  bool audio_enabled;
  uint8_t volume_level;         // 0-10
};

struct MenuState {
  bool reset_enabled;
  bool info_enabled;
  bool difficulty_enabled;
  bool starting_board_enabled;
  bool history_enabled;
  bool exit_enabled;
  bool score_enabled;
  bool undo_enabled;
};

struct Move {
  uint8_t from_row;
  uint8_t from_col;
  uint8_t to_row;
  uint8_t to_col;
  MoveType type; // enum { Empty, Swap };
};

Session-level context pairs `SessionStats` with `MenuState` to track score
totals, icon enablement, and to drive the scoreboard overlay renderer.
```

Supporting tables encode adjacency and winning path checks using bit masks to
minimize runtime branching.

### State Machines

- **Selection FSM:** Idle → Hover → PieceSelected → DestinationSelected →
  Resolving → Idle.
- **Menu Enable FSM:** Startup → AwaitFirstMove → PostMove (disables starting board, enables history) → GameOver → Reset.
- **Overlay FSM:** Hidden → TransitionIn → Visible → TransitionOut → Hidden.
- **AI FSM:** Idle → Thinking(with indicator) → Evaluating → MoveSelected → Executing → Idle.
- **Error Recovery FSM:** Normal → ErrorDetected → AttemptRecovery → (Normal | Degraded | Critical).

## Rendering Strategy

- Use bitmap layer 2 for the board background with precomputed 28x28 cell grid
  aligned to screen center, including a 1-pixel outer border and checkerboard
  fills that match the UI palette.
- Use sprite layer 1 for white pieces and sprite layer 1 for black pieces, with
  dedicated sprite indices for swapped variants so their artwork differs from
  normal pieces; highlight overlays occupy sprite layer 0.
- Winning path highlights draw color-coded CLUT changes: one color for the winning
  player's path, another for the opponent, and logic to composite both if a
  simultaneous win occurs. Only one cell per row (rows 2-7) is highlighted.
- Menu icons appear on the right margin using dedicated sprite indices in layer
  1, spaced vertically with 8-pixel padding. Disabled icons swap to desaturated
  frames and suppress hover sprites.
- Text output for messages and scores will use the text overlay layer.  Future design may render text to a bitmap or tile layer
- Double-buffer sprite attribute tables where possible to prevent tearing during
  AI turns.

## Input Handling

- Poll mouse position and button state each tick through `f256lib` helpers.
- Map keyboard arrow keys and Enter to board movement and selection; map Escape
  or a mouse click on the selected piece to overlay dismissal and selection
  cancel when appropriate.
- Filter pointer events when an icon is disabled so no hover or click feedback
  is produced.
- Maintain a focus ring index for keyboard navigation; update hover states for
  mouse events independently.

## Rule Engine

- Precompute legal direction deltas for 8-way adjacency.
- For each selection, evaluate candidate cells; mark `Move` structs with rule
  outcomes (empty or swap) and store the impacted piece IDs to update swapped
  flags efficiently.
- Victory detection uses Union-Find on the subset of player cells from
  rows 2-7; caches connectivity bitsets to accelerate repeated checks. Winning
  paths feed the renderer with per-player color selections and support dual-path
  highlighting when both players satisfy the condition simultaneously. Only one
  cell per row (rows 2-7) is collected for highlighting.

## Audio System

- Prepare waveform tables or tone parameters for enhanced audio experience:
  - Core cues: startup, game init/reset, select, deselect, menu confirm,
    victory, defeat, exit
  - Ambient cues: hover feedback, AI thinking pulse, error alerts
  - Volume levels: 11 discrete levels (0=mute, 1-10=audible)
- Implement audio service with priority queuing (critical > feedback > ambient)
  and overlap prevention to avoid audio clutter.
- Support keyboard volume controls (+, -, M for mute) with visual feedback
  overlay showing current volume level.
- Integrate with user preferences system to persist audio settings across
  session resets.
- Provide subtle hover sound feedback to enhance interface tactility without
  overwhelming the user experience.

## AI Design

### Architecture Overview

- Deterministic negamax core with alpha-beta pruning and aspiration windows.
- Iterative deepening driver honours difficulty profiles while guaranteeing a
  minimum four-ply search in Standard and Expert modes.
- Search context tracks node budgets, elapsed milliseconds, and principal
  variations so the engine can return the deepest completed iteration when
  resources expire.
- Optional transposition cache (64 entries) uses 32-bit Zobrist hashes to store
  exact scores, bounds, and killer moves for quick reuse on subsequent visits.

### State Representation

- Board snapshots are copied into stack-local `board_t` instances for look
  ahead; each snapshot maintains piece ownership, swapped flags, and move
  counters.
- Connectivity caches supply row reachability bitsets (`reach_top`,
  `reach_bottom`) so evaluation can reward partial chains that approach the
  goal rows.
- A three-bit move signature encodes move class (win, swap threat, central,
  fallback) for use in the history heuristic and killer lists.

### Move Generation and Ordering

- Legal move enumeration delegates to `board_get_legal_moves`, capturing the
  move type, player, and swapped status impact.
- Ordering heuristics apply the following priority tiers:
  1. Immediate wins and blocks (detected via shallow probes)
  2. Swap moves that create dual threats or clear large clusters under the
     active swap rule
  3. Central file advances (columns B and C) that progress toward the target
     row band
  4. Defensive interpositions that touch opponent frontier cells
  5. Remaining mobility options sorted by static evaluation delta
- Killer move tables retain the top two cut-inducing moves per depth to bias
  future ordering.
- Moves that would result in an immediate win for the opponent are filtered out
  during heuristic selection to prevent blunders.
- Standard and Expert difficulties add an ordering bonus for moves that force an
  immediate win on the agent's following turn so alpha-beta inspects those
  branches first.

### Evaluation Function

- Feature vector computed for each side:
  - **Connection Progress**: Weighted sum of reachability from rows 1/6 to the
    target bands using breadth-first search along eight-way adjacency.
  - **Bridge Potential**: Count of friendly pairs separated by one empty cell
    with available linking moves.
  - **Swap Pressure**: Bonus for swapped pieces threatening opponent clusters
    (rule-aware) and penalties for isolated swapped pieces.
  - **Blocking Coverage**: Coverage score for occupying opponent frontier cells
    and attacking their shortest connection paths.
  - **Mobility**: Difference in legal move count emphasising forward and
    diagonal advances into the central files.
- Immediate win detection: If the current player can achieve a winning position
  in one move, the evaluation treats the position as a win for that player,
  ensuring the AI avoids blunders that allow opponent instant wins.
  - Forcing move detection: On Standard and Expert difficulties, the evaluation
    checks if the opponent has a move that leaves the AI with no safe response
    (all AI moves lead to immediate opponent win). Such positions receive a
    heavy penalty to discourage entering forcing sequences.
- Scores normalise to signed 16-bit values using rule-specific weight tables
  stored in ROM so the engine remains 8-bit friendly.

### Difficulty Profiles

- **Learning**: Depth 1 with feature scaling at 40%, node limit 512, cache off.
- **Easy**: Depth 2, reduced mobility weight, node limit 2k, cache off.
  - **Standard**: Depth 4 minimum, node limit 8k, transposition cache on, killer
    moves enabled, forcing move probes active for both ordering and evaluation
    safeguards.
  - **Expert**: Depth 4 baseline with extension to 6 on tactical triggers,
    aspiration search, transposition cache and iterative deepening enabled,
    forcing move detection and prioritisation mirroring Standard with higher
    depth limits.

### Time and Node Management

- Configurable per-move budgets: fixed node cap plus optional millisecond
  target derived from hardware clock when available.
- The search aborts gracefully when the node or time budget is exceeded,
  returning the best move from the deepest completed iteration and setting the
  fallback flag for diagnostics.

### Search Phase Adaptation

- The AI computes a goal-band pressure score equal to the greatest number of
  rows between 2 and 7 (inclusive) occupied by either player in the current
  position.
- While pressure is at most three rows, the agent bypasses recursive search and
  selects moves using single-ply heuristic evaluation to keep response time on
  the 12 MHz 65C02 within a few hundred milliseconds.
- When pressure reaches exactly four rows, the engine performs at most a
  two-ply alpha-beta search with ordering but disables iterative deepening and
  transposition lookups to limit node counts under heavy branching.
- Once pressure is at least five rows, the engine re-enables the configured
  deep-search limits (base depth four for Standard, extensions for Expert) and
  reintroduces full win probing so endgame accuracy is preserved.
- A move-volume guard counts the legal moves for the side to move; twelve or
  more available moves clamp the search to two plies and shrink the node cap to
  preserve responsiveness, while eighteen or more moves bypass recursive search
  entirely and fall back to the single-ply heuristic selector.

### Profiling and Diagnostics

- Optional diagnostics reset hardware timer0 before search, capture the elapsed
  ticks after move selection, and render node/timer metrics on the left HUD via
  `print_formatted_text` for on-device profiling.
- Diagnostics reuse the existing breakdown hook so tuning sessions can
  correlate timing, node count, and feature contributions without recompiling
  the overlay.

### Diagnostics and Tuning

- Engine can emit per-feature contributions and search statistics when built
  with `AI_AGENT_DIAGNOSTIC` to aid in weight tuning.
- `docs/heuristic_tuning.md` captures guidance for adjusting weights per swap
  rule and interpreting diagnostics without modifying engine code.

#### AI Self-Play Tuning Harness

- Host-side unit tests compile the AI with `AI_AGENT_HOST_TEST` so MMU bank
  swaps and timer pokes collapse to no-ops, allowing the search core to run on
  desktop compilers without undefined behaviour.
- The harness drives deterministic self-play sessions between named weight
  profiles, capping the number of half-moves to keep runs fast while still
  exercising early- and mid-game decision making.
- Candidate profiles play head-to-head matches against the frozen baseline for
  every swap rule, logging deterministic win/loss/draw outcomes that surface in
  regression tests whenever aggression regresses.
- After each session the harness reports advancement metrics (frontier rows,
  cumulative progress toward the goal band) that back regression assertions
  and highlight candidates that promote aggressive play.
- Profiles are stored alongside their swap-rule metadata so future tuning can
  sweep alternative tables without touching production code.
- The extended tuning workflow runs at least 1000 self-play matches per swap
  rule from the standard layout, uses a deterministic depth-capped profile to
  finish within host test budgets, declares a draw after 200 plies, and
  aggregates advancement and frontier deltas into refined weight tables for
  follow-up regression checks.

### AI Overlay Execution

- The negamax search core, move ordering, evaluation pipeline, and diagnostics
  helpers live in an overlay section (`.ai_overlay`) that links against Foenix
  block 8. The runtime copies the overlay image from far memory into the
  reserved 0xA000 window the first time the agent is invoked.
- A lightweight guard in `ai_agent_find_best_move` ensures the overlay is
  resident before delegating to the heavy search routines, avoiding duplicate
  copies on later calls.
- Overlay transfers rely on `FAR_PEEK`/`POKE` primitives from `f256lib` to
  stream bytes from the pgZ image into the execution window without borrowing
  additional buffers.
- Host-side tests build without overlay indirection, keeping the same source
  but bypassing the copy guard so regression harnesses continue to run under
  desktop compilers.

## Menu and Overlay Implementation

- Menu icons maintain a struct of sprite IDs, bounding boxes, callback
  pointers, and an enabled flag. Hover detection uses mouse coordinates mapped
  to screen pixels and skips disabled entries.
- Post-initialization, the menu controller disables the Starting Board icon and
  enables Move History after the first move; resetting the session reverses that
  state when puzzles are present, and the icon remains disabled entirely if the
  puzzle catalog is empty.
- The menu controller and text overlay surface a "No puzzles available" status
  in the puzzle information panel whenever the catalog header reports zero
  entries so hardware users receive immediate feedback instead of a silent
  failure.
- Overlays reuse bitmap layer 1 with semi-transparent blitting (implemented via
  dithered patterns) to leave board context visible.
- Move history stores up to 40 entries in a ring buffer; render the top 12
  entries per page and toggle overlay availability with the menu state machine.
- Reset workflows attempt to reapply the currently selected puzzle via
  `game_state_apply_current_puzzle`, falling back to the default board layout
  and status messaging when no puzzle records exist, while also clearing move
  history and swapped flags and leaving the session scoreboard intact. The game
  initialization audio cue still triggers after the reset action completes.
- Exit confirmations queue the exit audio cue immediately before issuing the
  shutdown sequence.

## Puzzle Data High-Memory Pipeline

### Binary Catalog Layout

| Field | Size (bytes) | Notes |
| --- | --- | --- |
| Puzzle count | 2 | Little-endian `uint16_t` value at offset 0. |
| Identifier | 32 | ASCII string, null-padded; max 31 printable chars plus terminator. |
| Swap rule | 2 | Encoded as the `swap_rule_t` ordinal. |
| Difficulty | 1 | Difficulty value (1-4). |
| Solved flag | 1 | Reserved for future progress tracking; currently zero. |
| Piece count | 1 | Number of `[row, packed_piece]` pairs populated in the piece buffer. |
| Pieces | 32 | Sixteen packed `[row, packed_piece]` pairs (unused slots zeroed). |
| Solution length | 1 | Number of solution moves (≤9). |
| Solution words | 36 | Eighteen `uint16_t` entries storing pairs of `[player, packed_move]`. |

Each puzzle record occupies 106 bytes. Records follow back-to-back after the
2-byte puzzle count header, allowing constant-time seeks via `index * 106`.

### Generation Flow

1. `scripts/convert_puzzles.py` loads one or more JSON puzzle manifests, packs
   the normalized data into fixed-width buffers, and writes
   `assets/generated/puzzle_data.bin` using the layout above.
2. The script enforces identifier, piece-count, and solution-length limits
   before emitting the binary. Violations raise errors during conversion.
3. A summary of the converted puzzle count is printed for build logs.

### Runtime Deserialization

- `src/puzzle_data.c` is now a hand-maintained module that embeds the binary
  catalog at address `0x30000` using `EMBED`. Runtime access uses
  `FAR_PEEK`/`FAR_PEEKW` to stream data directly from far memory.
- `src/platform_f256.h` centralises Foenix-specific overrides, pinning the
  far-memory swap slot to bank 5 (0xA000 window) so catalog reads do not
  clash with the firmware-reserved bank 7 used by the microkernel.
- Low-memory buffers sized for a single puzzle (`32` bytes for the identifier
  and pieces, `36` bytes for the solution words) receive the streamed data.
  `get_puzzle_by_index` populates these buffers and returns a stable pointer to
  the static `puzzle_t` struct.
- `get_puzzle_collection` lazily reads the catalog header to expose the puzzle
  count without materialising pointer arrays. The legacy `puzzles` pointer is
  left `NULL` for compatibility with existing callers that only inspect
  `count`.
- `puzzle_catalog_ensure_header` and `puzzle_catalog_load_record` emit
  diagnostics via `print_puzzle_debug`, reporting the header count, embed base
  address, loaded record index, identifier, and out-of-range errors directly in
  the on-screen text panel for hardware troubleshooting.
- `game_state_apply_current_puzzle` orchestrates board resets and new-session
  initialisation by streaming the selected puzzle, clearing swapped flags,
  synchronising swap rules and AI configuration, and updating puzzle info text;
  if the catalog is empty the helper restores the default layout and prints the
  "No puzzles available" status message.
- Host builds that lack `EMBED` support load the binary from disk on demand
  and reuse the same deserialisation path, preserving functional parity across
  testing environments.

## Asset Pipeline

- Artwork prepared as indexed PNG files converted to Foenix-compatible sprite
  and bitmap data via `f256lib` asset tools. During early development a
  deterministic Python script (`scripts/generate_assets.py`) produces
  placeholder assets into `assets/generated/`, including the 320x240 board
  bitmap, 24x24 piece sprites, highlight sprites, and 16x16 menu icon
  sprites. The script encodes palette indexes aligned with the active theme so
  visual smoke tests exercise the real CLUT layout.  Assets are loaded using the llvm-mos EMBED function.
- Font glyphs derived from an 8x8 monospace set for legibility at low
  resolution.
- Build process uses the llvm-mos build script: llvm-mos/f256dev/f256build.sh

## Build and Packaging

- Build is done with the following command:
llvm-mos/f256dev/f256build.sh ../f256_switch
- The primary build target is a `.pgz` image produced by `mos-f256-clang` with
  the local linker script. The build also emits the companion ELF binary, map
  file, raw binary dump, symbol table, and annotated disassembly for debugging.

## Memory Layout

- Zero page reserved for hot data: current selection, hover indices, AI timers.
- Main RAM bank maps board state, move buffers, menu enable flags, session
  scoreboard, and AI workspace (~8 KB).
- Overlay workspace at 0xA000 stores the copied AI search code. The loader
  asserts that `.data + .bss` finish below 0xA000 so the overlay never tramples
  persistent globals or the software stack.
- Sprite attribute memory configured per `f256jr_ref.pdf` table 5-2; double
  buffers allocate contiguous 1 KB blocks.
- Audio cue tables reside in high RAM or ROM banks with pointers cached during
  initialization for low-latency playback.
- Code segments placed in ROM bank with function grouping: rendering, rules,
  AI, input.

## Error Handling and Logging

- Initialize a circular debug log buffer in RAM for development builds; display
  via serial console if available.
- Recover from asset load failures by presenting an error overlay with reset
  instructions, relying on `f256lib` error codes.

## Testing Strategy

- Unit tests for rule validation and AI evaluation run on host via LLVM-MOS
  cross compilation harness.
- Hardware smoke tests load onto actual F256 hardware or emulator to verify
  sprite alignment, input latency, and performance metrics.

## Future Enhancements

- Networked play over serial link using Foenix modem APIs.
- Tournament persistence saved to SD card via `f256lib` filesystem hooks.
- Accessibility options for colorblind-friendly palettes and slower animations.
