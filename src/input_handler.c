/**
 * @file input_handler.c
 * @brief Input event handler implementation
 */

#include "input_handler.h"
#include "mouse_pointer.h"
#include "video.h"
#include "text_display.h"
#include "render.h"
#include "game_state.h"
#include <string.h>

#include "screen.h"
#include "overlay_config.h"

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

#pragma code(ovl15_code)
void FAR_input_handler_process_event(game_state_t *state, const input_event_t *event) {
    static uint16_t last_hovered_row = 0xFFFF;
    static uint16_t last_hovered_col = 0xFFFF;
    switch (event->type) {
        case INPUT_EVENT_MOUSE_DOWN:
            if (event->data.mouse.button == MOUSE_BUTTON_LEFT) {
                hit_result_t hit = input_handler_hit_test(event->data.mouse.x, event->data.mouse.y);
                if (hit.type == HIT_BOARD_CELL) {
                    uint8_t row = hit.data.cell.row;
                    uint8_t col = hit.data.cell.col;
                    if (state->selection.has_selection) {
                        bool found_move = false;
                        for (uint8_t i = 0; i < state->selection.legal_move_count; i++) {
                            if (state->selection.legal_moves[i].to_row == row &&
                                state->selection.legal_moves[i].to_col == col) {
                                game_state_execute_selected_move(state, i);
                                found_move = true;
                                break;
                            }
                        }
                        if (!found_move) {
                            if (row == state->selection.selected_row && col == state->selection.selected_col) {
                                game_state_deselect_piece(state);
                            }
                        }
                    } else {
                        game_state_select_piece(state, row, col);
                    }
                } else if (hit.type == HIT_MENU_ICON) {
                    if (state->menu.enabled[hit.data.icon]) {
                        game_state_activate_menu_icon(state, hit.data.icon);
                    }
                }
            }
            break;
        case INPUT_EVENT_MOUSE_MOVE:
            // Update hover state for highlights
            {
            hit_result_t hit = input_handler_hit_test(event->data.mouse.x, event->data.mouse.y);

            switch(hit.type) {
                case HIT_BOARD_CELL:
                // use mouse over color for cell indexed by hit.data.cell.row, hit.data.cell.col
                // track index of highlighted cell
                // if hovered cell not = to tracked index then remove highlight from tracked index
                // add highlight to new cell and update the tracked index
                    if (!state->win_path.has_path &&last_hovered_row != 0xFFFF && last_hovered_col != 0xFFFF &&
                        (last_hovered_row != hit.data.cell.row || last_hovered_col != hit.data.cell.col)) {
                        // Clear previous highlight
                        video_reset_board_cell_color(last_hovered_row, last_hovered_col);
                    }
                    last_hovered_row = hit.data.cell.row;
                    last_hovered_col = hit.data.cell.col;
                    if (!state->win_path.has_path) {
                        video_set_board_cell_hover_color(hit.data.cell.row, hit.data.cell.col);
                    }
                    break;
                case HIT_MENU_ICON:
                
                // output a text string describing the icon function
                    print_icon_tooltip(hit.data.icon);
                    break;
                default:
                    // clear any highlighted cell or icon
                    if (last_hovered_row != 0xFFFF && last_hovered_col != 0xFFFF) {
                        video_reset_board_cell_color(last_hovered_row, last_hovered_col);
                        last_hovered_row = 0xFFFF;
                        last_hovered_col = 0xFFFF;
                    }
                    print_press_f1_for_help();
                    break;
                }
            }
            break;
        case INPUT_EVENT_KEY_DOWN:
            switch (event->data.key.code) {
                case KEY_UP:
                case KEY_DOWN:
                case KEY_LEFT:
                case KEY_RIGHT:
                    input_handler_move_focus(state, event->data.key.code);
                    // clear any highlighted cell or icon
                    if (last_hovered_row != 0xFFFF && last_hovered_col != 0xFFFF) {
                        video_reset_board_cell_color(last_hovered_row, last_hovered_col);
                        last_hovered_row = 0xFFFF;
                        last_hovered_col = 0xFFFF;
                    }
                    break;
                case KEY_ENTER:
                case KEY_SPACE:
                    input_handler_activate_focused(state);
                    break;
                case KEY_ESCAPE:
                    if (state->selection.has_selection) {
                        game_state_deselect_piece(state);
                    }
                    break;
                case KEY_M:
                    if (state->menu.enabled[MENU_ICON_GAME_MODE]) {
                        game_state_activate_menu_icon(state, MENU_ICON_GAME_MODE);
                    }
                    break;
                case KEY_R:
                    if (state->menu.enabled[MENU_ICON_RESET]) {
                        game_state_activate_menu_icon(state, MENU_ICON_RESET);
                    }
                    break;
                case KEY_P:
                    if (state->menu.enabled[MENU_ICON_PREVIOUS]) {
                        game_state_activate_menu_icon(state, MENU_ICON_PREVIOUS);
                    }
                    break;
                case KEY_N:
                    if (state->menu.enabled[MENU_ICON_NEXT]) {
                        game_state_activate_menu_icon(state, MENU_ICON_NEXT);
                    }
                    break;
                case KEY_B:
                    (void)game_state_step_history_back(state);
                    break;
                case KEY_F:
                    (void)game_state_step_history_forward(state);
                    break;
                case KEY_S:
                    if (state->menu.enabled[MENU_ICON_SWAP]) {
                        game_state_activate_menu_icon(state, MENU_ICON_SWAP);
                    }
                    break;
                case KEY_D:
                    if (state->menu.enabled[MENU_ICON_DIFFICULTY]) {
                        game_state_activate_menu_icon(state, MENU_ICON_DIFFICULTY);
                    }
                    break;
                case KEY_H:
                    if (state->menu.enabled[MENU_ICON_HINT]) {
                        game_state_activate_menu_icon(state, MENU_ICON_HINT);
                    }
                    break;
                case KEY_X:
                    if (state->menu.enabled[MENU_ICON_EXIT]) {
                        game_state_activate_menu_icon(state, MENU_ICON_EXIT);
                    }
                    break;
                case KEY_U:
                    // TODO: Implement undo
                    break;

                default:
                    break;
            }
            break;
        default:
            break;
    }
    // To add a new screen: update screen_state_t, add a case above, and implement input guards and transitions as needed.
}
#pragma code(code)

void input_handler_process_event(game_state_t *state, const input_event_t *event) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_15);
    FAR_input_handler_process_event(state, event);
    POKE(OVERLAY_MMU_REG, saved);
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
