---
post_title: "F256 Switcharoo Requirements Specification"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-requirements"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Game Design", "Requirements"]
ai_note: "Updated via code audit of the October 2025 C implementation."
summary: "Requirements aligned with the current Switcharoo C runtime and identified gaps."
post_date: 2025-10-19
---

## Overview
This revision reflects the behaviour observed in the C sources under `src/` on
2025-10-19. Statements below capture what the program presently delivers on the
Foenix F256K2 and in the host harness while highlighting missing functionality
in a dedicated discrepancies section.

## Stakeholders
- Human player using keyboard and/or mouse input.
- AI opponent operating on-device with deterministic heuristics.
- Game developers integrating with Foenix hardware primitives.
- Future maintainers extending rules, assets, or heuristics.

## Functional Requirements

### Board and Swap Rules
- WHEN the title screen transitions to gameplay or the Reset icon is triggered
  while free play is active, THE SYSTEM SHALL call `board_set_starting_layout`
  with the current `layout_id` and populate the 8x4 grid with the selected
  starting arrangement.
- WHEN puzzle mode is entered or a puzzle reset occurs, THE SYSTEM SHALL fetch
  the active puzzle via `get_puzzle_by_index`, call `apply_puzzle_position`,
  reset move history, and align the AI configuration with the puzzle's swap
  rule.
- WHEN a player completes an empty-cell move, THE SYSTEM SHALL relocate the
  moving piece, clear the origin cell, and clear swapped status according to
  the active swap rule (Classic clears all, Clears Own restores the mover's
  swapped pieces, Swapped Clears clears all only if the mover was swapped, and
  Swapped Clears Own restores the mover's swapped pieces when that mover was
  swapped).
- WHEN a swap move succeeds, THE SYSTEM SHALL exchange the two endpoints and
  mark both pieces as swapped so subsequent swap attempts recognise their
  protected state.
- WHEN `board_execute_move` accepts a move, THE SYSTEM SHALL shift the move
  history array so the newest entry resides at index zero and maintain at most
  `MAX_MOVE_HISTORY` entries.
- WHEN `board_check_win` detects a continuous chain for a player, THE SYSTEM
  SHALL populate `win_path` with one cell per row and set `has_path` true so
  rendering can colour the winning path.

### Turn and Phase Flow
- WHEN `game_state_init` completes, THE SYSTEM SHALL initialise board,
  preferences, menu, and selection state, set phase to `GAME_PHASE_TITLE`, and
  set `board.current_player` to `PLAYER_WHITE`.
- WHEN `game_state_start_new_game` runs, THE SYSTEM SHALL force free play mode,
  clear highlights, and set phase to `GAME_PHASE_PLAYING`.
- WHEN `game_state_execute_selected_move` applies a legal move and no win is
  detected, THE SYSTEM SHALL call `board_switch_turn`, update menu enables, and
  set phase to `GAME_PHASE_AI_THINKING` whenever the next player is
  `PLAYER_BLACK`.
- WHEN `game_state_update` processes `GAME_PHASE_AI_THINKING` and
  `ai_agent_find_best_move` returns a move, THE SYSTEM SHALL execute it after at
  least 30 frames, evaluate victory, switch to the opposing player, and restore
  `GAME_PHASE_PLAYING`; if no move is found, the system prints "AI HAS NO
  MOVES" and toggles turn.
- WHEN the AI is performing move search, THE SYSTEM SHALL periodically invoke
  a registered progress callback with current search depth and node count to
  enable UI progress updates.
- WHILE the AI evaluates immediate-win opportunities during an active search,
  THE SYSTEM SHALL reuse the current search context to emit throttled
  progress callbacks so interface animations remain responsive.
- WHEN the AI search progress callback is invoked, THE SYSTEM SHALL refresh the
  "AI Agent Thinking" text with a cycling dot indicator so players see active
  progress.
- WHEN the Exit icon is activated, THE SYSTEM SHALL set phase to
  `GAME_PHASE_EXIT` so `main.c` can drive the Foenix soft reset sequence.

### Input and Selection
- WHEN `input_translate_event` receives mouse delta data, THE SYSTEM SHALL
  scale the 640x480 hardware coordinates into the 320x240 playfield and emit
  `INPUT_EVENT_MOUSE_MOVE` or button transitions while keeping `keyboard_mode`
  false.
- WHEN the left mouse button clicks a player-controlled piece with at least one
  legal move, THE SYSTEM SHALL set `selection.has_selection` true and cache up
  to eight legal moves via `board_get_legal_moves`.
- WHEN the left mouse button clicks a cached legal destination, THE SYSTEM
  SHALL execute the indexed move through `game_state_execute_selected_move` and
  clear the selection.
- WHEN the selected piece is clicked again or Escape is pressed while a
  selection is active, THE SYSTEM SHALL call `game_state_deselect_piece` and
  hide highlight sprites.
- WHEN arrow keys are pressed, THE SYSTEM SHALL update the focused board cell
  via `input_handler_move_focus`, turning on `keyboard_mode`.
- WHEN Enter or Space is pressed during `keyboard_mode`, THE SYSTEM SHALL
  either execute the focused legal move or select the focused piece if no legal
  move is targeted.

### Menu Actions
- WHEN the Game Mode icon or M key toggles, THE SYSTEM SHALL flip
  `is_puzzle_mode`, apply either the active puzzle or starting layout, reset
  highlights, refresh puzzle text, and set phase to `GAME_PHASE_PLAYING`.
- WHEN the Reset icon or R key is activated, THE SYSTEM SHALL reload the
  current mode's layout or puzzle, clear hints, and resume
  `GAME_PHASE_PLAYING` without toggling modes.
- WHEN the Next or Previous icon (or N/P keys) is activated in puzzle mode, THE
  SYSTEM SHALL wrap `current_puzzle_index` within the catalog count, apply the
  puzzle, and refresh puzzle metadata; in free play the same actions cycle
  `board.layout_id` through the four predefined layouts.
- WHEN the Swap icon or S key is activated, THE SYSTEM SHALL rotate
  `user_preferences.swap_rule`, reinitialise the AI configuration, and clear the
  swap lockout message only if free play is active and `move_count` is zero;
  otherwise it prints the swap unavailable notice.
- WHEN the Difficulty icon or D key cycles, THE SYSTEM SHALL increment
  `difficulty_level` modulo four, update `ai_config.difficulty`, and update the
  HUD before the AI next acts.
- WHEN the Hint icon or H key is activated, THE SYSTEM SHALL display the first
  solution move if in puzzle mode with zero moves or run
  `ai_agent_find_best_move` as `PLAYER_WHITE` and call `print_AI_hint`
  otherwise.

### HUD and Text Output
- WHEN `text_display_init` runs, THE SYSTEM SHALL configure text callbacks for
  the widget toolkit and clear baseline HUD rows.
- WHEN `game_state_apply_current_puzzle` succeeds, THE SYSTEM SHALL call
  `print_puzzle_info` and `clear_puzzle_hint` so the HUD reflects the selected
  puzzle.
- WHEN move history changes, THE SYSTEM SHALL call `print_move_history` and
  `print_current_player` from the main loop to keep the HUD aligned with board
  history.
- WHEN `game_state_check_win` reports a winner, THE SYSTEM SHALL call
  `print_game_winner` and `print_win_loss` to update scoreboard text and
  session totals.
- WHEN the AI provides a hint or puzzle solution, THE SYSTEM SHALL render the
  suggestion via `print_AI_hint` or `display_puzzle_solution` and allow clearing
  through `clear_puzzle_hint`.

### Puzzle Tracking
- WHEN `get_puzzle_collection` runs on hardware, THE SYSTEM SHALL lazy-load the
  catalog header from far memory, set the puzzle count, and avoid duplicating
  the pointer table.
- WHEN `mark_puzzle_solved` is invoked, THE SYSTEM SHALL write a value of one
  into the puzzle record's solved flag at `0x30000 + offset` and update the
  in-memory cache.
- IF a puzzle index exceeds the stored count, THEN THE SYSTEM SHALL return
  `NULL` and leave previously cached puzzle data unchanged.
- WHEN `display_puzzle_solution` is called, THE SYSTEM SHALL format the first
  solution move into 25 characters, append swap markers when applicable, and
  print it to the hint row.

### Artificial Intelligence
- WHEN the AI takes its turn, THE SYSTEM SHALL copy the root board into a
  scratch buffer, compute goal-row pressure via `ai_goal_row_pressure`, and
  derive dynamic depth and node caps using `ai_select_dynamic_depth` and
  `ai_select_node_cap`.
- IF every legal reply still allows the opponent an immediate win on their
  next turn, THEN THE SYSTEM SHALL bypass deep search and select a move using
  the shallow heuristic pathway to minimise unnecessary computation.
- WHERE blunder mode is configured, THE SYSTEM SHALL store a percentage
  probability that is evaluated once per AI turn.
- WHEN blunder mode is enabled for Learning or Easy difficulty and the
  blunder probability triggers, THE SYSTEM SHALL choose a legal move that
  allows the opponent to win on their following turn.
- WHEN blunder mode is enabled for Standard difficulty and the blunder
  probability triggers, THE SYSTEM SHALL choose a legal move that allows the
  opponent to establish a forcing move on their following turn.
- WHEN Expert difficulty is active, THE SYSTEM SHALL ignore blunder
  configuration and select moves using the normal search pipeline.
- WHEN blunder mode is disabled or the probability check does not trigger,
  THE SYSTEM SHALL select moves using the normal heuristics/search pipeline.
- WHEN dynamic depth resolves to zero, THE SYSTEM SHALL choose a move via
  `ai_select_move_heuristic`, recording node counts when requested.
- WHEN dynamic depth is positive, THE SYSTEM SHALL run `ai_negamax` with
  alpha-beta pruning, optional iterative deepening, and transposition caching
  gated by pressure, halting when `node_limit` or timer thresholds request
  abort.
- WHEN a move is evaluated, THE SYSTEM SHALL record killer moves and prefer
  immediate wins, blocks, swap-preserving options, and forcing moves inside
  `ai_generate_moves` and `ai_select_move_heuristic`.
- WHEN `diagnostics_enabled` is true, THE SYSTEM SHALL call
  `ai_print_diagnostics` with node and tick counts and compute an evaluation
  breakdown for the chosen move via `ai_agent_evaluate_internal`.
- WHEN `ai_agent_hint_trace_enable` is true, THE SYSTEM SHALL capture up to
  sixty-four evaluation records per search and expose them through
  `ai_agent_hint_trace_get` for host tooling.
- WHEN the AI difficulty experiment harness runs, THE SYSTEM SHALL initialise
  kStartingLayout2 for each game, alternate Easy and Expert colours on
  successive trials, and report wins, losses, draws, and average half-moves
  per difficulty.
- WHEN the experiment harness is configured with an epsilon-random Expert
  opponent, THE SYSTEM SHALL inject the requested random-move percentage into
  that opponent's turn selection while keeping the deterministic competitor
  behaviour unchanged.
- WHEN the experiment harness starts, THE SYSTEM SHALL seed its deterministic
  pseudo-random generator from the provided seed value so identical
  configurations yield reproducible summaries.
- WHEN the AI difficulty is Learning or Easy, THE SYSTEM SHALL evaluate the
  top-ranked legal moves and, with a configured probability, select from the
  highest-ranking subset instead of always choosing the single best move.
- WHEN the AI difficulty is Standard, THE SYSTEM SHALL apply a low-probability
  random choice among the top-ranked legal moves to introduce rare but
  plausible deviations.
- WHEN the AI difficulty is Expert, THE SYSTEM SHALL always select the
  highest-ranked legal move as determined by the search without applying any
  randomisation.

### Diagnostics and Messaging
- WHEN the player attempts to toggle the swap rule in puzzle mode or after
  moving, THE SYSTEM SHALL call `print_swap_unavailable` and leave the swap
  configuration unchanged.
- WHEN `ai_agent_find_best_move` fails to locate a move during AI thinking, THE
  SYSTEM SHALL call `print_formatted_text` at row 21 with "AI HAS NO MOVES"
  before handing the turn back.
- WHEN puzzle loading fails, THE SYSTEM SHALL print "Puzzle load failed" and
  leave the board in the free play layout.
- WHEN `mark_puzzle_solved` succeeds, THE SYSTEM SHALL refresh puzzle info text
  so the HUD reflects the updated solved status.

## Non-Functional Requirements
- THE SYSTEM SHALL wait for the raster to reach the VBLANK window before
  calling `render_update` to avoid tearing on VICKY hardware.
- THE SYSTEM SHALL run AI search and evaluation code from far-memory overlays,
  swapping MMU bank `0x000D` to block 8 or 9 as required and restoring the
  previous mapping afterwards.
- THE SYSTEM SHALL stream puzzle catalog bytes directly from `0x30000` without
  allocating additional buffers beyond the static caches to stay within the
  low-memory budget.
- THE SYSTEM SHALL maintain deterministic AI behaviour across hardware and host
  builds by guarding hardware-specific instructions with `AI_AGENT_HOST_TEST`.

## Functional Discrepancies
- Audio cues, volume controls, and mute toggles referenced in prior drafts are
  not implemented; the input bindings remain placeholders.
- The Info overlay, settings menu, tooltips, and disabled-icon hover
  suppression are not present in the current code.
- Menu enable/disable logic does not react to move counts or empty puzzle
  catalogs; icons remain enabled regardless of state.
- The move history overlay and scoreboard strip exist only as text output;
  sprite-layer overlays are not rendered.
- Undo functionality and keyboard shortcuts for volume control remain
  unimplemented.
- Hint trace export to CSV is stubbed out in `main.c`; diagnostics are not
  persisted automatically on exit.

## Assumptions and Constraints
- THE SYSTEM SHALL operate on a fixed 8x4 board with four predefined starting
  layouts aligned to the renderer's 26-pixel cell spacing.
- THE SYSTEM SHALL centre the board within a 320x240 bitmap and render menu
  icons in a two-by-four grid to the right as assumed by
  `input_handler_hit_test`.
- THE SYSTEM SHALL treat `PLAYER_WHITE` as the human-controlled side and
  `PLAYER_BLACK` as the AI-controlled side in the shipped build.
- THE SYSTEM SHALL rely on pre-generated asset binaries residing at the VRAM
  addresses embedded via `EMBED` macros.

## Traceability Matrix

| Requirement Area | Implementation Reference | Notes |
| --- | --- | --- |
| Board setup and swap rules | `src/board.c`, `src/game_state.c` | Move validation, history, swap clearing |
| Turn and menu flow | `src/game_state.c`, `src/input_handler.c` | Phase control and icon actions |
| Rendering and HUD | `src/render.c`, `src/text_display.c`, `src/video.c` | Sprites, highlights, text output |
| AI engine and hints | `src/ai_agent.c`, `tests/ai_agent_tests.c` | Negamax search, hint tracing, host harness |
| Puzzle streaming | `src/puzzle_data.c`, `assets/generated` | Far-memory catalog access |
| System integration | `src/main.c`, `src/system.c` | Main loop, Foenix reset sequence |
