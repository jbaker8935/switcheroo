---
post_title: "F256 Switcharoo Implementation Tasks"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-tasks"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Project Planning", "Tasks"]
ai_note: "Drafted with AI assistance based on provided materials. Added task for puzzle mode difficulty default change."
summary: "Implementation roadmap and task breakdown for the F256 Switcharoo project."
post_date: 2025-10-30
---

## Overview

This plan decomposes the current Switcharoo requirements into actionable tasks
that match the implementation recorded on 2025-10-30. The board engine, puzzle
pipeline, rendering, and AI search are all active on hardware and in the host
tests. Menu polish, audio, undo, and advanced error handling remain in the
backlog.

## Task Board

| ID | Title | Description | Deliverables | Dependencies | Status |
| --- | --- | --- | --- | --- | --- |
| T1 | Toolchain Setup | Configure LLVM-MOS, `f256lib`, and asset generation scripts; validate `.pgz` packaging. | Build scripts, linker script, packaging artefacts. | None | Done |
| T2 | Video & Asset Bring-Up | Embed board bitmap and sprites, configure palettes, and stage sprite attribute tables. | `video.c` helpers, embedded assets, render smoke test. | T1 | Done |
| T3 | Input Translation & Focus | Translate mouse/keyboard hardware events and provide selection/focus helpers. | `input.c`, `input_handler.c`, host stubs. | T1 | Done |
| T4 | Board & Swap Rules | Implement 8x4 board, move validation, swap rule handling, and win detection. | `board.c`, unit coverage in host tests. | T1 | Done |
| T5 | Game State Orchestration | Manage phases, menu actions, turn switching, and HUD updates. | `game_state.c`, menu activation API. | T2, T3, T4 | Done |
| T6 | Rendering & HUD | Project board state to sprites, highlight wins, and print HUD text. | `render.c`, `text_display.c`, palette reset helpers. | T2, T4, T5 | Done |
| T7 | Puzzle Streaming | Stream binary catalog from far memory, apply puzzles, and show diagnostics. | `puzzle_data.c`, catalog embed, text feedback. | T2, T4, T5 | Done |
| T8 | AI Heuristic Engine | Provide immediate-win aware heuristic ranking with forced-win checks, top-k tuning, and diagnostics. | `ai_agent.c`, host regression harness. | T4, T5 | Done |
| T9 | Diagnostics & Host Harnesses | Maintain hint traces, AI breakdown reporting, and desktop test shims. | `tests/ai_agent_tests.c`, `tests/hint_trace_host.c`, docs. | T8 | Done |
| T10 | Menu & UX Polish | Implement disabled-icon visuals, hover feedback, and puzzle-aware enables. | Updated sprites, `game_state_update_menu_enables`. | T5, T6 | Not Started |
| T11 | Audio Layer | Add cue playback, volume/mute controls, and align input bindings. | Audio driver module, assets, HUD indicators. | T1, T5 | Not Started |
| T12 | Undo & Accessibility | Provide undo stack, keyboard-only UX fixes, and colourblind themes. | Board history snapshots, palette swaps, tests. | T4, T5, T6 | Not Started |
| T13 | Error Handling Hardening | Centralise error reporting and graceful recovery beyond HUD text. | Error manager, recovery flows, tests. | T5, T7 | Not Started |
| T14 | Documentation Sync | Keep `requirements.md`, `design.md`, and `tasks.md` aligned with code. | Updated docs, traceability notes. | T1-T9, T16 | In Progress |
| T15 | Release QA & Packaging | Run regression suite, hardware smoke tests, and finalise build artefacts. | Test logs, release notes, packaged `.pgz`. | T1-T14 | Not Started |
| T16 | AI Progress Callback | Legacy callback for deep search updates; retained only for backward compatibility after heuristic simplification. | `ai_agent.h/.c`, `ui_progress.c/.h` legacy wiring. | T8 | Superseded |
| T17 | AI Progress Responsiveness | Legacy optimisation of progress callbacks; behaviour now superseded by the non-search heuristic engine. | Historical notes, profiling logs. | T16 | Superseded |
| T18 | AI Move Generation Optimisation | Inline adjacency walk with ownership LUT to reduce per-move helper calls during search. | `ai_agent.c` direct generator, host/target profiling notes. | T8 | Done |
| T19 | AI Difficulty Experiment Harness | Build a host-side executable to compare Easy vs Expert matchups on kStartingLayout2, including epsilon-random Expert baselines and summary reporting. | `tests/ai_difficulty_experiments.c`, reproducible output samples. | T8, T18 | Done |
| T20 | AI Inevitability Shortcut | Detect forced-loss states and fall back to the shallow selector when all replies concede an immediate opponent win. | `ai_agent.c` inevitability helper, host regression assertion. | T8, T18 | Done |
| T21 | AI Blunder Behaviour | Enable difficulty-specific blunders with configurable probability, allowing immediate-win or forcing-line mistakes per difficulty rules. | `ai_agent.c` blunder helper, config API, host regression coverage. | T8, T18, T20 | Done |
| T22 | Expert Forced-Loss Regression | Add host regression to confirm the Expert AI avoids `kStartingLayout2` sequences that hand White a forced win and captures documentation updates. | `tests/ai_agent_tests.c`, requirements/design sync. | T8, T9, T20 | Done |
| T23 | Forced Immediate Win Diagnostics | Expose host helper and expand regression logging to enumerate forced-win detection across legal moves. | `src/ai_agent.c`, `tests/ai_agent_tests.c` | T8, T9, T22 | Done |
| T24 | Immediate Loss Guard | Penalise or discard AI moves that allow the opponent an immediate win and document the behaviour. | `src/ai_agent.c`, docs | T8, T20, T22 | Done |
| T25 | AI Immediate-Win Prescan | Short-circuit deep search by checking all legal moves for immediate wins at the root. | `src/ai_agent.c`, requirements/design/tasks sync. | T8, T18, T20 | Done |
| T26 | Puzzle Mode Standard Difficulty Default | Change puzzle mode AI difficulty default from Expert to Standard for better accessibility. | `src/game_state.c`, documentation updates. | T5, T8 | Done |
| T26 | AI Search Simplification | Replace the negamax pipeline with heuristic-only move selection per `ai_agent_simplification.md`. | `ai_agent.c`, requirements/design/tasks updates, host regressions. | T8, T21 | Done |
| T27 | AI Forcing Coverage Fix | Ensure immediate- and forced-win helpers iterate all legal moves beyond the ordered buffer limit and update diagnostics. | `src/ai_agent.c`, `tests/ai_agent_tests.c`, docs | T8, T23, T25 | Done |
| T28 | Host Profiling Pass | Instrument `tests/ai_agent_tests` with gprof, capture `gmon.out`, and archive profiling artefacts. | Profiling command log, `gmon.out`, hotspot summary. | T8, T9, T24 | Done |
| T29 | Win Detection Optimisation Review | Analyse profiling hotspots for `board_check_win` and related helpers, evaluate caching or structural optimisations, and document recommendations. | Profiling analysis, optimisation proposal. | T28 | Done |
| T30 | Swapped Piece Tracking | Maintain per-player swapped piece indices so clearing routines avoid full-board scans; re-profile to validate gains. | `src/board.c`, `src/board.h`, profiling comparison. | T4, T28 | Done |
| T31 | Winning Row Counters | Cache winning-row occupancy per player and refresh on init and move execution. | `src/board.c`, `src/board.h`, docs sync. | T4, T29 | Done |
| T32 | Move History Isolation | Provide a no-history board move helper for AI simulations and refresh docs. | `src/board.c`, `src/board.h`, `ai_agent.c`, docs. | T4, T18, T31 | Done |
| T33 | Expert Own-Goal Guard | Ensure fallback move selection never concedes an immediate win when only risky replies remain. | `src/ai_agent.c`, host regression notes. | T8, T24 | Done |
| T34 | Forced-Loss Short-Circuit | Reinstate the forced-loss precheck so deep forcing analysis is skipped when every reply still loses. | `src/ai_agent.c`, profiling logs. | T8, T20 | Done |
| T35 | AI Own-Goal Pruning | Remove own-goal moves during candidate generation so evaluation and fallback never select them. | `src/ai_agent.c`, documentation sync. | T8, T24, T33 | In Progress |
| T36 | AI Tactical Streamlining | Remove duplicate immediate/forced checks, stop enumeration after self wins, and force deterministic hint selection. | Updated `src/ai_agent.c`, docs refresh. | T8, T25 | Done |
| T37 | Forced Win Swap Filter | Limit FORCED_WIN_A analysis to swap moves only, reducing early-game overhead while preserving reply coverage. | `src/ai_agent.c`, documentation updates. | T8, T36 | Done |
| T38 | Swap Count Cache | Maintain per-player swapped-piece counters on the board so AI heuristics avoid repeated full-board scans. | `src/board.c`, `src/board.h`, `src/ai_agent.c`. | T4, T18 | Done |
| T39 | Indexed Move Ordering | Use an indirect index table for AI move ordering to eliminate repeated struct shuffling. | `src/ai_agent.c`. | T8, T36 | Done |
| T40 | Evaluation Forcing Shortcut | Skip redundant forcing checks during move evaluation while preserving penalty scoring hooks. | `src/ai_agent.c`, profiling notes. | T8, T37 | Done |
| T41 | Bitmask Win Detection | Replace the BFS array visit tracking with a 24-bit mask in `board_check_win_fast` to eliminate memset overhead. | `src/board.c`, host benchmark logs. | T4, T30 | Done |
| T42 | Move Flag Sharing | Cache immediate-win results during move generation and reuse them in evaluation to avoid duplicate win checks. | `src/ai_agent.c`. | T8, T40 | Done |
| T43 | Popcount Lookup Table | Switch the popcount helper to nibble-table lookups for fewer shifts on 65C816. | `src/ai_agent.c`. | T8 | Done |
| T44 | Win Mask Lookup Cache | Precompute victory-row bit masks so `board_check_win_fast` avoids dynamic shifts on target hardware. | `src/board.c`, profiling notes. | T41 | Done |
| T45 | Opponent Immediate Flagging | Persist opponent immediate-win results from move generation to evaluation to cut redundant searches. | `src/ai_agent.c`. | T42 | Done |
| T46 | Move Enumeration Cache | Reuse the generated candidate list for forced-loss checks and evaluation to eliminate redundant board simulations. | `src/ai_agent.c`. | T42 | Done |
| T47 | Puzzle Difficulty Retention | Keep manually selected puzzle difficulty across resets and navigation so lightweight heuristics stay active. | `src/game_state.c`. | T14 | Done |
| T48 | Default Game Mode Change | Change the default game mode from free play to puzzle mode on game start. | `src/game_state.c`, `requirements.md` updates. | T5 | Done |
| T49 | Puzzle Filter Scaling | Replace fixed-size swap-rule index caches with on-demand catalog scans so large puzzle sets stay addressable. | `src/puzzle_data.c`, docs sync. | T7 | Done |
| T50 | Puzzle Mode First Unsolved | Show the first unsolved puzzle whenever puzzle mode is entered from free play. | `src/game_state.c`, docs sync. | T7 | Done |
| T51 | Help Text Embedding | Embed the help text from `assets/mockup/help_text.txt` as a C string constant in `src/help.c` for in-game display. | `src/help.c`, build validation. | None | Done |
| T52 | Achievement Tracking Core | Implement struct-of-arrays achievement state, persistence helpers, and freeplay/puzzle event integration. | `src/achievements.c/.h`, `src/game_state.c`, docs sync. | T5, T7 | In Progress |
| T53 | Timed Puzzle Achievement Timer | Start and poll the 900-tick puzzle timer through the achievements subsystem and integrate hint/no-hint tracking. | `src/achievements.c`, `src/main.c`, `src/game_state.c`, docs sync. | T5, T7, T52 | In Progress |

## Milestones
- **M1: Core Bring-Up (T1-T7)** – Board, puzzle, rendering, and main loop functional on hardware. ✓ Done
- **M2: Competitive AI (T8-T9)** – Adaptive search with diagnostics validated on host. ✓ Done
- **M3: UX & Audio Polish (T10-T12)** – Menu feedback, accessibility, and cue playback. ⏳ Pending
- **M4: Robustness & Release (T13-T15)** – Error handling, documentation sync, and QA. ⏳ Pending

## Resource Needs
- Foenix F256K2 hardware or emulator to validate VRAM timing and input latency.
- Asset support for icon states, hover sprites, and future palettes.
- Time budget for AI tuning runs and documentation upkeep.

## Risks and Mitigations
- **UX Gaps:** Missing hover/disabled cues may confuse users. Mitigate by
  prioritising T10 and adding visual assets for menu state.
- **Audio Silence:** Absent cues make state changes harder to perceive. Mitigate
  by scheduling T11 work once assets are available.
- **Error Recovery:** Current HUD-only messaging may be insufficient on failure.
  Mitigate by introducing central logging and recovery flows (T13).
- **Documentation Drift:** Active development can desynchronise requirements and
  design notes. Mitigate by revisiting T14 each sprint.

## Validation Checklist
- Host regression suite (`tests/ai_agent_tests`, `tests/example2_simple_test`) passes with deterministic output.
- Hardware smoke test verifies rendering alignment, puzzle streaming, and AI move cadence.
- Menu interactions (reset, puzzle select, difficulty toggle, hint) behave per
  requirements with clear HUD feedback.
- Win detection highlights correct paths and clears on reset.
- Diagnostics output (hint traces, AI breakdown) remains accessible when
  enabled.
- Documentation set (`requirements.md`, `design.md`, `tasks.md`) reflects the
  shipped behaviour after each iteration.
