# Sprite Color and Positioning Fix

## Issue Identified

The sprites were displaying with gray colors instead of the intended modern color theme because of a **CLUT index mismatch** between the asset generator and the video initialization code.

### Root Cause

**Asset Generator (`generate_assets.py`)** - Correct per video_assets.md:
- Player A piece colors: CLUT indices **65-68, 73-74**
- Player B piece colors: CLUT indices **69-72, 75-76**
- Icon colors: CLUT indices **77-84**

**Video Code (`video.c`)** - INCORRECT indices:
- Player A piece colors: indices 38-43 ❌
- Player B piece colors: indices 44-49 ❌
- Icon colors: indices 50-57 ❌

Since the sprite bitmaps referenced indices 65-84 but the palette was only defining colors at 38-57, the sprites were picking up undefined/gray values.

## Fixes Applied

### 1. Corrected CLUT Index Definitions in video.c

Updated all sprite color CLUT index definitions to match the specification in `video_assets.md`:

```c
// Player A piece colors (indices 65-68, 73-74) - per video_assets.md
#define VIDEO_CLUT_PLAYER_A_EDGE_1 65
#define VIDEO_CLUT_PLAYER_A_EDGE_2 66
#define VIDEO_CLUT_PLAYER_A_FILL_1 67
#define VIDEO_CLUT_PLAYER_A_FILL_2 68
#define VIDEO_CLUT_PLAYER_A_SWAPPED_1 73
#define VIDEO_CLUT_PLAYER_A_SWAPPED_2 74

// Player B piece colors (indices 69-72, 75-76) - per video_assets.md
#define VIDEO_CLUT_PLAYER_B_EDGE_1 69
#define VIDEO_CLUT_PLAYER_B_EDGE_2 70
#define VIDEO_CLUT_PLAYER_B_FILL_1 71
#define VIDEO_CLUT_PLAYER_B_FILL_2 72
#define VIDEO_CLUT_PLAYER_B_SWAPPED_1 75
#define VIDEO_CLUT_PLAYER_B_SWAPPED_2 76

// Icon colors (indices 77-84) - per video_assets.md
#define VIDEO_CLUT_ICON_EDGE_1 77
#define VIDEO_CLUT_ICON_EDGE_2 78
#define VIDEO_CLUT_ICON_FILL_1 79
#define VIDEO_CLUT_ICON_FILL_2 80
#define VIDEO_CLUT_ICON_SYMBOL_1 81
#define VIDEO_CLUT_ICON_SYMBOL_2 82
#define VIDEO_CLUT_ICON_SYMBOL_3 83
#define VIDEO_CLUT_ICON_SYMBOL_4 84
```

### 2. Updated Color Palette to Modern Theme

Changed from gray placeholder colors to vibrant modern theme:

**Player A (Blue Theme):**
- Edge colors: `#4A90E2` (light blue), `#2171B5` (dark blue)
- Fill colors: `#3498DB`, `#2980B9` (blue shades)
- Swapped symbol: `#FFFFFF`, `#F0F0F0` (white/light gray)

**Player B (Purple Theme):**
- Edge colors: `#9B59B6` (light purple), `#8E44AD` (dark purple)
- Fill colors: `#7D3C98`, `#6C3483` (purple shades)
- Swapped symbol: `#FFFFFF`, `#F0F0F0` (white/light gray)

**Icons (Orange Theme):**
- Edge colors: `#E67E22`, `#D35400` (orange shades)
- Fill colors: `#F39C12`, `#E67E22` (orange/gold)
- Symbol colors: `#F1C40F` (yellow), `#F39C12`, `#E67E22`, `#D35400` (orange gradient)

### 3. Improved Sprite Positioning for Better Centering

Enhanced the sprite positioning logic to ensure pieces are perfectly centered within board cells:

```c
// Calculate centering offset for pieces within cells
// Cell is 28x28, piece is 24x24, so offset by (28-24)/2 = 2 pixels
const uint16_t cell_offset = (VIDEO_BOARD_CELL_SIZE - VIDEO_PIECE_SPRITE_SIZE) / 2;

// Account for 4-pixel border before first cell
const int16_t first_cell_x = board_x + 4;
const int16_t first_cell_y = board_y + 4;

// For each piece:
uint16_t cell_x = first_cell_x + (col * (CELL_SIZE + 1));  // +1 for cell border
uint16_t cell_y = first_cell_y + (row * (CELL_SIZE + 1));
uint16_t piece_x = cell_x + cell_offset;  // Center within cell
uint16_t piece_y = cell_y + cell_offset;
```

## Expected Results

After this fix:
- ✅ Player A pieces display in **blue tones** with white star symbols when swapped
- ✅ Player B pieces display in **purple tones** with white diamond symbols when swapped
- ✅ Icon sprites display in **orange/yellow** theme
- ✅ All sprites are properly **centered** within their board cells
- ✅ Sprite details (edges, fills, symbols) are now **visible and distinct**

## Files Modified

- `src/video.c` - Fixed CLUT indices, color definitions, and sprite positioning
- Built successfully: `f256_switch.pgz` (85,313 bytes)

## Verification

View the generated PNG assets in `assets/generated/png/` to see the intended color scheme that should now display correctly on the F256.
