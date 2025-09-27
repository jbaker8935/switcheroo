---
post_title: "F256 Switcharoo Requirements Specification"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-requirements"
microsoft_alias: "copilot"
featured_image: ""
categories: ["Architecture"]
tags: ["Foenix F256", "Game Design", "Requirements"]
ai_note: "Drafted with AI assistance based on provided materials."
summary: "Requirements specification for the F256 Switcharoo strategy game."
post_date: 2025-09-27
---

## Overview

F256 Switcharoo is a head-to-head abstract strategy game for the Foenix F256K2
platform. A human player competes against a heuristic-driven AI while managing
a board of four columns by eight rows. This document translates the game
concept from `game_design_request.md` into testable Easy Approach to
Requirements Syntax (EARS) statements. Platform capabilities reference the
`f256jr_ref.pdf` hardware manual, and runtime services reference the
`f256lib.pdf` LLVM-MOS library guide.

## Stakeholders

- Human player using keyboard and/or mouse input
- AI opponent operating on-device with heuristic evaluation
- Game developers integrating with Foenix hardware primitives
- Future maintainers extending rules, assets, or heuristics

## Functional Requirements

### Board and Pieces

- WHEN the title screen transitions to gameplay, THE SYSTEM SHALL initialize an
  8x4 board with player A pieces placed on rows 7-8 and player B pieces placed
  on rows 1-2 unless an alternate starting layout is selected.
- WHEN a player performs an empty cell move, THE SYSTEM SHALL relocate the
  selected piece to the chosen adjacent empty cell and mark all swapped pieces
  involved in prior swaps as normal.
- WHEN a player performs a swap move, THE SYSTEM SHALL exchange the positions
  of the initiating piece and the targeted opponent normal piece and mark both
  as swapped.
- IF a piece is marked as swapped, THEN THE SYSTEM SHALL prevent it from being
  targeted by opponent swap moves while permitting it to initiate swaps against
  normal opponent pieces.
- WHEN a move yields a continuous chain of the active player's pieces connecting
  any cell in row 2 to any cell in row 7 via 8-way adjacency, THE SYSTEM SHALL
  declare victory for that player.
- IF a piece is marked swapped, then the system will display a different piece graphic from the normal piece graphic.

### Turn Management

- WHEN gameplay begins, THE SYSTEM SHALL assign the first turn to the human
  player controlling the white pieces.
- WHEN a legal move completes, THE SYSTEM SHALL toggle the active player unless
  the move ends the game.
- IF a player has no legal moves on their turn, THEN THE SYSTEM SHALL skip that
  turn and notify both players.

### Input and Selection

- WHEN the user hovers the mouse cursor over a board cell or menu icon, THE
  SYSTEM SHALL display a hover state sprite consistent with the UI theme.
- WHEN the user clicks or presses Enter on a highlighted piece, THE SYSTEM
  SHALL toggle its selection state and highlight all legal destination cells.
- WHEN the user selects a highlighted destination, THE SYSTEM SHALL execute the
  associated move, update game state, and refresh highlights.
- WHEN the user presses Escape while a piece is selected Or clicks the mouse while hovering over the piece, THE SYSTEM SHALL deselect the piece and clear move highlights.

### Menu and Overlays

- WHEN the user activates the Reset icon, THE SYSTEM SHALL restore the board to
  the currently chosen starting layout and reset swapped states and
  move history for the active session.
- WHEN the user activates the Information icon, THE SYSTEM SHALL present an
  overlay containing the condensed rules and controls until dismissed via mouse
  click or Escape.
- WHEN the user activates the Difficulty icon, THE SYSTEM SHALL cycle through
  available AI difficulty presets and immediately apply the new heuristic
  configuration.
- WHEN the user activates the Starting Board icon, THE SYSTEM SHALL display and
  apply one of the predefined layouts without exiting the current session.
- WHEN the user activates the Move History icon, THE SYSTEM SHALL open an
  overlay list of moves with the newest entry at the top and allow dismissal via
  mouse click or Escape.
- WHEN the user activates the Exit icon, THE SYSTEM SHALL prompt for
  confirmation and quit the application only on affirmative response.
- WHEN at least one move has been made in the game, THE SYSTEM SHALL disable the Starting Board icon and will enable the Move History icon.
- WHEN the game is initialized, THE SYSTEM SHALL enable the Starting Board icon and disable the Move History icon.
- WHEN an icon is disabled, THE SYSTEM SHALL display the icon with the disabled state consistent with the UI them and the icon will not display the hover state when the mouse is positioned over the disabled icon

### Move History and Scoring

- WHEN a move completes, THE SYSTEM SHALL append an algebraic notation entry to
  the move history and scroll older entries as needed.
- WHEN a player wins, THE SYSTEM SHALL increment their session score and highlight the winning path cells until the game is reset.
WHEN a game is initialized Or a player wins, THE SYSTEM SHALL display the session score as a bitmap graphic positioned under the last menu item as:  W: (white wins) B: (black wins)
- WHEN the session resets, THE SYSTEM SHALL clear the move history unless a tournament mode is introduced in future updates.

### Artificial Intelligence

- WHEN it is the AI player's turn, THE SYSTEM SHALL evaluate legal moves using
  heuristic weights for row occupancy between 2 and 7, connectivity, swapped
  piece count, opponent threats, and loss avoidance patterns.
- WHEN evaluating a candidate move, THE SYSTEM SHALL detect immediate wins or
  losses and prioritize moves that secure a win or prevent the opponent from
  winning on the next turn.
- WHEN the selected difficulty changes, THE SYSTEM SHALL adjust heuristic depth,
  iteration limits, or weight values to match the chosen profile.

### Audio Feedback

- WHEN the program finishes initialization and presents the title screen, THE
  SYSTEM SHALL play a startup audio cue.
- WHEN a new game session initializes or the board is reset, THE SYSTEM SHALL
  play a distinct game initialization audio cue.
- WHEN the user selects or deselects a piece, THE SYSTEM SHALL play a short
  selection audio cue differentiating the pick-up and put-back actions.
- WHEN the user activates a menu icon that is enabled, THE SYSTEM SHALL play a
  menu confirmation audio cue.
- WHEN the human player wins a game, THE SYSTEM SHALL play a victory jingle.
- WHEN the human player loses a game, THE SYSTEM SHALL play a defeat jingle.
- WHEN the user confirms exit from the program, THE SYSTEM SHALL play a program
  exit audio cue prior to terminating.

## Non-Functional Requirements

### Performance

- THE SYSTEM SHALL maintain at least 30 frames per second during board
  rendering transitions on the Foenix F256K2 hardware.
- THE SYSTEM SHALL compute AI moves within 500 milliseconds under the
  "Standard" difficulty profile.

### Reliability

- THE SYSTEM SHALL recover gracefully to the main menu when detecting sprite or
  bitmap initialization failures by reloading assets from `f256lib` services.
- THE SYSTEM SHALL validate input buffers to avoid corruption when switching
  between mouse and keyboard controls.

### Usability

- THE SYSTEM SHALL provide consistent color-coded highlights for selectable
  pieces, legal moves, and winning paths for accessibility.
- THE SYSTEM SHALL provide clear textual feedback for difficulty level, current
  turn, and prompts.

### Compliance and Platform Integration

- THE SYSTEM SHALL initialize video modes, sprite layers, and audio via
  documented registers from `f256jr_ref.pdf` using helper abstractions supplied
  by `f256lib.pdf`.
- THE SYSTEM SHALL remain compatible with the LLVM-MOS toolchain versions cited
  in `f256lib.pdf`.

## Assumptions and Constraints

- THE SYSTEM SHALL leverage 320x240 graphics mode with 28x28 pixel board cells and 24x24 pixel sprites as specified in the design brief.  Board cells will be separated with a 1 pixel border and the entire board will have a 1 pixel boarder.
- The SYSTEM SHALL color board cells in a checkerboard pattern consistent with the UI theme
- THE SYSTEM SHALL color highlighted cells for a winning path in a color consisent with the UI theme
- THE SYSTEM SHALL use colors for the winning path from each player.
- THE SYSTEM SHALL display both winning paths if a move creates a simultaneous win for both players.
- THE SYSTEM SHALL store persistent session stats in volatile memory; long-term
  persistence is out of scope.
- THE SYSTEM SHALL operate without network connectivity.

## Traceability Matrix

| Requirement Area | Source Reference | Notes |
| --- | --- | --- |
| Board setup and movement | `game_design_request.md` | Core gameplay rules |
| Swap rules and states | `game_design_request.md` | Unique mechanic |
| Victory conditions | `game_design_request.md` | Connectivity check |
| Menu interactions | `game_design_request.md` | UI specification |
| AI heuristics | `game_design_request.md` | Difficulty profiles |
| Hardware integration | `f256jr_ref.pdf` | Video and input subsystems |
| Library usage | `f256lib.pdf` | LLVM-MOS runtime and helpers |
