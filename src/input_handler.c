/**
 * @file input_handler.c
 * @brief Input event handler implementation
 */

#include "../src/input_handler.h"
#include "../src/mouse_pointer.h"
#include <string.h>

// Layout constants from video.c
#define SCREEN_WIDTH 320u
#define SCREEN_HEIGHT 240u
#define BOARD_CELL_SIZE 26u
#define BOARD_CELL_SEPARATOR 1u
#define ICON_SIZE 16u
#define BOARD_BORDER 4u

// Calculate board position (centered on screen)
static int16_t s_board_x;
static int16_t s_board_y;
static int16_t s_icon_x;       // Menu icons on right side
static int16_t s_icon_start_y; // First icon Y position

void input_handler_init(void) {

    const int16_t board_width = (BOARD_COLS * BOARD_CELL_SIZE) + (3 * BOARD_CELL_SEPARATOR) + (2 * BOARD_BORDER);
    const int16_t board_height = (BOARD_ROWS * BOARD_CELL_SIZE) + (7 * BOARD_CELL_SEPARATOR) + (2 * BOARD_BORDER);
    
    s_board_x = (SCREEN_WIDTH - board_width) / 2;
    s_board_y = (SCREEN_HEIGHT - board_height) / 2;

    // Menu icons positioned in 2 columns by 4 rows on right side of board
    s_icon_x = s_board_x + board_width + 16;
    s_icon_start_y = s_board_y + 8;
}

hit_result_t input_handler_hit_test(uint16_t screen_x, uint16_t screen_y) {
    hit_result_t result;
    result.type = HIT_NONE;
    
    // Check if click is within board area
    const int16_t board_width = (BOARD_COLS * BOARD_CELL_SIZE) + (3 * BOARD_CELL_SEPARATOR) + (2 * BOARD_BORDER);
    const int16_t board_height = (BOARD_ROWS * BOARD_CELL_SIZE) + (7 * BOARD_CELL_SEPARATOR) + (2 * BOARD_BORDER);
    
    if (screen_x >= s_board_x && screen_x < s_board_x + board_width &&
        screen_y >= s_board_y && screen_y < s_board_y + board_height) {
        
        // Inside board - determine which cell
        int16_t rel_x = screen_x - s_board_x - BOARD_BORDER;
        int16_t rel_y = screen_y - s_board_y - BOARD_BORDER;
        
        if (rel_x >= 0 && rel_y >= 0) {
            uint8_t col = rel_x / (BOARD_CELL_SIZE + BOARD_CELL_SEPARATOR);
            uint8_t row = rel_y / (BOARD_CELL_SIZE + BOARD_CELL_SEPARATOR);
            
            if (col < BOARD_COLS && row < BOARD_ROWS) {
                result.type = HIT_BOARD_CELL;
                result.data.cell.row = row;
                result.data.cell.col = col;
                return result;
            }
        }
    }
    
    // Check if click is on menu icons (2 x 4 layout on right side)
    // Icons are 16x16, spaced 24px apart (16 + 8 gap), arranged in 2 columns by 4 rows
    const int16_t icon_spacing = 24;  // ICON_SIZE (16) + 8 gap
    
    // Check if X coordinate is in icon area 2 columns wide
    if (screen_x >= s_icon_x && screen_x < s_icon_x + 2 * ICON_SIZE + 8) {
        // Check if Y coordinate is within icon area
        if (screen_y >= s_icon_start_y) {
            int16_t rel_x = screen_x - s_icon_x;
            int16_t rel_y = screen_y - s_icon_start_y;
            // Determine which column
            uint8_t col = rel_x / (icon_spacing);
            if (col > 1) {
                return result;  // Outside icon columns
            }

            uint8_t row = rel_y / icon_spacing;

            uint8_t icon_index = row * 2 + col;
            
            // Check if actually on the icon (not in gap between icons)
            int16_t icon_y = s_icon_start_y + (row * icon_spacing);
            int16_t icon_x = s_icon_x + (col * icon_spacing);
            if (screen_y >= icon_y && screen_y < icon_y + ICON_SIZE &&
                screen_x >= icon_x && screen_x < icon_x + ICON_SIZE &&
                icon_index < MENU_ICON_COUNT) {
                result.type = HIT_MENU_ICON;
                result.data.icon = (menu_icon_t)icon_index;
                return result;
            }
        }
    }
    
    return result;
}

void input_handler_process_event(game_state_t *state, const input_event_t *event) {
    if (state->phase == GAME_PHASE_EXIT) {
        return;  // Don't process input when exiting
    }
    
    switch (event->type) {
        case INPUT_EVENT_MOUSE_DOWN:
            if (event->data.mouse.button == MOUSE_BUTTON_LEFT) {
                // Hit test to see what was clicked
                hit_result_t hit = input_handler_hit_test(event->data.mouse.x, event->data.mouse.y);
                
                if (hit.type == HIT_BOARD_CELL) {
                    // Clicked on board cell
                    uint8_t row = hit.data.cell.row;
                    uint8_t col = hit.data.cell.col;
                    
                    if (state->selection.has_selection) {
                        // A piece is already selected
                        // Check if clicked on a legal move destination
                        bool found_move = false;
                        for (uint8_t i = 0; i < state->selection.legal_move_count; i++) {
                            if (state->selection.legal_moves[i].to_row == row &&
                                state->selection.legal_moves[i].to_col == col) {
                                // Execute the move (this will deselect automatically)
                                game_state_execute_selected_move(state, i);
                                found_move = true;
                                break;
                            }
                        }
                        
                        if (!found_move) {
                            // Not a legal move - check if clicking the same selected cell to deselect
                            if (row == state->selection.selected_row && col == state->selection.selected_col) {
                                // Clicking selected piece again - deselect it
                                game_state_deselect_piece(state);
                            }
                            // Otherwise, ignore the click (don't select a different piece while one is selected)
                        }
                    } else {
                        // No selection - try to select this piece (will only work if it's current player's piece)
                        game_state_select_piece(state, row, col);
                    }
                } else if (hit.type == HIT_MENU_ICON) {
                    // Clicked on menu icon
                    if (state->menu.enabled[hit.data.icon]) {
                        game_state_activate_menu_icon(state, hit.data.icon);
                    }
                }
            }
            break;
            
        case INPUT_EVENT_MOUSE_MOVE:
            // Update hover state for highlights
            // TODO: Set hovered_move or hovered_icon based on mouse position
            break;
            
        case INPUT_EVENT_KEY_DOWN:
 
            switch (event->data.key.code) {
                case KEY_UP:
                case KEY_DOWN:
                case KEY_LEFT:
                case KEY_RIGHT:
                    input_handler_move_focus(state, event->data.key.code);
                    break;
                    
                case KEY_ENTER:
                    input_handler_activate_focused(state);
                    break;
                    
                case KEY_ESCAPE:
                    // Deselect piece or close overlay
                    if (state->selection.has_selection) {
                        game_state_deselect_piece(state);
                    }
                    break;
                    
                case KEY_M:  // Game Mode Toggle
                    if (state->menu.enabled[MENU_ICON_GAME_MODE]) {
                        game_state_activate_menu_icon(state, MENU_ICON_GAME_MODE);
                    }
                    break;
                                        
                case KEY_R:  // Reset
                    if (state->menu.enabled[MENU_ICON_RESET]) {
                        game_state_activate_menu_icon(state, MENU_ICON_RESET);
                    }
                    break;
                    
                case KEY_P:  // Previous
                if (state->menu.enabled[MENU_ICON_PREVIOUS]) {
                    game_state_activate_menu_icon(state, MENU_ICON_PREVIOUS);
                }
                break;
                
                case KEY_N:  // Next
                if (state->menu.enabled[MENU_ICON_NEXT]) {
                    game_state_activate_menu_icon(state, MENU_ICON_NEXT);
                }
                break;

                case KEY_S:  // Swap
                if (state->menu.enabled[MENU_ICON_SWAP]) {
                    game_state_activate_menu_icon(state, MENU_ICON_SWAP);
                }
                break;

                case KEY_D:  // Difficulty
                if (state->menu.enabled[MENU_ICON_DIFFICULTY]) {
                    game_state_activate_menu_icon(state, MENU_ICON_DIFFICULTY);
                }
                break;
                
                case KEY_H:  // Hint
                    if (state->menu.enabled[MENU_ICON_HINT]) {
                        game_state_activate_menu_icon(state, MENU_ICON_HINT);
                    }
                    break;
                    
                case KEY_X:  // Exit
                if (state->menu.enabled[MENU_ICON_EXIT]) {
                    game_state_activate_menu_icon(state, MENU_ICON_EXIT);
                }
                break;
                    
                case KEY_U:  // Undo
                    // TODO: Implement undo
                    break;
                    
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

void input_handler_move_focus(game_state_t *state, key_code_t direction) {
    // Get current focus from input state
    uint8_t row, col;
    input_get_focus(&row, &col);
    
    // Move focus based on direction
    switch (direction) {
        case KEY_UP:
            if (row > 0) row--;
            break;
        case KEY_DOWN:
            if (row < BOARD_ROWS - 1) row++;
            break;
        case KEY_LEFT:
            if (col > 0) col--;
            break;
        case KEY_RIGHT:
            if (col < BOARD_COLS - 1) col++;
            break;
        default:
            break;
    }
    
    // Update focus
    input_set_focus(row, col);

}

void input_handler_activate_focused(game_state_t *state) {
    // Get current focus
    uint8_t row, col;
    input_get_focus(&row, &col);
    
    if (input_is_keyboard_mode()) {
        // Activate the focused cell
        if (state->selection.has_selection) {
            // Check if focused cell is a legal move
            for (uint8_t i = 0; i < state->selection.legal_move_count; i++) {
                if (state->selection.legal_moves[i].to_row == row &&
                    state->selection.legal_moves[i].to_col == col) {
                    game_state_execute_selected_move(state, i);
                    return;
                }
            }
            
            // Not a legal move - deselect and select new
            game_state_deselect_piece(state);
            game_state_select_piece(state, row, col);
        } else {
            // No selection - select focused cell
            game_state_select_piece(state, row, col);
        }

    }
}
