/**
 * @file input_handler.c
 * @brief Input event handler implementation
 */

#include "../src/input_handler.h"
#include "../src/mouse_pointer.h"
#include "../src/video.h"
#include "../src/text_display.h"
#include "../src/render.h"
#include "../src/game_state.h"
#include <string.h>

static game_phase_t old_phase = GAME_PHASE_TITLE;

void input_handler_init(void) {

}

hit_result_t input_handler_hit_test(uint16_t screen_x, uint16_t screen_y) {
    hit_result_t result;
    result.type = HIT_NONE;
    


    if (screen_x >= VIDEO_BOARD_FIRST_CELL_X && screen_x < VIDEO_BOARD_FIRST_CELL_X + VIDEO_BOARD_INSIDE_WIDTH &&
        screen_y >= VIDEO_BOARD_FIRST_CELL_Y && screen_y < VIDEO_BOARD_FIRST_CELL_Y + VIDEO_BOARD_INSIDE_HEIGHT) {

        // Inside board - determine which cell
        int16_t rel_x = screen_x - VIDEO_BOARD_FIRST_CELL_X;
        int16_t rel_y = screen_y - VIDEO_BOARD_FIRST_CELL_Y;
        
        if (rel_x >= 0 && rel_y >= 0) {
            uint8_t col = rel_x / (VIDEO_BOARD_CELL_SIZE + VIDEO_BOARD_CELL_SEPARATOR);
            uint8_t row = rel_y / (VIDEO_BOARD_CELL_SIZE + VIDEO_BOARD_CELL_SEPARATOR);
            
            if (col < BOARD_COLS && row < BOARD_ROWS) {
                result.type = HIT_BOARD_CELL;
                result.data.cell.row = row;
                result.data.cell.col = col;
                return result;
            }
        }
    }
    

    // Check if X coordinate is in icon area 2 columns wide
    if (screen_x >= VIDEO_MENU_FIRST_ICON_X && screen_x < VIDEO_MENU_FIRST_ICON_X + VIDEO_MENU_SPACING_HORIZONTAL + VIDEO_ICON_SELECT_SIZE) {
        // Check if Y coordinate is within icon area
        if (screen_y >= VIDEO_MENU_FIRST_ICON_Y) {
            int16_t rel_x = screen_x - VIDEO_MENU_FIRST_ICON_X;
            int16_t rel_y = screen_y - VIDEO_MENU_FIRST_ICON_Y;
            // Determine which column
            uint8_t col = rel_x / (VIDEO_MENU_SPACING_HORIZONTAL);
            if (col > 1) {
                return result;  // Outside icon columns
            }

            uint8_t row = rel_y / VIDEO_MENU_SPACING_VERTICAL;

            uint8_t icon_index = row * 2 + col;
            
            // Check if actually on the icon (not in gap between icons)
            int16_t icon_x = VIDEO_MENU_FIRST_ICON_X + (col * VIDEO_MENU_SPACING_HORIZONTAL);
            int16_t icon_y = VIDEO_MENU_FIRST_ICON_Y + (row * VIDEO_MENU_SPACING_VERTICAL);
            if (screen_y >= icon_y && screen_y < icon_y + VIDEO_ICON_SELECT_SIZE &&
                screen_x >= icon_x && screen_x < icon_x + VIDEO_ICON_SELECT_SIZE &&
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

            if ( state->phase == GAME_PHASE_HELP && event->data.key.code != KEY_SPACE) {
                break; // Ignore all keys except SPACE when in help screen
            } else if (state->phase == GAME_PHASE_HELP && event->data.key.code == KEY_SPACE) {
                // Exit help screen on SPACE key
                game_state_set_phase(state, old_phase);
                display_hide_help_screen();
                render_update_score(&state->stats);
                print_ai_difficulty(state->ai_config.difficulty);
                print_game_mode(state->is_puzzle_mode);
                print_swap_rule(state->ai_config.swap_rule);
                print_current_player(state->context.current_player);
                print_move_history(state->context.history, state->context.history_count);                          
                if(state->is_puzzle_mode) { 
                    clear_swap_unavailable();
                    game_state_apply_current_puzzle(state, true);
                }
            }

 
            switch (event->data.key.code) {
                case KEY_UP:
                case KEY_DOWN:
                case KEY_LEFT:
                case KEY_RIGHT:
                    input_handler_move_focus(state, event->data.key.code);
                    break;
                    
                case KEY_ENTER:
                case KEY_SPACE:
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

                case KEY_F1:  // F1 for Help
                    old_phase = state->phase;
                    game_state_set_phase(state, GAME_PHASE_HELP);
                    display_show_help_screen();
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
            return;
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
