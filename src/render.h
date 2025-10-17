/**
 * @file render.h
 * @brief Rendering pipeline for F256 Switcharoo
 * 
 * Connects game state to video hardware, handles sprite updates and highlights
 */

#ifndef RENDER_H
#define RENDER_H

#include "platform_f256.h"
#include "../src/game_state.h"

// Highlight types for visual feedback
typedef enum {
    HIGHLIGHT_NONE = 0,
    HIGHLIGHT_SELECTED,       // Selected piece
    HIGHLIGHT_LEGAL_MOVE,     // Legal move destination
    HIGHLIGHT_WIN_PATH,       // Winning path cells
    HIGHLIGHT_MENU_HOVER      // Menu icon hover
} highlight_type_t;

// Initialize rendering system
void render_init(void);

// Update all rendering based on game state
void render_update(const game_state_t *state);

// Update piece sprites to match board state
void render_update_pieces(const board_t *board);

// Update highlights for selected piece and legal moves
void render_update_highlights(const selection_state_t *selection);

// Update winning path highlights
void render_update_win_path(const win_path_t *path);

// Update session score display
void render_update_score(const session_stats_t *stats);

// Helper: Get screen coordinates for board cell
void render_cell_to_screen(uint8_t row, uint8_t col, uint16_t *x, uint16_t *y);

// Invalidate internal render cache forcing a full sprite redefinition
// on next render_update_pieces() call.
void render_invalidate_cache(void);

// Helper: Get board cell from screen coordinates
bool render_screen_to_cell(uint16_t x, uint16_t y, uint8_t *row, uint8_t *col);

#endif // RENDER_H
