# F256 Switcharoo Game

## Objective

Create a game for the foenix f256k2 system using llvm-mos that allows a human player to play against a heuristic AI agent.

## Game Rules

Board: 8 rows , 4 columns.
Players: Player A (White, bottom) vs Player B (Black, top). Player A moves first.
Movement: Move one piece to any adjacent cell (8 directions).
Empty Cell Move: Move to an adjacent empty cell. This unmarks ALL swapped pieces.
Swap Move: Move to an adjacent cell occupied by an opponent's NORMAL piece. Both pieces swap positions and become SWAPPED.
SWAPPED Pieces: Cannot be the target of a swap, but can initiate a swap with a NORMAL opponent piece.
Win Condition: Create a connected path of your pieces linking row 2 and row 7 (inclusive). pieces are considered connected if they are adjacent in any direction, including diagonally.
Swap Rules: The game may be played in one of four swap rule modes, which can be selected by the player.
- Classic: Moving into an empty cell clears all swapped pieces. This is the default mode described above
- Clears Own: Moving into an empty cell clears the player's swapped pieces. Opponent's pieces remain unchanged.
- Swapped Clears: Moving a swapped piece into an empty cell clears all pieces. Moving a normal piece to an empty cell does not clear any swapped pieces.
- Swapped Clears Own: Moving a swapped piece into an empty cell clears only the player's swapped pieces. Opponent's pieces remain unchanged. Moving a normal piece to an empty cell does not clear any swapped pieces.


## User Interface
Screen shows a centered game board with the human player's white pieces positioned at the bottom of the screen.
To the right of the screen is a vertical icon based menu
- Reset: reinitializes the game board
- Information: displays the game rules
- Difficulty: selects AI difficulty
- Starting Board: selects from a set of piece starting positions
- Move History: displays a list of game moves with the most recent move listed at the top of the list.
- Exit: prompts the user for confirmation and if confirmed, exits the program
- Score: displays the wins for each player since the start of the session.
User may select a piece using a mouse click Or by using arrow keys to highlight a piece and use enter to select the piece
When a piece is selected all available moves are highlighted
A selected piece may be deselected so another piece can be selected
When a piece is selected, one of the available move positions can be selected by the user using the mouse or keyboard which will execute the move of the piece.
When using a mouse both the game pieces and menu icons will use a hover display style when the mouse is positioned over the display element.
When a player makes a winning move the cells of the winning path are highlighted.

## Platform Design Considerations
Platform supports bitmap, tile and sprite based graphics on a 320x240 pixel screen.  Text can be overlayed on top of the graphics screen
Graphics layer priority: sprite layer 0, bitmap/tile layer 0, sprite layer 1, bitmap/tile layer 1, sprite layer 2, bitmap/tile layer 2
The Game Board will be rendered as a bitmap.  The game board cell will be 28x28 pixels so that the game piece sprite can be centered in a game board cell.  game board cells will separated by a single pixel black border and the entire game board will have a single pixel black border
Game pieces will be 24x24 pixel sprites
Icon Menu will be positioned to the right of the game board and will use vertically positioned 16x16 sprites for each menu element.
Text display for move history and information will overlay the graphics display.  Users will be able to click with a mouse or use a defined 'escape' key to dismiss the text display overlay and return to the game.

## AI Agent Heuristics
objective is to have a connected path of pieces between rows 2 and 7, inclusive
positive heuristic elements:  number of rows occupied between 2 and 7, number of connected rows between 2 and 7, number of swapped pieces (which limits opponent legal move options)
negative heuristic elements: number of pieces on 'back' row, opponent connected pieces between rows 2 and 7
win heuristics: move creates an immediate win for the player, move forces the opponent to make a move that allows the player to win on their subsequent move
lose avoidance heuristic: move creates an immediate win for the opponent player, move allows the opponent to make a winning move.
advanced loss avoidance heuristic:  move allows the opponent to create a forcing move which forces the player to make a move on their next move that allows the opponent to win on their following move.
the AI will use the selected Swap Rule to determine legal moves and to select optimal play.



