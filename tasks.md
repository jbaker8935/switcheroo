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
post_date: 2025-10-19
---

## Overview

This plan decomposes the current Switcharoo requirements into actionable tasks
that match the implementation recorded on 2025-10-19. The board engine, puzzle
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
| T8 | AI Search Engine | Provide adaptive-depth negamax with heuristic evaluation and diagnostics. | `ai_agent.c`, host regression harness. | T4, T5 | Done |
| T9 | Diagnostics & Host Harnesses | Maintain hint traces, AI breakdown reporting, and desktop test shims. | `tests/ai_agent_tests.c`, `tests/hint_trace_host.c`, docs. | T8 | Done |
| T10 | Menu & UX Polish | Implement disabled-icon visuals, hover feedback, and puzzle-aware enables. | Updated sprites, `game_state_update_menu_enables`. | T5, T6 | Not Started |
| T11 | Audio Layer | Add cue playback, volume/mute controls, and align input bindings. | Audio driver module, assets, HUD indicators. | T1, T5 | Not Started |
| T12 | Undo & Accessibility | Provide undo stack, keyboard-only UX fixes, and colourblind themes. | Board history snapshots, palette swaps, tests. | T4, T5, T6 | Not Started |
| T13 | Error Handling Hardening | Centralise error reporting and graceful recovery beyond HUD text. | Error manager, recovery flows, tests. | T5, T7 | Not Started |
| T14 | Documentation Sync | Keep `requirements.md`, `design.md`, and `tasks.md` aligned with code. | Updated docs, traceability notes. | T1-T9, T16 | In Progress |
| T15 | Release QA & Packaging | Run regression suite, hardware smoke tests, and finalise build artefacts. | Test logs, release notes, packaged `.pgz`. | T1-T14 | Not Started |
| T16 | AI Progress Callback | Implement callback mechanism for UI progress updates during AI move search. | Updated `ai_agent.h/.c`, `ui_progress.c/.h`, callback wiring. | T8 | Done |
| T17 | AI Progress Responsiveness | Boost callback cadence by wiring frequent win-detection helpers into the throttled progress emitter. | Updated `ai_agent.c` helper hook, profiling validation notes. | T16 | Done |
| T18 | AI Move Generation Optimisation | Inline adjacency walk with ownership LUT to reduce per-move helper calls during search. | `ai_agent.c` direct generator, host/target profiling notes. | T8 | Done |

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
