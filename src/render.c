/**
 * @file render.c
 * @brief Rendering pipeline implementation
 */

#include "../src/render.h"
#include <string.h>

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
#define VIDEO_CLUT_HIGHLIGHT_PRIMARY 35
#define VIDEO_CLUT_HIGHLIGHT_SECONDARY 36
// Highlight sprite CLUT slots (distinct from board highlight CLUTs)
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_PRIMARY 85
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_SECONDARY 86
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY 87
#define VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_SECONDARY 88

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

void render_init(void) {
    // Calculate board layout (matching video_position_sprites logic)
    const int16_t board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 11;
    const int16_t board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 15;
    
    s_board_x = (VIDEO_SCREEN_WIDTH - board_width) / 2;
    s_board_y = (VIDEO_SCREEN_HEIGHT - board_height) / 2;
    
    // Icon panel position
    s_icon_x = s_board_x + board_width + 16;
    s_icon_start_y = s_board_y + 8;
}

void render_cell_to_screen(uint8_t row, uint8_t col, uint16_t *x, uint16_t *y) {
    const int16_t first_cell_x = s_board_x + 4;
    const int16_t first_cell_y = s_board_y + 4;
    const uint16_t cell_offset = (VIDEO_BOARD_CELL_SIZE - VIDEO_PIECE_SPRITE_SIZE) / 2;
    
    uint16_t cell_x = (uint16_t)(first_cell_x + (col * (VIDEO_BOARD_CELL_SIZE + 1)));
    uint16_t cell_y = (uint16_t)(first_cell_y + (row * (VIDEO_BOARD_CELL_SIZE + 1)));
    
    if (x) *x = cell_x + cell_offset;
    if (y) *y = cell_y + cell_offset;
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
                // Position and configure sprite
                uint16_t x, y;
                render_cell_to_screen(row, col, &x, &y);
                
                spriteDefine(sprite_id, bitmap_addr, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PRIMARY_CLUT, 1);
                spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + x, VIDEO_SPRITE_OFFSET + y);
                spriteSetVisible(sprite_id, 1);
            }
        }
    }
    
    // Hide unused white sprites
    for (uint8_t i = white_sprite_count; i < 8; ++i) {
        spriteSetVisible(VIDEO_SPRITE_PIECE_BASE + i, 0);
    }
    
    // Hide unused black sprites  
    for (uint8_t i = black_sprite_count; i < 8; ++i) {
        spriteSetVisible(VIDEO_SPRITE_PIECE_BASE + 8 + i, 0);
    }
}

void render_update_highlights(const selection_state_t *selection) {
    // Clear all highlight sprites first
    for (uint8_t i = 0; i < 16; ++i) {
        spriteSetVisible(VIDEO_SPRITE_HIGHLIGHT_BASE + i, 0);
    }
    
    if (!selection->has_selection) {
        return;
    }
    
    uint8_t highlight_index = 0;
    
    // Highlight selected piece cell - use a distinct visual indicator
    uint16_t sel_x, sel_y;
    render_cell_to_screen(selection->selected_row, selection->selected_col, &sel_x, &sel_y);
    
    uint8_t sel_sprite = VIDEO_SPRITE_HIGHLIGHT_BASE + highlight_index++;
    // Use dedicated highlight bitmap and highlight CLUT
    spriteDefine(sel_sprite, VIDEO_VRAM_HIGHLIGHT, VIDEO_PIECE_SPRITE_SIZE, 
                VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY, 1);
    spriteSetPosition(sel_sprite, VIDEO_SPRITE_OFFSET + sel_x, VIDEO_SPRITE_OFFSET + sel_y);
    spriteSetVisible(sel_sprite, 1);
    
    // Highlight all legal move destinations
    for (uint8_t i = 0; i < selection->legal_move_count && highlight_index < 16; ++i) {
        const move_t *move = &selection->legal_moves[i];
        
        uint16_t move_x, move_y;
        render_cell_to_screen(move->to_row, move->to_col, &move_x, &move_y);
        
    // Do not highlight a move that targets the currently selected cell
    if (move->to_row == selection->selected_row && move->to_col == selection->selected_col) {
        continue;
    }

    uint8_t move_sprite = VIDEO_SPRITE_HIGHLIGHT_BASE + highlight_index++;
        
    // Distinguish hovered move from other legal moves and empty vs occupied target
    bool target_occupied = (move->type == MOVE_TYPE_SWAP);
    uint8_t clut_index;
    if (target_occupied) {
        clut_index = (selection->hovered_move == (int8_t)i) ? VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY : VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_SECONDARY;
    } else {
        clut_index = (selection->hovered_move == (int8_t)i) ? VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_PRIMARY : VIDEO_CLUT_HIGHLIGHT_SPRITE_EMPTY_SECONDARY;
    }

    spriteDefine(move_sprite, VIDEO_VRAM_HIGHLIGHT, VIDEO_PIECE_SPRITE_SIZE,
            clut_index, 1);
        spriteSetPosition(move_sprite, VIDEO_SPRITE_OFFSET + move_x, VIDEO_SPRITE_OFFSET + move_y);
        spriteSetVisible(move_sprite, 1);
    }
}

void render_update_win_path(const win_path_t *path) {
    // Clear highlight sprites when no win path
    if (!path || !path->has_path) {
        for (uint8_t i = 0; i < 16; ++i) {
            spriteSetVisible(VIDEO_SPRITE_HIGHLIGHT_BASE + i, 0);
        }
        return;
    }
    
    // Highlight each cell in the winning path
    for (uint8_t i = 0; i < path->path_length && i < 16; ++i) {
        uint8_t cell_index = path->path_cells[i];
        uint8_t row = cell_index / BOARD_COLS;
        uint8_t col = cell_index % BOARD_COLS;
        
        uint16_t x, y;
        render_cell_to_screen(row, col, &x, &y);
        
        uint8_t sprite_id = VIDEO_SPRITE_HIGHLIGHT_BASE + i;
        
    // Use special CLUT for winning path (occupied-style highlight)
    spriteDefine(sprite_id, VIDEO_VRAM_HIGHLIGHT, VIDEO_PIECE_SPRITE_SIZE,
        VIDEO_CLUT_HIGHLIGHT_SPRITE_OCCUPIED_PRIMARY, 1);  // Distinct CLUT for win highlight
        spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + x, VIDEO_SPRITE_OFFSET + y);
        spriteSetVisible(sprite_id, 1);
    }
    
    // Hide unused highlight sprites
    for (uint8_t i = path->path_length; i < 16; ++i) {
        spriteSetVisible(VIDEO_SPRITE_HIGHLIGHT_BASE + i, 0);
    }
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
        if (!menu->enabled[i]) {
            // Use dimmed CLUT for disabled icons
            clut_index = VIDEO_PRIMARY_CLUT + 5;  // Dedicated disabled icon CLUT
        } else if (menu->selected_icon == (int8_t)i) {
            // Highlight selected/hovered icon
            clut_index = VIDEO_PRIMARY_CLUT + 6;  // Highlight CLUT
        }
        
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
    
    // Update winning path if game over
    if (state->phase == GAME_PHASE_GAME_OVER) {
        render_update_win_path(&state->win_path);
    }
    
    // Update menu icons
    render_update_menu(&state->menu);
    
    // Update score display
    render_update_score(&state->stats);
}
