# Graphics Assets

## Graphics Color Lookup table
- For this application clut 0 will be used.
- The application will use clut color entries to apply themes and to display special colors to the game board cells
- Clut slot 0 is reserved for transparency
- Clut slots 1 through 32 are dedicated color slots for game board cells.  each game board cell is assigned a unique clut slot so that colors for a cell can be adjusted independently
- Clut slot 33 is reserved for the game board border lines
- Clut slots 34-64 are reserved for future use
- Clut slot 65 Player A Piece Sprite Edge Color 1
- Clut slot 66 Player A Piece Sprite Edge Color 2
- Clut slot 67 Player A Piece Sprite Fill Color 1
- Clut slot 68 Player A Piece Sprite Fill Color 2
- Clut slot 69 Player B Piece Sprite Edge Color 1
- Clut slot 70 Player B Piece Sprite Edge Color 2
- Clut slot 71 Player B Piece Sprite Fill Color 1
- Clut slot 72 Player B Piece Sprite Fill Color 2
- Clut slot 73 Player A Piece Sprite Swapped Symbol Color 1
- Clut slot 74 Player A Piece Sprite Swapped Symbol Color 2
- Clut slot 75 Player B Piece Sprite Swapped Symbol Color 1
- Clut slot 76 Player B Piece Sprite Swapped Symbol Color 2
- Clut slot 77 Icon Sprite Edge Color 1
- Clut slot 78 Icon Sprite Edge Color 2
- Clut slot 79 Icon Sprite Fill Color 1
- Clut slot 80 Icon Sprite Fill Color 2
- Clut slot 81 Icon Sprite Symbol Color 1
- Clut slot 82 Icon Sprite Symbol Color 2
- Clut slot 83 Icon Sprite Symbol Color 3
- Clut slot 84 Icon Sprite Symbol Color 4
- Clut slot 85 Highlight Sprite Empty Cell Color 1
- Clut slot 86 Highlight Sprite Empty Cell Color 2
- Clut slot 87 Highlight Sprite Occupied Cell Color 1
- Clut slot 88 Highlight Sprite Occupied Cell Color 2
- Clut slot 89 Focus Color

## Sprites
- 
## Piece Sprite
- Piece Sprites are defined on Sprite Layer 1
- 24x24 pixels
- two sprite bitmaps are defined for each player
- - normal piece bitmap
- - swapped piece bitmap
- sprite bitmaps use the reserved Piece Clut slot values defined in the Graphics Color Lookup Table
- 16 piece sprites are defined, 8 for each player
- generated assets to be used during testing
- - piece sprite should have a 1 pixel transparent border
- - piece sprite should have a 2 pixel edge border
- - piece sprite should have a color fill
- - swapped piece sprite should have an inset symbol, e.g. star shaped

## Highlight Sprites
- Highlight Sprites are defined on Sprite Layer 0
- 24x24 pixels
- Two bit maps required for highlight sprites
- - one bitmap for Highlight Sprites used on an Empty Cell
- - one bitmap for Highlight Sprites used on an Occupied Cell
- Eight highlight sprites for Empty Cells are defined representing the 8 possible movement directions from a piece position
- Eight highlight sprites for Occupied Cells are defined representing the 8 possible movement directions from a piece position
- Highlight sprites placed over empty cells use a different color slot than a highlight sprite placed over an opponent's piece that is a candidate for swapping.
- Highlight Sprite bitmaps use the reserved Piece Clut slot values defined in the Graphics Color Lookup Table
- - bitmaps for Empty Cells use the Clut indices for Highlight Sprite Empty Cell Color
- - bitmaps for Occupied Cells use the Clut indices for Highlight Sprite Occupied Cell Color

- generated assets to be used during testing
- - highlight sprite should have a 8 pixel transparent border
- - highlight sprite should have a 1 pixel edge border
- - highlight sprite should have a color fill

## Piece Focus Sprite
- Piece Focus Sprites are defined on Sprite Layer 0
- 24x24 pixels
- one bitmaps 
- focus sprite bitmaps use the reserved Focus Clut slot values defined in the Graphics Color Lookup Table
- one Piece Focus Sprite is defined
- generated assets to be used during testing
- - piece focus sprite should have a 1 pixel white dashed line border
- - the interior of the focus sprite will be transparent

## Icon Focus Sprite
- Icon Focus Sprites are defined on Sprite Layer 0
- 16x16 pixels
- one bitmaps 
- focus sprite bitmaps use the reserved Focus Clut slot values defined in the Graphics Color Lookup Table
- one Icon Focus Sprite is defined
- generated assets to be used during testing
- - icon focus sprite should have a 1 pixel white dashed line border
- - the interior of the focus sprite will be transparent


## Icon Sprite
- Icon Sprites are defined on Sprite Layer 1
- 16x16 pixels
- sprite bitmap defined for each Icon
- - Reset: reinitializes the game board
- - Information: displays the game rules
- - Difficulty: selects AI difficulty
- - Starting Board: selects from a set of piece starting positions
- - Move History: displays a list of game moves with the most recent move listed at the top of the list.
- - Exit: prompts the user for confirmation and if confirmed, exits the program
- generated assets to be used during testing
- - icon sprite should have a 2 pixel edge border
- - each icon sprite should have unique symbology for test purposes
- Icon Sprites use the Color Lut slot values defined in the Graphics Color Lookup Table
## Board Bitmap
- Board Bitmap is defined on Bitmap Layer 2
- a single 320x240 bitmap bill be used
- bitmap will be located at high-memory address 0x44000
- bitmap memory  Page 2 - 0x44000 → 0x56bff
- bitmap will have a centered 4 col x 8 row checkerboard game board
- each cell will be 28x28 and will be assigned a unique clut slot as defined in Graphics Color Lookup Table
- each cell will be divided by a single pixel border using clut slot 33
- the gameboard will have a 4 pixel border using clut slot 33
- generated asset for testing
- - use the board bitmap description to define a bitmap for testing
- - use the EMBED function to locate the bitmap at 0x44000

## Color Lookup Table Use Cases
- Color Slots 1-32 are used to define the checkboard pattern of the game board and to present the winning path to the user.  The application will populate the CLUT slot entries as appropriate
- Color Slots 65-76 are used for setting color themes for Piece Sprites
- Color Slots 77-84 are used for setting color themes for Icon Sprites, including enabled and disabled Icon states