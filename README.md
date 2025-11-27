
## F256 Switcharoo

A strategic puzzle game for the **Foenix/Wildbits F256** retro computer, built with **llvm-mos**. Play against a heuristic Engine opponent in this unique piece-swapping strategy game.

## Game Overview

Switcharoo is a two-player abstract strategy game where players compete to create a connected path of their pieces across the board.

### Board and Players

- **Board**: 8 rows × 4 columns
- **Players**: White (human, bottom) vs Black (Engine, top)
- **Objective**: Create a connected path of your pieces linking rows 2 through 7

### Movement Rules

- **Empty Cell Move**: Move one piece to any adjacent cell (8 directions). This may clear swapped pieces depending on the active swap rule.
- **Swap Move**: Move to an adjacent cell occupied by an opponent's NORMAL piece. Both pieces exchange positions and become SWAPPED.
- **Swapped Pieces**: Cannot be the target of a swap, but can initiate swaps with NORMAL opponent pieces.

### Swap Rule Variants

| Mode | Behavior |
|------|----------|
| Classic | Moving into an empty cell clears ALL swapped pieces |
| Clears Own | Moving into an empty cell clears only the mover's swapped pieces |
| Swapped Clears | Only swapped pieces moving to empty cells trigger the clear |
| Swapped Clears Own | Swapped pieces moving to empty clear only the mover's pieces |

## Features

- **Puzzle Mode**: Solve pre-designed puzzles with optimal move sequences
- **Free Play Mode**: Open gameplay against the Engine with multiple starting layouts
- **Engine Difficulty Levels**: Adjustable Engine strength from easy to advanced
- **Achievements System**: Track your progress and accomplishments
- **Move History**: Review and navigate through past moves
- **Mouse & Keyboard Support**: Full input support for both control methods
- **Audio**: Sound effects for moves, wins, and losses

## Building

### Prerequisites


- [llvm-mos for f256](https://kangaroopunch.com/view/ShowSoftware?id=13) toolchain 
- Foenix F256 development environment (`f256build.sh`)

### Build Command

```bash
./build.sh
```

This wraps the `f256build.sh` script from the llvm-mos F256 development environment.

### Output

The build produces `switcheroo.pgz` in the `project root` directory, ready to load on the F256.
Game should run on core1x or core2x gen1 and gen2 f256 hardware. 

## Running on Emulator
1. Copy `switcheroo.pgz` to the emulated SD Card
2. Load the program through the command prompt.

NOTE: When run on the Foenix IDE use the spacebar to progress the Splash and Exit screens.  The SID sound
routine on those screens requires a timer, which is not working in the IDE.  Playback can be bypassed by selecting the
space key.

## Running on Hardware

1. Copy `switcheroo.pgz` to your F256 SD card
2. Boot the F256 and load the program

## Host Testing

For rapid iteration and debugging, the Engine can be tested on a Linux host:

```bash
gcc -o tests/ai_agent_tests_host \
    -I. -I./src \
    -DAI_AGENT_HOST_TEST=1 \
    -std=c99 -Wall -Wextra -g \
    tests/ai_agent_tests.c \
    tests/stubs.c \
    src/board.c \
    src/ai_agent.c

./tests/ai_agent_tests_host
```

See [HOST_TEST_BUILD_GUIDE.md](HOST_TEST_BUILD_GUIDE.md) for detailed instructions.

## Project Structure

```
f256_switch/
├── src/                    # Main source code
│   ├── main.c              # Application entry point
│   ├── game_state.c/.h     # Game phase and state management
│   ├── board.c/.h          # Board representation and move logic
│   ├── ai_agent.c/.h       # Heuristic AI opponent
│   ├── render.c/.h         # Sprite and graphics rendering
│   ├── input.c/.h          # Mouse and keyboard input
│   ├── puzzle_data.c/.h    # Puzzle loading and management
│   ├── achievements.c/.h   # Achievement tracking
│   └── ...                 # Additional modules
├── tests/                  # Host-side test harness
├── assets/                 # Graphics, sounds, and puzzle data
│   ├── aseprite/           # Sprite source files
│   ├── sounds/             # Audio assets
│   └── puzzle_json/        # Puzzle definitions
├── scripts/                # Build and asset conversion tools
├── docs/                   # Additional documentation
├── requirements.md         # Functional requirements (EARS notation)
├── design.md               # Architecture and design documentation
└── tasks.md                # Implementation task tracking
```

## Architecture

The game is organized into distinct layers:

| Layer | Responsibility | Key Modules |
|-------|----------------|-------------|
| Application Shell | Startup, event loop, shutdown | `main.c`, `system.c` |
| Game State | Phase control, menu, puzzle transitions | `game_state.c` |
| Board & Rules | Move validation, swap rules, win detection | `board.c` |
| Engine | Heuristic move selection, difficulty tuning | `ai_agent.c` |
| Presentation | Sprites, highlights, text HUD | `render.c`, `video.c` |
| Input | Mouse/keyboard translation | `input.c`, `input_handler.c` |

### Memory Constraints

- RAM budget: ~48 KB outside Engine overlay window
- Assets and puzzle data stream from far memory
- Minimal dependencies on `f256lib` primitives for portability

## Engine Heuristics

The Engine evaluates moves using multiple factors:

**Connection Progress**
- Rows occupied in the victory span (rows 2-7)
- Number of connected rows in the victory span

**Bridge Potential**
- Empty cells available for path expansion

**Swap Pressure**
- Swapped pieces restrict opponent movement options (cannot be swap targets)

**Blocking Coverage**
- Pieces in center columns of victory rows
- Adjacency for movement restriction and further swap opportunities

**Mobility**
- Total legal moves available

**Win/Loss detection:**
- Immediate win detection
- Forced win sequences (win-in-2, win-in-3)
- Loss avoidance and forcing move recognition

## Documentation

- [requirements.md](requirements.md) - Functional requirements in EARS notation
- [design.md](design.md) - Technical architecture and module design
- [tasks.md](tasks.md) - Implementation task tracking
- [HOST_TEST_BUILD_GUIDE.md](HOST_TEST_BUILD_GUIDE.md) - Host testing setup

## Controls

### Keyboard

- **Arrow Keys**: Navigate board/menu
- **Enter**: Select piece or confirm move
- **Escape**: Deselect piece or dismiss overlay

### Mouse

- **Left Click**: Select pieces and execute moves
- **Hover**: Visual feedback on interactive elements

## License

See repository for license information.

## Acknowledgments

- Built for the [Foenix F256](https://wiki.f256foenix.com/index.php?title=Main_Page) retro computer
- Compiled with [llvm-mos](https://github.com/llvm-mos/llvm-mos-sdk)
