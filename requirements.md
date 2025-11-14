---
post_title: "F256 Switcharoo Requirements Specification"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-requirements"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Game Design", "Requirements"]
ai_note: "Updated via code audit of the October 2025 C implementation. Updated puzzle mode default difficulty to Standard."
summary: "Requirements aligned with the current Switcharoo C runtime and identified gaps."
post_date: 2025-10-30
---

## Overview
This revision reflects the behaviour observed in the C sources under `src/` on
2025-10-30. Statements below capture what the program presently delivers on the
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
- WHEN gameplay uses `board_execute_move` to accept a move, THE SYSTEM SHALL
  shift the move history array so the newest entry resides at index zero and
  maintain at most `MAX_MOVE_HISTORY` entries.
- WHEN an AI helper applies moves to cloned boards, THE SYSTEM SHALL execute
  them via `board_execute_move_without_history` so piece state and move counts
  update while cached winning rows refresh and move history remains
  unchanged.
- WHEN win detection runs, THE SYSTEM SHALL treat board rows two through seven
  (1-based numbering) as the victory span, declaring a win once a contiguous
  chain links those rows for a single player.
- WHEN win detection constructs its traversal state, THE SYSTEM SHALL reuse
  precomputed bit masks for the victory rows so repeated invocations avoid
  per-call bit shifting on Foenix hardware.
- WHEN `board_check_win` detects a continuous chain for a player, THE SYSTEM
  SHALL populate `win_path` with one cell per row and set `has_path` true so
  rendering can colour the winning path.

### Turn and Phase Flow
- WHEN `game_state_init` completes, THE SYSTEM SHALL initialise board,
  preferences, menu, and selection state, set phase to `GAME_PHASE_TITLE`, and
  set `board.current_player` to `PLAYER_WHITE`.
- WHEN `game_state_start_new_game` runs, THE SYSTEM SHALL force puzzle mode,
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
  highlights, refresh puzzle text, set AI difficulty to STANDARD when entering
  puzzle mode, and set phase to `GAME_PHASE_PLAYING`.
- WHEN the Reset icon or R key is activated, THE SYSTEM SHALL reload the
  current mode's layout or puzzle, clear hints, reset move history and the
  last-moving-player indicator, and resume `GAME_PHASE_PLAYING` without
  toggling modes.
- WHEN the Next or Previous icon (or N/P keys) is activated in puzzle mode, THE
  SYSTEM SHALL wrap `current_puzzle_index` within the catalog count, apply the
  puzzle, and refresh puzzle metadata; in free play the same actions cycle
  `board.layout_id` through the four predefined layouts and reset move history
  before play resumes.
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
- WHEN a user manually sets the puzzle difficulty, THE SYSTEM SHALL retain that
  selection across puzzle resets and puzzle navigation without reverting to the
  catalog default.
- WHEN move history changes, THE SYSTEM SHALL call `print_move_history` and
  `print_current_player` from the main loop to keep the HUD aligned with board
  history.
- WHEN `game_state_check_win` reports a winner, THE SYSTEM SHALL call
  `print_game_winner` and `print_win_loss` to update scoreboard text and
  session totals.
- WHEN the help screen is displayed, THE SYSTEM SHALL render the embedded help text from the `help_text` constant in `src/help.c`.

### Free Play Move History
- WHEN free play accepts a move while the viewed turn is the live board and the
  resulting position leaves the human player to act, THE SYSTEM SHALL snapshot
  the resulting `board_t` into a four-entry circular history, insert the move at
  index zero, and clamp the history length to four states.
- WHEN the player presses Back (`B`) in free play and an older snapshot exists, THE
  SYSTEM SHALL step the viewed index backward by one, restore the board and
  context from that snapshot, clear selections, and keep the AI in a manual
  phase.
- WHEN the player presses Forward (`F`) in free play and a newer snapshot exists,
  THE SYSTEM SHALL step the viewed index forward by one, restore the board and
  context from that snapshot, and refresh HUD highlights without executing new
  logic.
- WHEN the player performs a legal move from a historical snapshot in free
  play, THE SYSTEM SHALL discard all snapshots that were newer than the viewed
  index, append the new state as the latest entry, and mark the session as
  ineligible for free play achievements until the history is cleared.
- WHEN free play history is cleared by reset, layout change, swap rule toggle
  before any moves, or mode transition, THE SYSTEM SHALL delete all snapshots,
  reset the viewed index to zero, and lift the achievement ineligibility flag.
- WHEN puzzle mode is active, THE SYSTEM SHALL ignore Back and Forward key
  presses for board navigation and preserve the existing puzzle controls.
- WHEN move history text is rendered in free play, THE SYSTEM SHALL identify
  the currently viewed snapshot with a caret marker or alternate color in the
  rendered column without altering puzzle displays.
- WHILE the base snapshot remains in free play history, THE SYSTEM SHALL append
  a dedicated "Start" entry after the recorded moves and highlight it when the
  base snapshot is selected.
- IF the base snapshot has aged out of free play history, THEN THE SYSTEM SHALL
  omit the "Start" entry from the move history render.
- WHEN Back or Forward is pressed without an available snapshot, THE SYSTEM
  SHALL leave the board unchanged and may play an optional error chime.
- WHEN the player navigates backward or forward to a stored free play snapshot,
  THE SYSTEM SHALL highlight in the move history the move that produced the
  viewed position, or the Start entry when the base snapshot is selected.
- WHEN free play navigation advances to a snapshot whose board already
  contains a winning path, THE SYSTEM SHALL restore the winning-path highlight
  without replaying the associated audio cue.

### Timing and Alarms
- WHEN subsystems schedule independent countdowns, THE SYSTEM SHALL provide
  separate Timer0-backed alarm channels so each countdown completes without
  interfering with the others.
- WHEN Timer0 signals a tick, THE SYSTEM SHALL decrement every active alarm
  channel exactly once per tick and leave completed channels at zero until the
  channel is rearmed.
- WHEN an alarm channel is configured, THE SYSTEM SHALL apply the requested
  tick budget to that channel while preserving the remaining time on all other
  active channels.

### Audio Feedback
- WHEN the game board is reset, THE SYSTEM SHALL play `SOUND_ID_RESET_BOARD`
  if audio is enabled.
- WHEN a legal move is executed and no immediate win or loss is detected, THE
  SYSTEM SHALL play `SOUND_ID_MOVE` if audio is enabled.
- WHEN the player wins, THE SYSTEM SHALL play `SOUND_ID_WIN` if audio is
  enabled.
- WHEN the player loses, THE SYSTEM SHALL play `SOUND_ID_LOSS` if audio is
  enabled.

### Puzzle Tracking
- WHEN `get_puzzle_collection` runs on hardware, THE SYSTEM SHALL lazy-load the
  catalog header from far memory, set the puzzle count, and avoid duplicating
  the pointer table.
- WHEN the puzzle catalog contains more than 256 entries, THE SYSTEM SHALL
  enumerate swap-rule filtered puzzles without truncating the collection so
  every puzzle remains accessible.
- WHEN the player switches from free play to puzzle mode, THE SYSTEM SHALL
  display the first unsolved puzzle for the active swap-rule filter.
- WHEN a puzzle record is requested through `get_puzzle_by_index`, THE SYSTEM
  SHALL derive the `is_solved` flag from the runtime bitset rather than the
  serialized catalog byte.
- WHEN `mark_puzzle_solved` is invoked, THE SYSTEM SHALL set the corresponding
  runtime bitset entry to one and best-effort mirror the value into the catalog
  byte at `0x30000 + offset` before updating the in-memory cache.
- THE SYSTEM SHALL treat the 600-bit runtime puzzle bitset as the
  authoritative source of solve state while the catalog bytes remain a
  best-effort mirror for tooling compatibility.
- IF a puzzle index exceeds the stored count, THEN THE SYSTEM SHALL return
  `NULL` and leave previously cached puzzle data unchanged.
- WHEN `display_puzzle_solution` is called, THE SYSTEM SHALL format the first
  solution move into 25 characters, append swap markers when applicable, and
  print it to the hint row.

### Artificial Intelligence
- WHEN the AI takes its turn, THE SYSTEM SHALL copy the root board into a
  scratch buffer, align the current player, and evaluate moves from that
  snapshot.
- WHEN the AI evaluates a turn, THE SYSTEM SHALL score every legal move using
  the heuristic evaluator and annotate each candidate with immediate-win,
  opponent-win-next, and forced-win flags.
- WHEN the AI generates candidate moves, THE SYSTEM SHALL flag every move
  that concedes an immediate opponent win and exclude those flags from the
  standard selector while leaving them available to the blunder chooser.
- WHEN the AI detects that applying a move hands the opponent an immediate win
  on their reply, THE SYSTEM SHALL cache that flag on the candidate so later
  evaluation passes can skip redundant win searches.
- WHEN the AI determines whether every legal reply concedes an opponent
  immediate win, THE SYSTEM SHALL reuse the cached candidate flags from the
  current generation pass instead of recomputing board outcomes.
- WHEN the AI inspects immediate wins, THE SYSTEM SHALL consider every legal
  move for the player to move, even if the total exceeds the ordered-move
  buffer used for heuristic ranking.
- IF a candidate yields an immediate win for the AI, THEN THE SYSTEM SHALL
  select that move and end evaluation.
- WHEN candidate generation identifies an immediate win for the side to move,
  THE SYSTEM SHALL stop enumerating further moves and return only the winning
  candidates for evaluation.
- WHEN a candidate allows an opponent immediate win on their next turn, THE
  SYSTEM SHALL discard the candidate unless the configured blunder rules
  select it.
- WHEN difficulty is STANDARD or EXPERT, THE SYSTEM SHALL also discard moves
  that let the opponent create a forced win; STANDARD may still pick such a
  move through blunder selection.
- WHEN difficulty is LEARNING or EASY, THE SYSTEM SHALL skip forced-win
  detection to keep evaluation lightweight.
- WHERE blunder mode is enabled and its chance roll succeeds, THE SYSTEM SHALL
  pick a move that concedes an opponent immediate win (Learning/Easy) or an
  opponent forcing sequence (Standard).
- WHEN difficulty is EXPERT, THE SYSTEM SHALL disable all blunder behaviour.
- WHEN randomisation is enabled for the current difficulty, THE SYSTEM SHALL
  choose uniformly among the top-k evaluated moves whenever the epsilon roll
  succeeds; EXPERT difficulty keeps top-k at one so no randomisation occurs.
- WHEN the candidate filter rejects every move, THE SYSTEM SHALL fall back to
  a uniformly random legal move to guarantee progress.
- WHEN `diagnostics_enabled` is true, THE SYSTEM SHALL call
  `ai_print_diagnostics` with node and tick counts and compute an evaluation
  breakdown for the chosen move via `ai_agent_evaluate_internal`.
- WHEN `ai_agent_hint_trace_enable` is true, THE SYSTEM SHALL capture up to
  sixty-four evaluation records per turn and expose them through
  `ai_agent_hint_trace_get` for host tooling.
- WHEN host diagnostics analyse a position, THE SYSTEM SHALL expose helpers
  that report whether a legal move creates or concedes immediate or forced
  wins so regressions can inspect tactical coverage.
- WHEN forced-win evaluation runs, THE SYSTEM SHALL examine every legal
  opponent reply after a candidate move so forced sequences are detected even
  when the move count surpasses the ordered buffer.
- WHEN forced-win detection considers an opponent opportunity, THE SYSTEM
  SHALL restrict the initiating candidate set to swap moves so early-game
  forcing analysis avoids unnecessary enumeration.
- WHEN the heuristic scoring stage runs after candidate generation, THE SYSTEM
  SHALL reuse the previously computed forced-win flags instead of executing a
  second forcing analysis pass.
- WHEN any move changes swapped status, THE SYSTEM SHALL update per-player
  swapped-piece counters on the board state so heuristic queries avoid full
  board scans.
- WHEN the AI orders candidate moves, THE SYSTEM SHALL sort an indirect index
  list referencing the candidate buffer rather than relocating the underlying
  move records to minimise memory churn.
- WHEN the HINT system runs, THE SYSTEM SHALL reuse the same heuristic
  pipeline from the perspective of the requesting player while disabling
  randomisation and blunder effects.
- WHEN the AI difficulty experiment harness runs, THE SYSTEM SHALL initialise
  `kStartingLayout2` for each game, alternate Easy and Expert colours on
  successive trials, and report wins, losses, draws, and average half-moves
  per difficulty.
- WHEN the experiment harness starts, THE SYSTEM SHALL seed its deterministic
  pseudo-random generator from the provided seed value so identical
  configurations yield reproducible summaries.
- WHEN the experiment harness configures epsilon-random competitors, THE
  SYSTEM SHALL apply the requested random-move percentage while keeping the
  opposing profile deterministic.

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

### Achievements
- WHEN the achievements subsystem initialises, THE SYSTEM SHALL load persistent
  progress into a struct-of-arrays state with bit-packed unlock flags and per-
  achievement counters sized for the 6502 memory budget.
- WHEN `achievements_refresh_catalog` populates the puzzle totals, THE SYSTEM
  SHALL retain the computed per-rule and catalog totals until a subsequent
  catalog refresh so HUD progress denominators remain stable.
- WHEN legacy achievement data omits puzzle totals, THE SYSTEM SHALL
  reconstruct per-rule and catalog puzzle counts from the active catalog
  during load before updating progress displays.
- WHEN achievements are enumerated for presentation, THE SYSTEM SHALL emit them
  in the order specified in the Freeplay and Puzzle lists of this revision so
  screen layouts can display progress without re-sorting.
- WHEN a freeplay win is recorded, THE SYSTEM SHALL increment the cumulative
  win counter, unlock the one-, ten-, and one-hundred-win achievements at the
  respective thresholds, and refresh per-achievement progress values.
- WHEN a freeplay win occurs on a starting layout, THE SYSTEM SHALL set the bit
  corresponding to that layout and unlock "Win all starting positions" once all
  layout bits are set.
- WHEN a freeplay win occurs under a swap rule, THE SYSTEM SHALL set the bit
  for that rule and unlock "Win a game in all swap rules" once every rule bit
  is present.
- WHEN a freeplay win is achieved against Expert difficulty, THE SYSTEM SHALL
  increment the Expert win counter and unlock the one- and ten-win Expert
  achievements at their thresholds.
- WHEN a freeplay win ends with a total move count under ten plies, THE SYSTEM
  SHALL record the best move count and unlock the "Win in under 10 moves"
  achievement.
- WHEN a puzzle attempt starts, THE SYSTEM SHALL reset the no-hint flag, start
  a 900-tick countdown via Timer0 for the thirty-second benchmark, and track the
  swap rule and solution length for the active puzzle.
- WHEN the main loop polls Timer0, THE SYSTEM SHALL forward the alarm tick to
  the achievements subsystem so timed puzzle counters expire exactly when the
  900-tick budget is exhausted.
- WHEN a puzzle is solved without hints and within the tracked timer budget,
  THE SYSTEM SHALL update the fast-solve counter, unlock the "Solve first
  puzzle" and "Solve 10 puzzles in under 30 seconds each" achievements at their
  thresholds, and increment the no-hint solve counter toward the "Solve 35"
  milestone.
- WHEN the player solves a qualifying "Win in 3" or "Win in 4" puzzle without
  hints, THE SYSTEM SHALL unlock the respective achievements based on the
  puzzle's solution length metadata.
- WHEN puzzles are solved without hints while puzzle mode remains active, THE
  SYSTEM SHALL increment the session counter and unlock "Solve 50 puzzles in
  one session" once the counter reaches fifty.
- WHEN a puzzle is newly marked solved for its swap rule, THE SYSTEM SHALL
  update per-rule completion counts, unlock "Solve all puzzles for a swap rule"
  once every puzzle in that rule is solved, and unlock "Solve all puzzles in the
  collection" once the aggregate solved count equals the catalog total.

### File I/O
- WHEN the application starts, THE SYSTEM SHALL check for the existence of "f256_switch.dat" in the local directory.
- WHEN "f256_switch.dat" exists at startup, THE SYSTEM SHALL restore the
  600-bit puzzle solved bitset using `puzzle_catalog_deserialize_solved`.
- WHEN "f256_switch.dat" exists at startup, THE SYSTEM SHALL load achievement status using achievements_deserialize.
- WHEN the application exits, THE SYSTEM SHALL persist the 600-bit puzzle
  solved bitset to "f256_switch.dat" using `puzzle_catalog_serialize_solved`.
- WHEN the application exits, THE SYSTEM SHALL write achievement status to "f256_switch.dat" using achievements_serialize.
- WHEN writing to "f256_switch.dat", THE SYSTEM SHALL overwrite any existing file.
- IF file operations fail during load, THEN THE SYSTEM SHALL continue execution with default state.
- IF file operations fail during save, THEN THE SYSTEM SHALL continue execution without saving.

## Non-Functional Requirements
- THE SYSTEM SHALL wait for the raster to reach the VBLANK window before
  calling `render_update` to avoid tearing on VICKY hardware.
- THE SYSTEM SHALL run AI evaluation code from far-memory overlays, swapping
  MMU bank `0x000D` to block 8 or 9 as required and restoring the previous
  mapping afterwards.
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
| AI engine and hints | `src/ai_agent.c`, `tests/ai_agent_tests.c` | Heuristic move ranking, forced-win checks, hint tracing |
| Puzzle streaming | `src/puzzle_data.c`, `assets/generated` | Far-memory catalog access |
| System integration | `src/main.c`, `src/system.c` | Main loop, Foenix reset sequence |
