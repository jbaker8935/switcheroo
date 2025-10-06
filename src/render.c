/**
 * @file render.c
 * @brief Rendering pipeline implementation
 */

#include "../src/render.h"
#include "../src/input.h"
#include "../src/board.h"
#include <string.h>

// External functions from video.c
extern void video_reset_board_cell_color(uint8_t row, uint8_t col);
extern void video_set_board_cell_win_color(uint8_t row, uint8_t col, player_t player);
extern void video_reset_all_board_cell_colors(void);

// Constants from video.c (should eventually be in a shared header)
#define VIDEO_SCREEN_WIDTH 320u
#define VIDEO_SCREEN_HEIGHT 240u
#define VIDEO_BOARD_CELL_SIZE 28u
#define VIDEO_PIECE_SPRITE_SIZE 24u
#define VIDEO_ICON_SPRITE_SIZE 16u
#define VIDEO_BOARD_COLUMNS 4u
#define VIDEO_BOARD_ROWS 8u

#define VIDEO_SPRITE_PIECE_BASE 0u
#define VIDEO_SPRITE_ICON_BASE 16u
#define VIDEO_SPRITE_HIGHLIGHT_BASE 22u  // After 6 icons
#define VIDEO_SPRITE_OFFSET 32u

#define VIDEO_VRAM_PIECE_A_NORMAL 0x56c00u
#define VIDEO_VRAM_PIECE_A_SWAPPED 0x58000u
#define VIDEO_VRAM_PIECE_B_NORMAL 0x59400u
#define VIDEO_VRAM_PIECE_B_SWAPPED 0x5a800u

#define VIDEO_VRAM_ICON_RESET 0x5bc00u
#define VIDEO_VRAM_ICON_INFO 0x5c400u
#define VIDEO_VRAM_ICON_DIFFICULTY 0x5cc00u
#define VIDEO_VRAM_ICON_STARTING_BOARD 0x5d400u
#define VIDEO_VRAM_ICON_HISTORY 0x5dc00u
// ICON_EXIT was moved; reference via video constants
#define VIDEO_VRAM_ICON_EXIT 0x5e400u

#define VIDEO_PRIMARY_CLUT 0

// Highlight sprite CLUT indices (defined in video.c)
#define VIDEO_VRAM_HIGHLIGHT 0x5e500u
#define VIDEO_VRAM_HIGHLIGHT_EMPTY 0x5e500u
#define VIDEO_VRAM_HIGHLIGHT_OCCUPIED 0x5e800u
#define VIDEO_CLUT_HIGHLIGHT_PRIMARY 35
#define VIDEO_CLUT_HIGHLIGHT_SECONDARY 36
// Highlight sprite CLUT slots (distinct from board highlight CLUTs)
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_PRIMARY 85
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_SECONDARY 86
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY 87
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_SECONDARY 88

// Focus CLUT/Vram (mirrors video.c defs used by render)
#define VIDEO_CLUT_FOCUS 89
#define VIDEO_VRAM_FOCUS_PIECE 0x5ec00u
#define VIDEO_VRAM_FOCUS_ICON 0x5ee00u

// Icon bitmap addresses
static const uint32_t s_icon_bitmap_addrs[6] = {
    VIDEO_VRAM_ICON_RESET,
    VIDEO_VRAM_ICON_INFO,
    VIDEO_VRAM_ICON_DIFFICULTY,
    VIDEO_VRAM_ICON_STARTING_BOARD,
    VIDEO_VRAM_ICON_HISTORY,
    VIDEO_VRAM_ICON_EXIT
};

// Board layout (calculated once)
static int16_t s_board_x;
static int16_t s_board_y;
static int16_t s_icon_x;
static int16_t s_icon_start_y;

// Cached render state to avoid unnecessary sprite redefinitions/positioning
static bool s_cache_initialized = false;
static uint8_t s_cache_board_snapshot[BOARD_ROWS][BOARD_COLS];
// Cached selection state to avoid per-frame highlight updates
static bool s_cache_selection_has = false;
static uint8_t s_cache_selected_row_val = 0xFF;
static uint8_t s_cache_selected_col_val = 0xFF;
static uint8_t s_cache_legal_move_count = 0;
static move_t s_cache_legal_moves[8];

// Track whether a winning-path CLUT has been applied (avoids repeated CLUT writes)
static bool s_win_path_applied = false;

// Focus sprite IDs
#define VIDEO_SPRITE_FOCUS_PIECE (VIDEO_SPRITE_HIGHLIGHT_BASE + 16)
#define VIDEO_SPRITE_FOCUS_ICON  (VIDEO_SPRITE_HIGHLIGHT_BASE + 17)

// Helper: snapshot board pieces for change detection
static void cache_board_snapshot(const board_t *board) {
    for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
        for (uint8_t c = 0; c < BOARD_COLS; ++c) {
            s_cache_board_snapshot[r][c] = (uint8_t)board_get_piece(board, r, c);
        }
    }
}

void render_init(void) {
    // Calculate board layout (matching video_position_sprites logic)
    const int16_t board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 11;
    const int16_t board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 15;
    
    s_board_x = (VIDEO_SCREEN_WIDTH - board_width) / 2;
    s_board_y = (VIDEO_SCREEN_HEIGHT - board_height) / 2;

    // (no diagnostic output)
    
    // Icon panel position
    s_icon_x = s_board_x + board_width + 16;
    s_icon_start_y = s_board_y + 8;

    // Initialize and define sprites once (bitmaps + CLUTs). Positions are updated at runtime.
    // Define piece sprites (16)
    for (uint8_t i = 0; i < 16; ++i) {
        uint32_t bitmap = (i < 8) ? VIDEO_VRAM_PIECE_A_NORMAL : VIDEO_VRAM_PIECE_B_NORMAL;
        spriteDefine((uint8_t)(VIDEO_SPRITE_PIECE_BASE + i), bitmap, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
        spriteSetVisible((uint8_t)(VIDEO_SPRITE_PIECE_BASE + i), 0);
    }

    // Define icon sprites (6)
    for (uint8_t i = 0; i < 6; ++i) {
        uint8_t sid = (uint8_t)(VIDEO_SPRITE_ICON_BASE + i);
        spriteDefine(sid, s_icon_bitmap_addrs[i], VIDEO_ICON_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
        spriteSetVisible(sid, 1);
    }

    // Define highlight sprites (8 empty + 8 occupied) - one per direction each.
    // Empty cell highlight sprites: base..base+7 use the EMPTY bitmap
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t sid = (uint8_t)(VIDEO_SPRITE_HIGHLIGHT_BASE + i);
        spriteDefine(sid, VIDEO_VRAM_HIGHLIGHT_EMPTY, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 0);
        spriteSetVisible(sid, 0);
    }
    // Occupied cell highlight sprites: base+8..base+15 use the OCCUPIED bitmap
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t sid = (uint8_t)(VIDEO_SPRITE_HIGHLIGHT_BASE + 8 + i);
        spriteDefine(sid, VIDEO_VRAM_HIGHLIGHT_OCCUPIED, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 0);
        spriteSetVisible(sid, 0);
    }

    // Define focus sprites (piece and icon) on layer 0
    spriteDefine((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, VIDEO_VRAM_FOCUS_PIECE, VIDEO_PIECE_SPRITE_SIZE, VIDEO_CLUT_FOCUS, 0);
    spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);
    spriteDefine((uint8_t)VIDEO_SPRITE_FOCUS_ICON, VIDEO_VRAM_FOCUS_ICON, VIDEO_ICON_SPRITE_SIZE, VIDEO_CLUT_FOCUS, 0);
    spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_ICON, 0);

    // Mark cache initialized
    s_cache_initialized = true;
}

// Force the render cache to be invalidated so next update will re-snapshot
void render_invalidate_cache(void) {
    s_cache_initialized = false;
    s_win_path_applied = false;
}

void render_cell_to_screen(uint8_t row, uint8_t col, uint16_t *x, uint16_t *y) {
    const int16_t first_cell_x = s_board_x + 4;
    const int16_t first_cell_y = s_board_y + 4;

    // Use the board definition: first cell is at (s_board_x + 4, s_board_y + 4).
    // Each cell is VIDEO_BOARD_CELL_SIZE pixels with a 1px separator, so
    // stride = VIDEO_BOARD_CELL_SIZE + 1. Pieces must be inset 2px from the
    // cell origin to leave a 2px margin around a 24x24 sprite in a 28x28 cell.
    const int16_t cell_x = first_cell_x + (col * (VIDEO_BOARD_CELL_SIZE + 1)) + 2;
    const int16_t cell_y = first_cell_y + (row * (VIDEO_BOARD_CELL_SIZE + 1)) + 2;

    if (x) *x = (uint16_t)cell_x;
    if (y) *y = (uint16_t)cell_y;
}

bool render_screen_to_cell(uint16_t x, uint16_t y, uint8_t *row, uint8_t *col) {
    const int16_t first_cell_x = s_board_x + 4;
    const int16_t first_cell_y = s_board_y + 4;
    
    // Check if within board bounds
    if (x < (uint16_t)first_cell_x || y < (uint16_t)first_cell_y) {
        return false;
    }
    
    int16_t rel_x = x - first_cell_x;
    int16_t rel_y = y - first_cell_y;
    
    // Calculate cell accounting for 1-pixel borders
    uint8_t c = (uint8_t)(rel_x / (VIDEO_BOARD_CELL_SIZE + 1));
    uint8_t r = (uint8_t)(rel_y / (VIDEO_BOARD_CELL_SIZE + 1));
    
    if (r >= VIDEO_BOARD_ROWS || c >= VIDEO_BOARD_COLUMNS) {
        return false;
    }
    
    if (row) *row = r;
    if (col) *col = c;
    return true;
}

int8_t render_screen_to_menu_icon(uint16_t x, uint16_t y) {
    const uint16_t icon_spacing = VIDEO_ICON_SPRITE_SIZE + 8;
    
    // Check if x coordinate is in icon area
    if (x < (uint16_t)s_icon_x || x >= (uint16_t)(s_icon_x + VIDEO_ICON_SPRITE_SIZE)) {
        return -1;
    }
    
    // Check y coordinate and determine which icon
    if (y < (uint16_t)s_icon_start_y) {
        return -1;
    }
    
    int16_t rel_y = y - s_icon_start_y;
    int8_t icon = (int8_t)(rel_y / icon_spacing);
    
    if (icon >= 6) {  // Only 6 icons
        return -1;
    }
    
    // Verify we're within the icon bounds (not in spacing gap)
    uint16_t icon_y = s_icon_start_y + (icon * icon_spacing);
    if (y < icon_y || y >= icon_y + VIDEO_ICON_SPRITE_SIZE) {
        return -1;
    }
    
    return icon;
}

void render_update_pieces(const board_t *board) {
    // Track which sprites we've used for each player
    uint8_t white_sprite_count = 0;
    uint8_t black_sprite_count = 0;
    uint8_t unswap_count = 0;  // Track how many pieces changed from swapped to normal

    // If cache not initialized, snapshot board and mark all for update
    bool force_full_update = false;
    // If cache not initialized, snapshot board and force full update so sprites
    // are redefined to match the current board state.
    if (!s_cache_initialized) {
        cache_board_snapshot(board);
        s_cache_initialized = true;
        force_full_update = true;
    }
    
    // Scan board and assign sprites to pieces
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            piece_type_t piece = board_get_piece(board, row, col);
            
            if (piece == PIECE_NONE) {
                continue;  // Empty cell
            }
            
            uint8_t sprite_id = 0;
            uint32_t bitmap_addr = 0;
            bool is_white = false;
            
            // Determine sprite and bitmap based on piece type
            if (piece == PIECE_WHITE_NORMAL) {
                if (white_sprite_count < 8) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + white_sprite_count;
                    bitmap_addr = VIDEO_VRAM_PIECE_A_NORMAL;
                    white_sprite_count++;
                    is_white = true;
                }
            } else if (piece == PIECE_WHITE_SWAPPED) {
                if (white_sprite_count < 8) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + white_sprite_count;
                    bitmap_addr = VIDEO_VRAM_PIECE_A_SWAPPED;
                    white_sprite_count++;
                    is_white = true;
                }
            } else if (piece == PIECE_BLACK_NORMAL) {
                if (black_sprite_count < 8) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + 8 + black_sprite_count;
                    bitmap_addr = VIDEO_VRAM_PIECE_B_NORMAL;
                    black_sprite_count++;
                }
            } else if (piece == PIECE_BLACK_SWAPPED) {
                if (black_sprite_count < 8) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + 8 + black_sprite_count;
                    bitmap_addr = VIDEO_VRAM_PIECE_B_SWAPPED;
                    black_sprite_count++;
                }
            }
            
            if (is_white || black_sprite_count > 0) {
                // Always ensure piece sprites are visible when pieces exist
                spriteSetVisible(sprite_id, 1);
                
                // Always position the sprite when assigned to a piece
                uint16_t x, y;
                render_cell_to_screen(row, col, &x, &y);
                // (no diagnostic output)
                spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + x, VIDEO_SPRITE_OFFSET + y);
                
                // If piece type changed (especially NORMAL<->SWAPPED), redefine sprite with new bitmap
                uint8_t prev_piece = s_cache_board_snapshot[row][col];
                if (force_full_update || prev_piece != (uint8_t)piece) {
                    spriteDefine(sprite_id, bitmap_addr, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
                    
                    // Count swapped->normal transitions
                    if ((prev_piece == PIECE_WHITE_SWAPPED || prev_piece == PIECE_BLACK_SWAPPED) &&
                        (piece == PIECE_WHITE_NORMAL || piece == PIECE_BLACK_NORMAL)) {
                        unswap_count++;
                    }
                }
            }
        }
    }
    
    (void)unswap_count; // Debug-only metric suppressed in release builds
    
    // Hide unused white sprites
    for (uint8_t i = white_sprite_count; i < 8; ++i) {
        spriteSetVisible(VIDEO_SPRITE_PIECE_BASE + i, 0);
    }
    
    // Hide unused black sprites  
    for (uint8_t i = black_sprite_count; i < 8; ++i) {
        spriteSetVisible(VIDEO_SPRITE_PIECE_BASE + 8 + i, 0);
    }

    // Update cache of board pieces after positioning
    cache_board_snapshot(board);
}

void render_update_highlights(const selection_state_t *selection) {
    // Mouse-based selection design:
    // - Select piece by clicking (only when no piece selected)
    // - Deselect by clicking the same piece again
    // - Move by clicking a highlighted legal move cell
    // - Highlights remain visible while selected piece doesn't change
    // - Mouse movement without clicking does not affect selection
    
    // Only update highlights when selection state changes
    bool selection_changed = false;

    if (s_cache_selection_has != selection->has_selection) {
        selection_changed = true;
    } else if (selection->has_selection) {
        if (s_cache_selected_row_val != selection->selected_row || s_cache_selected_col_val != selection->selected_col) {
            selection_changed = true;
        } else if (s_cache_legal_move_count != selection->legal_move_count) {
            selection_changed = true;
        } else {
            // Compare cached moves
            for (uint8_t i = 0; i < selection->legal_move_count; ++i) {
                if (s_cache_legal_moves[i].to_row != selection->legal_moves[i].to_row ||
                    s_cache_legal_moves[i].to_col != selection->legal_moves[i].to_col ||
                    s_cache_legal_moves[i].type != selection->legal_moves[i].type) {
                    selection_changed = true;
                    break;
                }
            }
        }
    }

    if (!selection_changed) {
        // No change — leave highlights alone
        return;
    }

    // Update cache
    s_cache_selection_has = selection->has_selection;
    if (!selection->has_selection) {
        s_cache_selected_row_val = 0xFF;
        s_cache_selected_col_val = 0xFF;
        s_cache_legal_move_count = 0;
        // Hide all highlights
        for (uint8_t i = 0; i < 16; ++i) {
            spriteSetVisible(VIDEO_SPRITE_HIGHLIGHT_BASE + i, 0);
        }
        return;
    }

    s_cache_selected_row_val = selection->selected_row;
    s_cache_selected_col_val = selection->selected_col;
    s_cache_legal_move_count = selection->legal_move_count;
    for (uint8_t i = 0; i < s_cache_legal_move_count && i < 8; ++i) {
        s_cache_legal_moves[i] = selection->legal_moves[i];
    }

    // Clear all highlight sprites first (8 empty + 8 occupied)
    for (uint8_t i = 0; i < 16; ++i) {
        spriteSetVisible(VIDEO_SPRITE_HIGHLIGHT_BASE + i, 0);
    }

    /* highlight_index removed: we use direction-mapped sprites (8 total) */
    uint8_t used_directions = 0u;

    // Highlight all legal move destinations
    for (uint8_t i = 0; i < selection->legal_move_count; ++i) {
        const move_t *move = &selection->legal_moves[i];
        uint16_t move_x, move_y;
        render_cell_to_screen(move->to_row, move->to_col, &move_x, &move_y);

        // Do not highlight a move that targets the currently selected cell
        if (move->to_row == selection->selected_row && move->to_col == selection->selected_col) {
            continue;
        }

        // Compute direction delta from selected cell to move target
        int8_t dr = (int8_t)move->to_row - (int8_t)selection->selected_row;
        int8_t dc = (int8_t)move->to_col - (int8_t)selection->selected_col;
        if (dr < 0) dr = -1; else if (dr > 0) dr = 1; else dr = 0;
        if (dc < 0) dc = -1; else if (dc > 0) dc = 1; else dc = 0;

        // Map (dr,dc) to a direction index 0..7
        uint8_t dir_index = 0;
        if (dr == -1 && dc == 0) dir_index = 0;       // N
        else if (dr == -1 && dc == 1) dir_index = 1;  // NE
        else if (dr == 0 && dc == 1) dir_index = 2;   // E
        else if (dr == 1 && dc == 1) dir_index = 3;   // SE
        else if (dr == 1 && dc == 0) dir_index = 4;   // S
        else if (dr == 1 && dc == -1) dir_index = 5;  // SW
        else if (dr == 0 && dc == -1) dir_index = 6;  // W
        else if (dr == -1 && dc == -1) dir_index = 7; // NW

        // If this direction sprite already used, skip (we only have one per dir)
        if (used_directions & (1u << dir_index)) {
            continue;
        }

        // Choose sprite group based on whether the target is occupied (swap candidate)
        bool target_occupied = (move->type == MOVE_TYPE_SWAP);
        uint8_t group_base = target_occupied ? (VIDEO_SPRITE_HIGHLIGHT_BASE + 8) : VIDEO_SPRITE_HIGHLIGHT_BASE;
        uint8_t move_sprite = group_base + dir_index;
        spriteSetPosition(move_sprite, VIDEO_SPRITE_OFFSET + move_x, VIDEO_SPRITE_OFFSET + move_y);
        spriteSetVisible(move_sprite, 1);

        // Mark the direction used
        used_directions |= (1u << dir_index);
    }

    // Also, ensure piece focus sprite is hidden when selection is active (mouse selection takes precedence)
    spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);
}

// Called from render_update to show focus indicator when keyboard mode is active
static void render_update_focus(void) {
    // Query input focus
    uint8_t row, col;
    input_get_focus(&row, &col);
    if (!input_is_keyboard_mode()) {
        // Hide both focus sprites
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_ICON, 0);
        return;
    }

    // Determine whether focus is on board cell or icon area.
    // If focus row within board rows, show piece focus; otherwise show icon focus.
    if (row < VIDEO_BOARD_ROWS) {
        // Show piece focus at focused cell
        uint16_t x, y;
        render_cell_to_screen(row, col, &x, &y);
        spriteSetPosition((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, VIDEO_SPRITE_OFFSET + x, VIDEO_SPRITE_OFFSET + y);
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 1);
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_ICON, 0);
    } else {
        // Map focus to icon index (col used for icon index)
        uint8_t icon_index = col % 6; // defensive
        uint16_t y = s_icon_start_y + (icon_index * (VIDEO_ICON_SPRITE_SIZE + 8));
        spriteSetPosition((uint8_t)VIDEO_SPRITE_FOCUS_ICON, VIDEO_SPRITE_OFFSET + s_icon_x, VIDEO_SPRITE_OFFSET + y);
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_ICON, 1);
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);
    }
}

void render_update_win_path(const win_path_t *path) {
    // If there's no path, ensure board colors are normal and clear the applied flag
    if (!path || !path->has_path) {
        if (s_win_path_applied) {
            video_reset_all_board_cell_colors();
            s_win_path_applied = false;
        }
        return;
    }

    // If we've already applied the win path previously, do nothing to avoid repeated CLUT writes
    if (s_win_path_applied) return;

    // Apply win highlight colors once
    for (uint8_t i = 0; i < path->path_length; ++i) {
        uint8_t cell_index = path->path_cells[i];
        uint8_t row = cell_index / BOARD_COLS;
        uint8_t col = cell_index % BOARD_COLS;
        video_set_board_cell_win_color(row, col, path->winner);
    }
    s_win_path_applied = true;
}

void render_update_menu(const menu_state_t *menu) {
    // Update icon sprite visibility and appearance based on enabled state
    const uint16_t icon_spacing = VIDEO_ICON_SPRITE_SIZE + 8;
    
    for (uint8_t i = 0; i < 6; ++i) {
        uint8_t sprite_id = VIDEO_SPRITE_ICON_BASE + i;
        uint16_t y = s_icon_start_y + (i * icon_spacing);
        
        // Position icon sprite
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + s_icon_x, VIDEO_SPRITE_OFFSET + y);
        
        // Show all icons, but use different CLUT for disabled vs enabled
        uint8_t clut_index = VIDEO_PRIMARY_CLUT;
        // if (!menu->enabled[i]) {
        //     // Use dimmed CLUT for disabled icons
        //     clut_index = VIDEO_PRIMARY_CLUT + 5;  // Dedicated disabled icon CLUT
        // } else if (menu->selected_icon == (int8_t)i) {
        //     // Highlight selected/hovered icon
        //     clut_index = VIDEO_PRIMARY_CLUT + 6;  // Highlight CLUT
        // }
        
        // Update sprite with appropriate appearance
        uint32_t bitmap_addr = s_icon_bitmap_addrs[i];
        spriteDefine(sprite_id, bitmap_addr, VIDEO_ICON_SPRITE_SIZE, clut_index, 1);
        spriteSetVisible(sprite_id, 1);
    }
}

void render_update_score(const session_stats_t *stats) {
    // TODO: Implement score display
    // 
    // Requirements:
    // - Display format: "W: XX  B: XX" (White vs Black)
    // - Position: Below menu icons in right margin
    // - Options for implementation:
    //   1. Use F256K2 text mode overlay (if available)
    //   2. Create bitmap font sprites for numbers
    //   3. Use tilemap for text rendering
    //   4. Pre-rendered score bitmaps (0-99 combinations)
    //
    // Current stats available:
    // - stats->white_wins (uint8_t)
    // - stats->black_wins (uint8_t)
    //
    // For now, this is a placeholder - scores tracked but not displayed
    (void)stats;  // Suppress unused warning until text rendering implemented
}

void render_update(const game_state_t *state) {
    // Wait for vertical blank to avoid tearing
	while (PEEKW(RAST_ROW_L) < 482u)
		// Spin our wheels.
		;
    
    // Update all rendering based on current game state
    
    // Update piece positions and sprites
    render_update_pieces(&state->board);
    
    // Update highlights for selection
    render_update_highlights(&state->selection);
    // Update focus indicator (keyboard navigation)
    render_update_focus();
    
    // Update winning path if game over
    if (state->phase == GAME_PHASE_GAME_OVER) {
        render_update_win_path(&state->win_path);
    }
    
    // Update menu icons
    render_update_menu(&state->menu);
    
    // Update score display
    render_update_score(&state->stats);
}
