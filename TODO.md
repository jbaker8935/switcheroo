1. Make sure swapping status is cleared on reset of game or swiching of puzzle
2. fix flickering sprites (reset is flickering)
3. Only redefine sprites when their status changes.
4. Implement Wait for VBL when redefining sprites.
5. Define the Text Screen Layout and provide output requirements
- Current Session W/L Score
- Current Player's Move
- Current Rule
- Game Mode: Puzzle or Play Against AI
- Puzzle Mode
- Puzzle Difficulty
- Puzzle Status: solved (only solved if win in N or less), unsolved
- Puzzle Hint - Example Solution
- Game History - last move at top
6. Rethink Icons
- set puzzle or game play mode
- reset current board
- change current rule
- select next puzzle
- display puzzle hint
- display game rules
- exit game
7. Add keyboard mode
- select cell or icon with cursor
- use enter to select icon, piece or to move to new destination
- use keyboard shortcuts for icons
8. Encourage AI to advance more pieces to connected positions in the early game



