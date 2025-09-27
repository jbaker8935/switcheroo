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
2. Initialize `f256lib` subsystems: video (bitmap layer 0, sprite layers 0-2),
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
  lock in the winning-path highlight until the next reset, update the session
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
  typography. These entries live in CLUT 0 slots 0-7 for easy reuse across
  bitmaps and sprites.
- Asset ingestion is staged through `video_load_assets`, which currently acts
  as a stub while artwork generation is pending; the function signature is in
  place so future asset packs can be supplied without refactoring the system
  bootstrap.

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

- Use bitmap layer 0 for the board background with precomputed 28x28 cell grid
  aligned to screen center, including a 1-pixel outer border and checkerboard
  fills that match the UI palette.
- Use sprite layer 0 for white pieces and sprite layer 1 for black pieces, with
  dedicated sprite indices for swapped variants so their artwork differs from
  normal pieces; highlight overlays occupy sprite layer 2.
- Winning path highlights draw color-coded overlays: one palette for the active
  player's path, another for the opponent, and logic to composite both if a
  simultaneous win occurs.
- Menu icons appear on the right margin using dedicated sprite indices in layer
  1, spaced vertically with 8-pixel padding. Disabled icons swap to desaturated
  frames and suppress hover sprites.
- Session score is rendered as a bitmap strip positioned beneath the last menu
  item with the format `W:## B:##`, refreshed after initialization and each win.
- Text overlays render via tile-based font on bitmap layer 1 to simplify
  scrolling move history and information screens.
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
- Victory detection uses Union-Find or DFS on the subset of player cells from
  rows 2-7; caches connectivity bitsets to accelerate repeated checks. Winning
  paths feed the renderer with per-player color selections and support dual-path
  highlighting when both players satisfy the condition simultaneously.

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

- Employ iterative deepening with depth-limited minimax and alpha-beta pruning.
- Implement four difficulty levels with distinct characteristics:
  - **Learning**: Depth 1, 15% chance of suboptimal moves, simplified heuristics
  - **Easy**: Depth 2, basic heuristics, no opening book
  - **Standard**: Depth 3 with alpha-beta pruning, full heuristics
  - **Expert**: Depth 4, opening book, transposition tables, quiescence search
- Heuristic function aggregates:
  - Row coverage between rows 2 and 7
  - Connected component bonuses weighted by size
  - Swapped piece count penalties or rewards depending on mobility
  - Opponent threat detection via lookahead for immediate wins
  - Advanced loss avoidance by flagging forced responses from opponent moves
- Maintain a small opening book (3-4 moves deep) with 5-8 good opening patterns
  to provide variety and avoid early blunders.
- Implement progressive disclosure: show "thinking" indicator after 100ms,
  display intermediate best move after 250ms if search continues.
- Store move explanations for display: "Blocked opponent", "Advanced goal",
  "Created threat", "Defensive move", etc.

## Menu and Overlay Implementation

- Menu icons maintain a struct of sprite IDs, bounding boxes, callback
  pointers, and an enabled flag. Hover detection uses mouse coordinates mapped
  to screen pixels and skips disabled entries.
- Post-initialization, the menu controller disables the Starting Board icon and
  enables Move History after the first move; resetting the session reverses that
  state.
- Overlays reuse bitmap layer 1 with semi-transparent blitting (implemented via
  dithered patterns) to leave board context visible.
- Move history stores up to 40 entries in a ring buffer; render the top 12
  entries per page and toggle overlay availability with the menu state machine.
- Reset workflows clear move history and swapped flags while leaving the
  session scoreboard intact and triggering the game initialization audio cue.
- Exit confirmations queue the exit audio cue immediately before issuing the
  shutdown sequence.

## Asset Pipeline

- Artwork prepared as indexed PNG files converted to Foenix-compatible sprite
  and bitmap data via `f256lib` asset tools. Assets include normal and swapped
  piece variants, checkerboard tiles, disabled menu frames, and score glyphs.
- Font glyphs derived from an 8x8 monospace set for legibility at low
  resolution.
- Build process integrates asset packing into the LLVM-MOS project using custom
  `Makefile` rules.

## Build and Packaging

- Maintain the Foenix-specific linker logic in-repo under
  `toolchain/linker/link.ld` so the project does not depend on the upstream
  `f256dev` directory layout. A thin wrapper `link.ld` at the repository root
  keeps compatibility with the llvm-mos driver's `-Tlink.ld` default while the
  full script inlines the zero-page, section, and overlay handling sourced from
  the Foenix SDK.
- The primary build target is a `.pgz` image produced by `mos-f256-clang` with
  the local linker script. The build also emits the companion ELF binary, map
  file, raw binary dump, symbol table, and annotated disassembly for debugging.
- Packaging steps are orchestrated through the project `Makefile`, which runs
  the Python helper `scripts/pgz_thunk.py` after each link to print segment
  metadata in the build log for quick validation.
- Toolchain executables (compiler, objdump, objcopy, nm) default to the
  versions shipped with the checked-out llvm-mos toolchain, but can be
  overridden via `toolchain.mk` when needed.
- Core `f256lib` rendering helpers (`f_graphics.c`, `f_bitmap.c`,
  `f_sprite.c`, `f_math.c`) are compiled as part of the project build so the
  generated binary remains self-contained even if the upstream SDK layout
  shifts.

## Memory Layout

- Zero page reserved for hot data: current selection, hover indices, AI timers.
- Main RAM bank maps board state, move buffers, menu enable flags, session
  scoreboard, and AI workspace (~8 KB).
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
