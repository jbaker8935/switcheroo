---
post_title: "F256 Switcharoo Requirements Specification"
author1: "GitHub Copilot"
post_slug: "f256-switcharoo-requirements"
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
  declare victory for that player and highlight one cell per row (rows 2-7) in
  the winning path using CLUT color changes.
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
- IF a piece has no legal moves, THEN THE SYSTEM SHALL prevent it from being selected.

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
- THE SYSTEM SHALL provide a Settings menu accessible via long-press or
  right-click on any menu icon, offering options for color schemes, AI move
  explanations, keyboard shortcuts display, and animation speed.
- THE SYSTEM SHALL display context-sensitive tooltips for all menu icons when
  hovered for more than 1 second, showing the action name and keyboard shortcut.
- THE SYSTEM SHALL remember user preferences (difficulty, color scheme, AI
  explanations) within the current session and apply them to new games.

### Move History and Scoring

- WHEN a move completes, THE SYSTEM SHALL append an algebraic notation entry to
  the move history and scroll older entries as needed.
- WHEN a player wins, THE SYSTEM SHALL increment their session score and highlight the winning path cells (one per row, rows 2-7) using CLUT color changes until the game is reset.
- WHEN a game is initialized or a player wins, THE SYSTEM SHALL display the session score as a bitmap graphic positioned under the last menu item as `W: (white wins) B: (black wins)`.
- WHEN the session resets, THE SYSTEM SHALL clear the move history unless a tournament mode is introduced in future updates.

### Artificial Intelligence

- WHEN it is the AI player's turn, THE SYSTEM SHALL enumerate every legal
  empty-cell and swap move for the active player using the configured swap rule
  and supply those moves to the search engine.
- WHEN the AI explores the game tree, THE SYSTEM SHALL execute a deterministic
  iterative deepening negamax search that evaluates at least four plies and
  extends depth when the position contains an immediate win or loss threat.
- WHEN a simulated move results in a win for either side, THE SYSTEM SHALL stop
  expanding that branch and return the evaluated score to the caller to ensure
  forced wins and losses are detected.
- WHEN the search budget in nodes or milliseconds is exhausted before
  completing the intended depth, THE SYSTEM SHALL return the best move from the
  deepest fully evaluated iteration.
- WHEN evaluating a board, THE SYSTEM SHALL combine connection progress, bridge
  potential, swap pressure, blocking coverage, and mobility features using
  signed 16-bit arithmetic that respects rule-specific weight tables.
- WHEN the swap rule changes, THE SYSTEM SHALL load the associated evaluation
  weights and swap-clearing behaviour so that move selection reflects the
  current rule set.
- WHEN ordering candidate moves, THE SYSTEM SHALL prioritise immediate wins,
  double threats, central advances, blocking replies, and remaining moves in
  that sequence to improve alpha-beta efficiency.
- WHEN transposition caching is enabled, THE SYSTEM SHALL store up to 64 recent
  board positions using Zobrist hashing and reuse cached scores and principal
  variations for subsequent searches.
- WHEN operating at Learning or Easy difficulty levels, THE SYSTEM SHALL reduce
  the maximum search depth or feature weights to honour the selected profile
  while preserving deterministic move choice.
- WHEN evaluation diagnostics are requested, THE SYSTEM SHALL produce a
  breakdown of feature contributions for the chosen move so tuning can be
  reviewed without altering search determinism.
- WHEN fewer than four of the rows between 2 and 7 inclusive contain at least
  one piece from either player, THE SYSTEM SHALL choose AI moves via
  single-ply heuristic evaluation without invoking recursive search.
- WHEN exactly four of the rows between 2 and 7 inclusive contain at least one
  piece from either player, THE SYSTEM SHALL cap the AI search depth at two
  plies and skip iterative deepening win probes.
- WHEN five or more of the rows between 2 and 7 inclusive contain at least one
  piece from either player, THE SYSTEM SHALL enable the configured deep-search
  depth and win detection routines for the AI player.
- WHEN AI diagnostics are enabled, THE SYSTEM SHALL reset hardware timer0
  before AI move selection begins and display the elapsed timer ticks together
  with node counts using `print_formatted_text` after the move is chosen.
- WHEN the active player has twelve or more legal moves available, THE SYSTEM
  SHALL cap the recursive search depth to at most two plies for that turn.
- WHEN the active player has eighteen or more legal moves available, THE SYSTEM
  SHALL select a move using single-ply heuristics without invoking recursive
  search.
- WHEN evaluating a board position, THE SYSTEM SHALL treat positions where the
  current player can win in one move as winning for that player, ensuring the
  AI avoids moves that allow opponent immediate wins unless all legal moves
  permit such wins.
- WHEN selecting moves via heuristic evaluation, THE SYSTEM SHALL filter out
  AI moves that result in an immediate win for the opponent.

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
- THE SYSTEM SHALL provide volume control accessible via keyboard shortcuts
  (+ and - keys) with visual feedback showing current volume level.
- THE SYSTEM SHALL support audio muting via the M key, with a visual indicator
  when audio is disabled.
- WHEN hovering over pieces or menu items, THE SYSTEM SHALL play subtle audio
  feedback to enhance the tactile feel of the interface.

### Assets and Theming

- WHEN the build pipeline executes, THE SYSTEM SHALL generate placeholder
  sprite assets measuring 24x24 pixels for pieces and 16x16 pixels for menu
  icons so rendering features can be validated before final art delivery.
- WHEN the build pipeline executes, THE SYSTEM SHALL programmatically produce
  the 320x240 board bitmap using the active UI theme color lookup table to
  maintain consistency across themes.
- THE SYSTEM SHALL expose at least three color lookup table themes (default,
  high contrast, colorblind) that can be selected by the video subsystem to
  support accessibility.
- THE SYSTEM SHALL reserve palette index 0 for transparency and ensure that
  generated bitmap assets and runtime CLUT updates avoid assigning visible
  colors to that index.
- WHEN the video subsystem initializes, THE SYSTEM SHALL upload the generated
  placeholder bitmap and sprite assets into VICKY VRAM so hardware tests can
  exercise populated bitmap and sprite layers.

## Non-Functional Requirements

### Performance

- THE SYSTEM SHALL maintain at least 30 frames per second during board
  rendering transitions on the Foenix F256K2 hardware.
- THE SYSTEM SHALL compute AI moves within 500 milliseconds under the
  "Standard" difficulty profile.
- THE SYSTEM SHALL implement frame-rate adaptive rendering, reducing animation
  complexity if frame rate drops below 25 FPS for more than 3 consecutive frames.
- THE SYSTEM SHALL use sprite culling to avoid rendering off-screen elements
  and batch sprite updates to minimize video memory transfers.
- THE SYSTEM SHALL precompute and cache winning path connectivity matrices
  during initialization to accelerate victory detection during gameplay.
- THE SYSTEM SHALL implement progressive AI evaluation, displaying intermediate
  move candidates if search exceeds 250ms to maintain responsiveness.

### Memory Management

- WHEN the AI search routine starts on Foenix hardware builds, THE SYSTEM SHALL
  load the AI overlay image into the reserved 0xA000 execution workspace before
  evaluating moves so the resident RAM segment remains within the 48 KB limit.
- WHEN the AI overlay image is resident, THE SYSTEM SHALL reuse the loaded
  workspace for subsequent searches instead of duplicating the copy operation
  to preserve headroom for the software stack and global data.

### Reliability

- THE SYSTEM SHALL recover gracefully to the main menu when detecting sprite or
  bitmap initialization failures by reloading assets from `f256lib` services.
- THE SYSTEM SHALL validate input buffers to avoid corruption when switching
  between mouse and keyboard controls.
- WHEN the AI engine encounters an infinite loop or exceeds maximum evaluation
  time, THE SYSTEM SHALL abort the current search and select a random legal move
  while logging the error condition.
- IF memory allocation fails during gameplay, THE SYSTEM SHALL attempt to free
  non-critical resources (audio buffers, move history beyond 10 entries) and
  continue with reduced functionality.
- WHEN hardware registers return unexpected values, THE SYSTEM SHALL reinitialize
  the affected subsystem once per session and fallback to software rendering
  if reinitialization fails.

### Usability

- THE SYSTEM SHALL provide consistent color-coded highlights for selectable
  pieces, legal moves, and winning paths for accessibility.
- THE SYSTEM SHALL provide clear textual feedback for difficulty level, current
  turn, and prompts.
- WHEN the AI is evaluating moves, THE SYSTEM SHALL display a thinking indicator
  (animated sprite or progress bar) to inform the user of system activity.
- THE SYSTEM SHALL provide keyboard shortcuts for all menu functions (R=Reset,
  I=Info, D=Difficulty, S=Starting Board, H=History, X=Exit) displayed in menu
  tooltips.
- THE SYSTEM SHALL support colorblind-friendly palette options accessible via
  a configuration overlay, with at least two alternative color schemes.
- WHEN displaying move history, THE SYSTEM SHALL use clear algebraic notation
  with from->to coordinates (e.g., "A1->B2 swap", "C3->C4 move") for clarity.
- THE SYSTEM SHALL provide undo functionality for the human player's last move
  during their turn, disabled once the AI begins evaluation.

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
- THE SYSTEM SHALL highlight one cell per row (rows 2-7) in the winning path using CLUT color changes.
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
