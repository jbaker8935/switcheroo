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

## Task Board

| ID | Title | Description | Deliverables | Dependencies | Status |
| --- | --- | --- | --- | --- | --- |
| T1 | Toolchain Setup | Configure LLVM-MOS, `f256lib`, and asset build tools; verify hardware emulator pipeline. | Build scripts, validated tool versions. | None | Not Started |
| T2 | Video Initialization | Implement video mode, sprite layer setup, and asset loader stubs per `f256jr_ref.pdf`. | Video init module, smoke test. | T1 | Not Started |
| T3 | Input Subsystem | Integrate mouse and keyboard polling with event translation, including deselection via Escape or re-click. | Input manager module, diagnostic overlay. | T1 | Not Started |
| T4 | Board Model | Implement board data structures, adjacency lookup tables, swap state tracking, and rule enforcement. | Board module, unit tests. | T1 | Not Started |
| T5 | Move History & Scoring | Build move logging, score tracking, session score display, and reset logic (clears history, preserves scores). | History buffer module, scoreboard renderer tests. | T4 | Not Started |
| T6 | Rendering Pipeline | Draw checkerboard board bitmap with border, place normal/swapped sprites, render color-coded highlights, disabled icons, and scoreboard strip. | Render pipeline module, visual test. | T2, T4 | Not Started |
| T7 | Menu System | Implement icon widgets with enable/disable states, hover suppression for disabled icons, and overlay transitions. | Menu module, UI test. | T3, T6 | Not Started |
| T8 | AI Engine | Implement heuristic evaluation, search strategy, and difficulty presets. | AI module, evaluation tests. | T4 | Not Started |
| T9 | Game Loop Integration | Tie together input, rules, AI, rendering, and overlays into the main loop. | Main loop module, integration test. | T2, T3, T4, T6, T7, T8 | Not Started |
| T10 | Victory & Highlight | Detect winning paths, support simultaneous wins, and animate per-player highlight colors until reset. | Victory detector, highlight routine. | T4, T6 | Not Started |
| T11 | Audio Feedback | Implement audio service and cues for startup, game init/reset, selection/deselection, menu confirm, victory, defeat, and exit. | Audio module, cue asset pack, trigger tests. | T2 | Not Started |
| T12 | QA & Polishing | Execute test suite, hardware profiling, bug fixes, and documentation updates. | Test reports, updated docs. | T1-T11 | Not Started |

## Milestones

- **M1: Engine Skeleton (T1-T4)** – Game logic runs in headless harness.
- **M2: Playable Prototype (T5-T9)** – Complete human vs AI loop with core UI.
- **M3: Launch Candidate (T10-T12)** – Visual polish, audio, validation.

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

- Unit tests pass for rule engine, AI evaluation, history logging, and menu enable logic.
- Rendering verified on real hardware and emulator for alignment, frame rate, swapped piece art, and scoreboard layout.
- User acceptance tests confirm menu functions, overlays, icon disable behavior, win highlighting longevity, and audio cue playback for all specified events.
- Documentation updated to reflect any changes in controls or heuristics.
