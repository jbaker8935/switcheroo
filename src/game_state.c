/**
 * @file game_state.c
 * @brief Game state management implementation
 */

#include "../src/game_state.h"
#include <string.h>

void game_state_init(game_state_t *state) {
    memset(state, 0, sizeof(game_state_t));
    
    // Initialize board
    board_init(&state->board);
    
    // Set default preferences
    state->prefs.difficulty_level = 2;  // Standard
    state->prefs.color_scheme = 0;      // Default theme
    state->prefs.ai_explanations_enabled = false;
    state->prefs.audio_enabled = true;
    state->prefs.volume_level = 7;
    
    // Initialize AI config - Classic swap rules, AI plays as Black (second player)
    ai_agent_init(&state->ai_config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);
    
    // Initialize menu state
    game_state_update_menu_enables(state);
    state->menu.hovered_icon = -1;
    state->menu.selected_icon = -1;
    
    // Start at title
    state->phase = GAME_PHASE_TITLE;
}

void game_state_set_phase(game_state_t *state, game_phase_t phase) {
    state->phase = phase;
}

game_phase_t game_state_get_phase(const game_state_t *state) {
    return state->phase;
}

void game_state_reset_board(game_state_t *state) {
    board_reset(&state->board);
    game_state_deselect_piece(state);
    game_state_update_menu_enables(state);
    state->win_path.has_path = false;
    
    // Reset board cell colors to original checkerboard pattern
    extern void video_reset_all_board_cell_colors(void);
    video_reset_all_board_cell_colors();
}

void game_state_start_new_game(game_state_t *state) {
    game_state_reset_board(state);
    state->phase = GAME_PHASE_PLAYING;
}

void game_state_select_piece(game_state_t *state, uint8_t row, uint8_t col) {
    // Get piece at location
    piece_type_t piece = board_get_piece(&state->board, row, col);
    
    // Check if it belongs to current player
    if (board_get_piece_owner(piece) != state->board.current_player) {
        return;
    }
    
    // Get legal moves first
    uint8_t legal_move_count = board_get_legal_moves(
        &state->board, row, col,
        state->selection.legal_moves, 8
    );
    
    // Only select if there are legal moves
    if (legal_move_count == 0) {
        return;
    }
    
    // Select the piece
    state->selection.has_selection = true;
    state->selection.selected_row = row;
    state->selection.selected_col = col;
    state->selection.hovered_move = -1;
    state->selection.legal_move_count = legal_move_count;
}

void game_state_deselect_piece(game_state_t *state) {
    state->selection.has_selection = false;
    state->selection.legal_move_count = 0;
    state->selection.hovered_move = -1;
}

bool game_state_execute_selected_move(game_state_t *state, uint8_t move_index) {
    if (!state->selection.has_selection || 
        move_index >= state->selection.legal_move_count) {
        return false;
    }
    
    move_t *move = &state->selection.legal_moves[move_index];
    
    if (board_execute_move(&state->board, move)) {
        game_state_deselect_piece(state);
        
        // Check for win
        if (game_state_check_win_condition(state)) {
            state->phase = GAME_PHASE_GAME_OVER;
        } else {
            // Switch turns
            board_switch_turn(&state->board);
            
            // If AI's turn, switch to AI thinking phase
            if (state->board.current_player == PLAYER_BLACK) {
                state->phase = GAME_PHASE_AI_THINKING;
            }
        }
        
        // Update menu enables
        game_state_update_menu_enables(state);
        return true;
    }
    
    return false;
}

void game_state_update_menu_enables(game_state_t *state) {
    // Reset is always enabled
    state->menu.enabled[MENU_ICON_RESET] = true;
    
    // Info is always enabled
    state->menu.enabled[MENU_ICON_INFO] = true;
    
    // Difficulty is always enabled
    state->menu.enabled[MENU_ICON_DIFFICULTY] = true;
    
    // Starting board enabled only when no moves have been made
    state->menu.enabled[MENU_ICON_STARTING_BOARD] = (state->board.move_count == 0);
    
    // History enabled only when moves have been made
    state->menu.enabled[MENU_ICON_HISTORY] = (state->board.history_count > 0);
    
    // Exit is always enabled
    state->menu.enabled[MENU_ICON_EXIT] = true;
}

void game_state_activate_menu_icon(game_state_t *state, menu_icon_t icon) {
    if (!state->menu.enabled[icon]) {
        return;
    }
    
    switch (icon) {
        case MENU_ICON_RESET:
            game_state_reset_board(state);
            state->phase = GAME_PHASE_PLAYING;
            break;
            
        case MENU_ICON_INFO:
            state->phase = GAME_PHASE_MENU_OVERLAY;
            break;
            
        case MENU_ICON_DIFFICULTY:
            // Cycle difficulty
            state->prefs.difficulty_level = (state->prefs.difficulty_level + 1) % 4;
            // Update AI difficulty
            state->ai_config.difficulty = (ai_difficulty_t)state->prefs.difficulty_level;
            break;
            
        case MENU_ICON_STARTING_BOARD:
            // TODO: Implement starting board selection
            break;
            
        case MENU_ICON_HISTORY:
            state->phase = GAME_PHASE_MENU_OVERLAY;
            break;
            
        case MENU_ICON_EXIT:
            state->phase = GAME_PHASE_EXIT;
            break;
            
        default:
            break;
    }
}

bool game_state_check_win_condition(game_state_t *state) {
    // Check both players for win condition
    bool white_wins = board_check_win(&state->board, PLAYER_WHITE, &state->win_path);
    
    if (white_wins) {
        state->stats.white_wins++;
        
        // Diagnostic output
        textGotoXY(0, 5);
        printf("WHITE WINS! W:%d B:%d",
               state->stats.white_wins,
               state->stats.black_wins);
        return true;
    }
    
    win_path_t black_path;
    bool black_wins = board_check_win(&state->board, PLAYER_BLACK, &black_path);
    
    if (black_wins) {
        state->stats.black_wins++;
        state->win_path = black_path;
        
        // Diagnostic output
        textGotoXY(0, 5);
        printf("BLACK WINS! W:%d B:%d",
               state->stats.white_wins,
               state->stats.black_wins);
        return true;
    }
    
    return false;
}

void game_state_update(game_state_t *state, float delta_time) {
    (void)delta_time;
    state->frame_count++;
    
    // Phase-specific updates
    switch (state->phase) {
        case GAME_PHASE_AI_THINKING: {
            // Add a small visual delay before AI makes move
            state->ai_think_frames++;
            
            // Diagnostic output
            textGotoXY(0, 3);
            printf("AI thinking... %d", (int)state->ai_think_frames);
            
            // Wait at least 30 frames (~0.5 seconds) before executing AI move
            if (state->ai_think_frames >= 30) {
                move_t ai_move;
                if (ai_agent_find_best_move(&state->board, &state->ai_config, &ai_move)) {
                    // Diagnostic output with piece types
                    piece_type_t from_piece = board_get_piece(&state->board, ai_move.from_row, ai_move.from_col);
                    piece_type_t to_piece = board_get_piece(&state->board, ai_move.to_row, ai_move.to_col);
                    
                    textGotoXY(0, 4);
                    printf("AI: (%d,%d)->(%d,%d) %s",
                           ai_move.from_row, ai_move.from_col,
                           ai_move.to_row, ai_move.to_col,
                           (ai_move.type == MOVE_TYPE_SWAP) ? "SWAP" : "EMPTY");
                    
                    textGotoXY(0, 6);
                    const char* from_name = board_is_piece_swapped(from_piece) ? "SWAP" : "NORM";
                    const char* to_name = (to_piece == PIECE_NONE) ? "EMPT" : 
                                         (board_is_piece_swapped(to_piece) ? "SWAP" : "NORM");
                    printf("From:%s To:%s", from_name, to_name);
                    
                    // Execute AI move
                    if (board_execute_move(&state->board, &ai_move)) {
                        // Check for win
                        if (game_state_check_win_condition(state)) {
                            state->phase = GAME_PHASE_GAME_OVER;
                        } else {
                            // Switch back to human player
                            board_switch_turn(&state->board);
                            state->phase = GAME_PHASE_PLAYING;
                        }
                    }
                }
                
                state->ai_think_frames = 0;
                game_state_update_menu_enables(state);
            }
            break;
        }
            
        default:
            break;
    }
}
