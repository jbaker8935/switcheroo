---
post_title: "F256 Switcharoo Implementation Tasks"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-tasks"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Project Planning", "Tasks"]
ai_note: "Drafted with AI assistance based on provided materials."
summary: "Implementation roadmap and task breakdown for the F256 Switcharoo project."
post_date: 2025-09-27
---

## Overview

This plan decomposes the Switcharoo requirements into actionable tasks that can
be tracked through implementation. Tasks align with the architecture outlined
in `design.md` and satisfy the EARS requirements captured in `requirements.md`.

**Current Status**: Core game engine is fully implemented and playable. The game
supports human vs human play with complete board logic, input handling, rendering,
and win detection. AI implementation and audio system remain as future enhancements.

## Task Board

| ID | Title | Description | Deliverables | Dependencies | Status |
| --- | --- | --- | --- | --- | --- |
| T1 | Toolchain Setup | Configure LLVM-MOS, `f256lib`, and asset build tools; verify hardware emulator pipeline. | Build scripts, local linker script, `.pgz` packaging artifacts, validated tool versions. | None | Done |
| T2 | Video Initialization | Implement video mode, sprite layer setup, and asset loader stubs per `f256jr_ref.pdf`. | Video init module, theme palettes, placeholder assets, VRAM upload routine, smoke test. | T1 | Done |
| T3 | Input Subsystem | Integrate mouse and keyboard polling with event translation, including deselection via Escape or re-click, keyboard shortcuts, and volume controls. | Input manager module, shortcut handler, diagnostic overlay. | T1 | Done |
| T4 | Board Model | Implement board data structures, adjacency lookup tables, swap state tracking, and rule enforcement. | Board module, unit tests. | T1 | Done |
| T5 | Move History & Scoring | Build move logging, score tracking, session score display, and reset logic (clears history, preserves scores). | History buffer module, scoreboard renderer tests. | T4 | Done (Integrated in game_state) |
| T6 | Rendering Pipeline | Draw checkerboard board bitmap with border, place normal/swapped sprites, render color-coded highlights, disabled icons, and scoreboard strip. | Render pipeline module, visual test. | T2, T4 | Done |
| T7 | Menu System | Implement icon widgets with enable/disable states, hover suppression for disabled icons, and overlay transitions. | Menu module, UI test. | T3, T6 | Not Started |
| T8 | AI Engine | Implement deterministic iterative-deepening negamax with alpha-beta pruning, rule-aware heuristics, killer moves, and transposition cache. | Updated `ai_agent.c`, search diagnostics, difficulty profiles. | T4 | In Progress |
| T9 | Game Loop Integration | Tie together input, rules, AI, rendering, and overlays into the main loop. | Main loop module, integration test. | T2, T3, T4, T6, T7, T8 | Done (Core game loop implemented, AI integration pending) |
| T10 | Victory & Highlight | Detect winning paths, support simultaneous wins, and animate per-player highlight colors until reset. | Victory detector, highlight routine. | T4, T6 | Done (Detection and CLUT-based highlighting implemented) |
| T11 | Audio Feedback | Implement enhanced audio service with volume control, priority queuing, hover feedback, and expanded cue set for comprehensive audio experience. | Audio module, volume control, cue asset pack, priority system tests. | T2 | Not Started |
| T12 | QA & Polishing | Execute test suite, hardware profiling, bug fixes, and documentation updates. | Test reports, updated docs. | T1-T11 | Not Started |
| T13 | UX Enhancements | Implement undo functionality, tooltips, settings overlay, colorblind support, and AI move explanations. | UX enhancement module, accessibility tests. | T6, T7, T8 | Not Started |
| T15 | AI Validation Suite | Deliver host-side AI regression tests, self-play tuning harness, and heuristic documentation per agent specification, including the extended 1000-game-per-rule tuning regimen. | `tests/ai_agent_tests.c`, head-to-head metrics, extended tuning reports, `docs/heuristic_tuning.md`, test logs. | T8 | In Progress |
| T16 | AI Overlay Integration | Move the AI search/evaluation core into a Foenix overlay and stream it into the 0xA000 workspace to resolve the RAM overflow. | Overlay linker script, runtime loader, successful `./build.sh` run. | T8 | In Progress |
| T14 | Error Handling | Implement comprehensive error recovery, graceful degradation, and diagnostic logging across all subsystems. | Error handling framework, recovery tests. | T1-T12 | Not Started |
| T17 | AI Search Phase Gating | Implement goal-band occupancy gating so deep search activates only when 4+ rows are occupied and full depth is limited to 5+ rows. | Updated `ai_agent.c`, performance benchmarks, doc updates. | T8 | Done |
| T18 | AI Timer Diagnostics | Integrate timer0 instrumentation and on-screen diagnostics for AI node counts and elapsed ticks. | Profiling helpers in `ai_agent.c`, documentation refresh. | T8 | Done |
| T19 | AI Immediate Threat Avoidance | Enhance evaluation to detect and avoid moves allowing opponent instant wins, ensuring AI only chooses such moves when all options permit them. | Updated `ai_agent_evaluate_internal`, regression tests pass. | T8 | Done |
| T20 | AI Swap Move Safety Filter | Filter out swapping moves that result in immediate opponent wins during heuristic selection. | Updated `ai_select_move_heuristic`, build succeeds. | T8 | Done |
| T21 | AI Forcing Move Detection | Implement forcing move probes that prioritise guaranteed next-turn wins on Standard/Expert difficulties and expand evaluation guards against opponent forcing lines. | Updated `ai_agent.c`, regression tests covering forcing preference. | T8 | Done |
| T22 | Puzzle Data High Memory Migration | Serialize puzzles into a fixed-record binary blob, embed it at 0x30000, and stream records into low-memory buffers on demand. | Updated `convert_puzzles.py`, `assets/generated/puzzle_data.bin`, `src/puzzle_data.c`, documentation refresh. | T2, T4 | In Progress |
| T23 | Puzzle Loader UX Integration | Auto-apply puzzles on resets and new sessions, disable the Starting Board icon when the catalog is empty, and surface fallback messaging. | Updated `src/game_state.c`, requirements & design refresh. | T22 | Done |
| T24 | Puzzle Catalog Diagnostics | Display on-device diagnostic text for header counts, record loads, and error conditions during puzzle streaming. | Updated `src/puzzle_data.c`, `src/text_display.*`, requirements & design updates. | T22 | Done |
| T25 | Starting Board Highlight Reset | Ensure Starting Board activation clears winning-path highlights and resets board palette after wins. | Updated docs, `src/game_state.c`. | T10, T23 | Done |
| T26 | Puzzle Hint Node Cap Bypass | Ensure puzzle hint mode retains the configured depth and node cap by skipping move-volume throttles. | Updated `src/ai_agent.c`, regression test for Example 2 puzzle. | T8, T17 | Done |
| T27 | Puzzle Hint Move Ordering | Bias move ordering toward swap-preserving lines and defer swap-clearing moves in both puzzle and free play hint contexts. | Updated `src/ai_agent.c`, host regression updated. | T8, T26 | Done |
| T28 | Puzzle Hint Eval Profile | Introduce a lightweight evaluation profile and disable forcing checks during hint searches to improve responsiveness. | Updated `src/ai_agent.c`, `src/game_state.c`, regression documentation. | T8, T26 | Done |
| T29 | Puzzle Hint Move Throttles | Reapply move-volume depth/node caps to puzzle hint searches to bound runtime after the first database hint. | Updated `src/ai_agent.c`, documentation refresh. | T26, T28 | Done |
| T30 | Hint Evaluation Instrumentation | Capture deterministic hint evaluation traces and leverage the math coprocessor for weighted feature products. | Updated `src/ai_agent.c`, `src/ai_agent.h`, docs. | T8, T28, T29 | Done |
| T31 | Hint Trace Export | Persist captured hint evaluation traces to a CSV log when the game exits so diagnostics survive resets. | Updated `src/main.c`, documentation refresh, build passes. | T30 | Done |

## Milestones

- **M1: Engine Skeleton (T1-T4)** – Game logic runs in headless harness. ✓ Done
- **M2: Playable Prototype (T5-T9)** – Complete human vs AI loop with core UI. ✓ Done (AI pending)
- **M3: Enhanced Experience (T10-T13)** – Audio, visual polish, UX improvements. 🔄 In Progress
- **M4: Launch Candidate (T14)** – Error handling, validation, final testing. ⏳ Pending

## Resource Needs

- Foenix F256K2 hardware unit or emulator for on-device validation.
- Asset artist for sprite/icon iteration.
- QA support for usability and heuristic tuning.

## Risks and Mitigations

- **AI Performance:** Depth 3 search may exceed 500 ms on hardware. Mitigate via
  iterative deepening with time caps and heuristic pruning.
- **Input Latency:** Mouse polling may conflict with keyboard scanning. Mitigate
  by decoupling input updates from render ticks using double-buffered state.
- **Memory Constraints:** Sprite buffers might exceed bank capacity. Mitigate via
  asset compression and bank-aware allocator in rendering pipeline.

## Validation Checklist

- Unit tests pass for rule engine, AI evaluation, history logging, menu enable logic, and error recovery.
- Rendering verified on real hardware and emulator for alignment, frame rate, swapped piece art, scoreboard layout, and colorblind accessibility.
- User acceptance tests confirm menu functions, overlays, icon disable behavior, win highlighting longevity, audio cue playback, keyboard shortcuts, and undo functionality.
- Performance benchmarks meet targets: 30+ FPS rendering, <500ms AI moves on Standard difficulty, <250ms UI response times.
- Accessibility verification: colorblind testing, audio-off gameplay, keyboard-only navigation.
- Error handling validation: graceful recovery from hardware failures, memory constraints, and invalid inputs.
- Documentation updated to reflect any changes in controls, heuristics, or system requirements.
