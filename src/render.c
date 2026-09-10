/**
 * @file render.c
 * @brief Rendering pipeline implementation
 */

#include "render.h"
#include "input.h"
#include "board.h"
#include "text_display.h"
#include "mouse_pointer.h"
#include <string.h>
#include "sram_assets.h"
#include "video.h"
#include "overlay_config.h"

// External functions from video.c
extern void video_reset_board_cell_color(uint8_t row, uint8_t col);
extern void video_set_board_cell_win_color(uint8_t row, uint8_t col, player_t player);
extern void video_reset_all_board_cell_colors(void);

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

// Helper: snapshot board pieces for change detection
static void cache_board_snapshot(const board_t *board) {
    for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
        for (uint8_t c = 0; c < BOARD_COLS; ++c) {
            s_cache_board_snapshot[r][c] = (uint8_t)board_get_piece_unchecked(board, r, c);
        }
    }
}

#pragma code(ovl13_code)
void FAR_render_init(void) {

    const int16_t board_width = VIDEO_BOARD_COLUMNS * VIDEO_BOARD_CELL_SIZE + 11;
    const int16_t board_height = VIDEO_BOARD_ROWS * VIDEO_BOARD_CELL_SIZE + 15;
    
    s_board_x = (VIDEO_SCREEN_WIDTH - board_width) / 2;
    s_board_y = (VIDEO_SCREEN_HEIGHT - board_height) / 2;

    // (no diagnostic output)
    
    // Icon panel position
    s_icon_x = VIDEO_MENU_FIRST_ICON_X;
    s_icon_start_y = VIDEO_MENU_FIRST_ICON_Y;

    // Initialize and define sprites once (bitmaps + CLUTs). Positions are updated at runtime.
    // Define piece sprites (16)
    for (uint8_t i = 0; i < 16; ++i) {
        uint32_t bitmap = (i < 8) ? SRAM_PIECE_A_NORMAL_LIGHT : SRAM_PIECE_B_NORMAL_LIGHT;
        spriteDefine((uint8_t)(VIDEO_SPRITE_PIECE_BASE + i), bitmap, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PIECES_CLUT, VIDEO_SPRITE_PIECE_LAYER);
        spriteSetVisible((uint8_t)(VIDEO_SPRITE_PIECE_BASE + i), 0);
    }

    // // Define icon sprites (8)
    // for (uint8_t i = 0; i < 8; ++i) {
    //     uint8_t sid = (uint8_t)(VIDEO_SPRITE_ICON_BASE + i);
    //     spriteDefine(sid, s_video_icon_vram_addrs[i], VIDEO_ICON_SPRITE_SIZE, VIDEO_MENU_CLUT, VIDEO_SPRITE_ICON_LAYER);
    //     spriteSetVisible(sid, 1);
    // }

    // Define highlight sprites (8 empty + 8 occupied) - one per direction each.
    // Empty cell highlight sprites: base..base+7 use the EMPTY bitmap
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t sid = (uint8_t)(VIDEO_SPRITE_HIGHLIGHT_BASE + i);
        spriteDefine(sid, SRAM_HIGHLIGHT_EMPTY, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PIECES_CLUT, VIDEO_SPRITE_HIGHLIGHT_LAYER);
        spriteSetVisible(sid, 0);
    }
    // Occupied cell highlight sprites: base+8..base+15 use the OCCUPIED bitmap
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t sid = (uint8_t)(VIDEO_SPRITE_HIGHLIGHT_BASE + 8 + i);
        spriteDefine(sid, SRAM_HIGHLIGHT_OCCUPIED, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PIECES_CLUT, VIDEO_SPRITE_HIGHLIGHT_LAYER);
        spriteSetVisible(sid, 0);
    }

    // Define focus sprites (piece and icon) on layer 0
    spriteDefine((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, SRAM_FOCUS_PIECE, VIDEO_FOCUS_SPRITE_SIZE, VIDEO_PIECES_CLUT, VIDEO_SPRITE_FOCUS_LAYER);
    spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);


    // Mark cache initialized
    s_cache_initialized = false;

}
#pragma code(code)

void render_init(void) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_render_init();
    POKE(OVERLAY_MMU_REG, saved);
}

// Force the render cache to be invalidated so next update will re-snapshot
void render_invalidate_cache(void) {
    s_cache_initialized = false;
    s_win_path_applied = false;
}

void render_cell_to_screen(uint8_t row, uint8_t col, uint16_t *x, uint16_t *y) {
    const int16_t first_cell_x = VIDEO_BOARD_FIRST_CELL_X;
    const int16_t first_cell_y = VIDEO_BOARD_FIRST_CELL_Y;
    const uint16_t cell_offset = (VIDEO_BOARD_CELL_SIZE - VIDEO_PIECE_SPRITE_SIZE) / 2;

    const int16_t cell_x = first_cell_x + (col * (VIDEO_BOARD_CELL_SIZE + 1)) + cell_offset;
    const int16_t cell_y = first_cell_y + (row * (VIDEO_BOARD_CELL_SIZE + 1)) + cell_offset;

    if (x) *x = (uint16_t)cell_x;
    if (y) *y = (uint16_t)cell_y;
}

bool render_screen_to_cell(uint16_t x, uint16_t y, uint8_t *row, uint8_t *col) {
    const int16_t first_cell_x = VIDEO_BOARD_FIRST_CELL_X;
    const int16_t first_cell_y = VIDEO_BOARD_FIRST_CELL_Y;
    
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


#pragma code(ovl13_code)
void FAR_render_update_pieces(const board_t *board, const win_path_t *path) {
    // Track which sprites we've used for each player
    uint8_t white_sprite_count = 0;
    uint8_t black_sprite_count = 0;

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
            piece_type_t piece = board_get_piece_unchecked(board, row, col);

            bool is_light = (row + col) % 2 == 0;
            bool in_path = false;

            if (piece == PIECE_NONE) {
                continue;  // Empty cell
            }

            uint8_t sprite_id = 0;
            uint32_t bitmap_addr = 0;

            // in winning path?
            if (path->has_path) {
                for (uint8_t p = 0; p < path->path_length; ++p) {
                    uint8_t cell_index = path->path_cells[p];
                    uint8_t path_row = cell_index / BOARD_COLS;
                    uint8_t path_col = cell_index % BOARD_COLS;
                    if (path_row == row && path_col == col) {
                        in_path = true;
                        break;
                    }
                }
            }

            // Determine sprite and bitmap based on piece type
            // future will support rendering win path differently
            if (piece == PIECE_WHITE_NORMAL) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + white_sprite_count;
                    bitmap_addr = is_light || in_path ? SRAM_PIECE_A_NORMAL_LIGHT : SRAM_PIECE_A_NORMAL_DARK;
                    white_sprite_count++;
            } else if (piece == PIECE_WHITE_SWAPPED) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + white_sprite_count;
                    bitmap_addr = is_light || in_path ? SRAM_PIECE_A_SWAPPED_LIGHT : SRAM_PIECE_A_SWAPPED_DARK;
                    white_sprite_count++;
            } else if (piece == PIECE_BLACK_NORMAL) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + 8 + black_sprite_count;
                    bitmap_addr = is_light || in_path ? SRAM_PIECE_B_NORMAL_LIGHT : SRAM_PIECE_B_NORMAL_DARK;
                    black_sprite_count++;
            } else if (piece == PIECE_BLACK_SWAPPED) {
                    sprite_id = VIDEO_SPRITE_PIECE_BASE + 8 + black_sprite_count;
                    bitmap_addr = is_light || in_path ? SRAM_PIECE_B_SWAPPED_LIGHT : SRAM_PIECE_B_SWAPPED_DARK;
                    black_sprite_count++;
            }


            // Check if piece has changed since last frame
            if (!force_full_update && s_cache_board_snapshot[row][col] == (uint8_t)piece) {
                // No change — just reposition sprite
            } else {
                // Piece has changed — update sprite definition
                spriteDefine(sprite_id, bitmap_addr, VIDEO_PIECE_SPRITE_SIZE, VIDEO_PIECES_CLUT, VIDEO_SPRITE_PIECE_LAYER);
            }

            uint16_t x, y;
            render_cell_to_screen(row, col, &x, &y);

            spriteSetPosition(sprite_id, VIDEO_SPRITE_OFFSET + x, VIDEO_SPRITE_OFFSET + y);

            spriteSetVisible(sprite_id, 1);

        }
    }

    // Update cache of board pieces after positioning
    cache_board_snapshot(board);
}

void FAR_render_update_highlights(const selection_state_t *selection) {
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
#pragma code(code)

void render_update_pieces(const board_t *board, const win_path_t *path) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_render_update_pieces(board, path);
    POKE(OVERLAY_MMU_REG, saved);
}

void render_update_highlights(const selection_state_t *selection) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_13);
    FAR_render_update_highlights(selection);
    POKE(OVERLAY_MMU_REG, saved);
}

// Called from render_update to show focus indicator when keyboard mode is active
static void render_update_focus(void) {
    // Query input focus
    uint8_t row, col;
    input_get_focus(&row, &col);
    if (!input_is_keyboard_mode()) {
        // Enable mouse and hide focus sprite
        enable_mouse();
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);

        return;
    }

    // Determine whether focus is on board cell or icon area.
    // If focus row within board rows, show piece focus; otherwise show icon focus.
    if (row < VIDEO_BOARD_ROWS) {
        // Show piece focus at focused cell
        uint16_t x, y;
        render_cell_to_screen(row, col, &x, &y);
        spriteSetPosition((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, VIDEO_SPRITE_OFFSET + x - 2, VIDEO_SPRITE_OFFSET + y - 2);
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 1);
    } else {
        spriteSetVisible((uint8_t)VIDEO_SPRITE_FOCUS_PIECE, 0);
    }
}

void refresh_win_path(const win_path_t *path) {
    s_win_path_applied = false;
    render_update_win_path(path);
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

void render_update_score(const session_stats_t *stats) {

    print_win_loss(stats->white_wins, stats->black_wins);
}

void render_update(const game_state_t *state) {
    // Wait for vertical blank to avoid tearing
	while (PEEKW(RAST_ROW_L) < 482)
		// Spin our wheels.
		;
    
    // Update all rendering based on current game state
    
    // Update piece positions and sprites
    render_update_pieces(&state->board, &state->win_path);
    
    // Update highlights for selection
    render_update_highlights(&state->selection);
    // Update focus indicator (keyboard navigation)
    render_update_focus();
    
    // Update winning path if game over
    if (state->phase == GAME_PHASE_GAME_OVER) {
        if(!s_win_path_applied) {
            render_update_win_path(&state->win_path);
            render_update_score(&state->stats);
        }

    }
    
}
