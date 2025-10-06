/**
 * @file game_state.c
 * @brief Game state management implementation
 */

#include "../src/game_state.h"
#include "../src/puzzle_data.h"
#include <string.h>

static void gs_copy_text(char *dest, size_t dest_size, const char *src) {
    if (!dest || dest_size == 0) {
        return;
    }
    size_t i = 0;
    if (src) {
        while (i + 1 < dest_size && src[i] != '\0') {
            dest[i] = src[i];
            ++i;
        }
    }
    dest[i] = '\0';
}

static void gs_format_win_in(char *dest, size_t dest_size, unsigned value) {
    if (!dest || dest_size == 0) {
        return;
    }
    static const char prefix[] = "WIN IN ";
    size_t len = 0;
    while (len + 1 < dest_size && prefix[len] != '\0') {
        dest[len] = prefix[len];
        ++len;
    }

    char digits[6];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));

    while (count > 0 && len + 1 < dest_size) {
        dest[len++] = digits[--count];
    }

    dest[len] = '\0';
}

// Helper function to format strings to exactly 25 characters with right-padding
void print_formatted_text(uint8_t x, uint8_t y, const char *text) {
    char buf[26]; // 25 chars + null terminator
    uint8_t len = strlen(text);
    if (len > 25) len = 25; // Truncate if too long
    
    // Copy the text
    memcpy(buf, text, len);
    
    // Right-fill with spaces
    while (len < 25) {
        buf[len++] = ' ';
    }
    buf[25] = '\0';
    
    textGotoXY(x, y);
    textPrint(buf);
}

void game_state_init(game_state_t *state) {
    memset(state, 0, sizeof(game_state_t));
    
    // Initialize board
    board_init(&state->board);
    
    // Set default preferences
    state->prefs.difficulty_level = 2;  // Standard
    state->prefs.swap_rule = SWAP_RULE_CLASSIC;
    state->prefs.current_puzzle_index = 0;
    state->prefs.color_scheme = 0;      // Default theme
    state->prefs.ai_explanations_enabled = false;
    state->prefs.audio_enabled = true;
    state->prefs.volume_level = 7;
    
    // Initialize AI config - Classic swap rules, AI plays as Black (second player)
    ai_agent_init(&state->ai_config, state->prefs.swap_rule, AI_DIFFICULTY_STANDARD, PLAYER_BLACK);
    
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
    /* If there have been moves played, reset back to the currently selected
       puzzle starting position so the user can retry the puzzle. If no moves
       have been made, reset to the normal gameplay starting position. */
    if (state->board.move_count > 0) {
        const puzzle_collection_t *collection = get_puzzle_collection();
        const puzzle_t *puzzle = NULL;
        if (collection && state->prefs.current_puzzle_index < collection->count) {
            puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
        }

        if (puzzle) {
            // Apply the puzzle position and ensure AI/swap rule are in sync
            apply_puzzle_position(&state->board, puzzle);
            // Ensure the player to move is set back to Player A (WHITE)
            state->board.current_player = PLAYER_WHITE;
            // Ensure move/history counters are reset so menu icons update
            state->board.move_count = 0;
            state->board.history_count = 0;
            state->prefs.swap_rule = puzzle->swap_rule;
            state->ai_config.swap_rule = puzzle->swap_rule;
            ai_agent_init(&state->ai_config, puzzle->swap_rule, state->ai_config.difficulty, state->ai_config.ai_player);
        } else {
            // Fallback to normal reset
            board_reset(&state->board);
            // Ensure no swapped flags remain
            board_clear_all_swapped(&state->board);
            // Clear any puzzle debug text lines (id, rule, difficulty, hint)
            print_formatted_text(0, 18, "");
            print_formatted_text(0, 19, "");
            print_formatted_text(0, 20, "");
            print_formatted_text(0, 21, "");
        }
    } else {
        // No moves played: normal gameplay reset
        board_reset(&state->board);
        board_clear_all_swapped(&state->board);
        // Clear any puzzle debug text lines
        print_formatted_text(0, 18, "");
        print_formatted_text(0, 19, "");
        print_formatted_text(0, 20, "");
        print_formatted_text(0, 21, "");
    }

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
    
    if (board_execute_move(&state->board, move, state->prefs.swap_rule)) {
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
            
        case MENU_ICON_INFO: {
            // Display the first solution move for the currently selected puzzle (hint)
            const puzzle_collection_t *collection = get_puzzle_collection();
            const puzzle_t *puzzle = NULL;
            if (collection && state->prefs.current_puzzle_index < collection->count) {
                puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
            }

            if (puzzle && puzzle->solution_length > 0) {
                display_puzzle_solution(puzzle);
            } else {
                print_formatted_text(0, 20, "NO SOLUTION          ");
            }

            state->phase = GAME_PHASE_PLAYING;
        } break;
            
        case MENU_ICON_DIFFICULTY:
            // Cycle difficulty
            state->prefs.difficulty_level = (state->prefs.difficulty_level + 1) % 4;
            // Update AI difficulty
            state->ai_config.difficulty = (ai_difficulty_t)state->prefs.difficulty_level;
            break;
            
        case MENU_ICON_STARTING_BOARD:
            // Cycle through puzzles
            const puzzle_collection_t *collection = get_puzzle_collection();
            state->prefs.current_puzzle_index = (state->prefs.current_puzzle_index + 1) % collection->count;
            const puzzle_t *puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
            if (puzzle) {
                // Apply puzzle position
                apply_puzzle_position(&state->board, puzzle);
                // Set swap rule from puzzle
                state->prefs.swap_rule = puzzle->swap_rule;
                state->ai_config.swap_rule = puzzle->swap_rule;
                // Reinitialize AI with new swap rule
                ai_agent_init(&state->ai_config, puzzle->swap_rule, state->ai_config.difficulty, state->ai_config.ai_player);
                // Reset game state
                game_state_deselect_piece(state);
                state->phase = GAME_PHASE_PLAYING;
                // Display debugging info: id, swap rule, and difficulty (WIN IN <n>)
                char id_msg[32];
                gs_copy_text(id_msg, sizeof(id_msg), puzzle->id);
                print_formatted_text(0, 18, id_msg);

                char rule_msg[32];
                gs_copy_text(rule_msg, sizeof(rule_msg), swap_rule_to_string(puzzle->swap_rule));
                print_formatted_text(0, 19, rule_msg);

                char diff_msg[32];
                gs_format_win_in(diff_msg, sizeof(diff_msg), (unsigned)puzzle->difficulty);
                print_formatted_text(0, 20, diff_msg);
            }
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
        return true;
    }
    
    win_path_t black_path;
    bool black_wins = board_check_win(&state->board, PLAYER_BLACK, &black_path);
    
    if (black_wins) {
        state->stats.black_wins++;
        state->win_path = black_path;
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
            // Wait at least 30 frames (~0.5 seconds) before executing AI move
            if (state->ai_think_frames >= 30) {
                move_t ai_move;
                if (ai_agent_find_best_move(&state->board, &state->ai_config, &ai_move)) {
                    // Execute AI move
                    if (board_execute_move(&state->board, &ai_move, state->ai_config.swap_rule)) {
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
