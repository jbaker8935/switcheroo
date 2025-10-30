/**
 * @file game_state.c
 * @brief Game state management implementation
 */

#include "../src/game_state.h"
#include "../src/puzzle_data.h"
#include "../src/ai_agent.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../src/text_display.h"
#include "../src/mouse_pointer.h"
#include "../src/board.h"

extern void render_invalidate_cache(void);
extern void video_reset_all_board_cell_colors(void);
extern void video_set_game_mode_icon_bitmap(bool is_puzzle_mode);

static void gs_copy_text(char *dest, size_t dest_size, const char *src)
{
    if (!dest || dest_size == 0)
    {
        return;
    }
    size_t i = 0;
    if (src)
    {
        while (i + 1 < dest_size && src[i] != '\0')
        {
            dest[i] = src[i];
            ++i;
        }
    }
    dest[i] = '\0';
}

static void gs_format_win_in(char *dest, size_t dest_size, unsigned value)
{
    if (!dest || dest_size == 0)
    {
        return;
    }
    static const char prefix[] = "WIN IN ";
    size_t len = 0;
    while (len + 1 < dest_size && prefix[len] != '\0')
    {
        dest[len] = prefix[len];
        ++len;
    }

    char digits[6];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));

    while (count > 0 && len + 1 < dest_size)
    {
        dest[len++] = digits[--count];
    }

    dest[len] = '\0';
}

static void game_state_clear_win_path(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    state->win_path.has_path = false;
    state->win_path.path_length = 0;
    state->win_path.winner = PLAYER_NONE;
    memset(state->win_path.path_cells, 0, sizeof(state->win_path.path_cells));

    video_reset_all_board_cell_colors();
    render_invalidate_cache();
}

static void game_state_configure_ai(game_state_t *state, swap_rule_t swap_rule, ai_difficulty_t difficulty,
                                    player_t ai_player)
{
    if (!state)
    {
        return;
    }

    ai_agent_init(&state->ai_config, swap_rule, difficulty, ai_player);
    ui_progress_register(&state->ai_config, &state->ui_progress);
}

static void game_state_toggle_swap_rule(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    if(state->is_puzzle_mode) {
        print_swap_unavailable();
    } 
    
    if (!state->is_puzzle_mode && state->phase == GAME_PHASE_PLAYING && state->board.move_count == 0) {
        state->prefs.swap_rule = (state->prefs.swap_rule + 1) % NUMBER_OF_SWAP_RULES;
        ai_difficulty_t difficulty = state->ai_config.difficulty;
        player_t ai_player = state->ai_config.ai_player;
        game_state_configure_ai(state, state->prefs.swap_rule, difficulty, ai_player);
        print_swap_rule(state->prefs.swap_rule);
        clear_swap_unavailable();
    } 
}


static bool game_state_apply_current_puzzle(game_state_t *state, bool announce)
{
    const puzzle_collection_t *collection = get_puzzle_collection();
    if (!collection || collection->count == 0u)
    {
        game_state_clear_win_path(state);
        return false;
    }

    if (state->prefs.current_puzzle_index >= collection->count)
    {
        state->prefs.current_puzzle_index = 0;
    }

    const puzzle_t *puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
    if (puzzle == NULL)
    {
        if (announce)
        {
            print_formatted_text(0, 5, "Puzzle load failed");
            print_formatted_text(0, 6, "");
            print_formatted_text(0, 7, "");
            clear_puzzle_hint();
        }
        game_state_clear_win_path(state);
        return false;
    }

    apply_puzzle_position(&state->board, puzzle);
    state->board.current_player = PLAYER_WHITE;
    state->board.move_count = 0;
    state->board.history_count = 0;

    state->prefs.swap_rule = puzzle->swap_rule;
    ai_difficulty_t difficulty = state->ai_config.difficulty;
    player_t ai_player = state->ai_config.ai_player;
    game_state_configure_ai(state, puzzle->swap_rule, difficulty, ai_player);

    game_state_clear_win_path(state);

    if (announce)
    {
        uint16_t total_puzzles = (collection->count > UINT16_MAX)
                                     ? UINT16_MAX
                                     : (uint16_t)collection->count;
        print_puzzle_info(state->prefs.current_puzzle_index, total_puzzles, puzzle->difficulty, puzzle->is_solved);
        clear_puzzle_hint();
    }

    return true;
}

void game_state_init(game_state_t *state)
{
    memset(state, 0, sizeof(game_state_t));

    ui_progress_init(&state->ui_progress);

    // Initialize board
    board_init(&state->board);
    state->board.layout_id = 0;

    // Set default preferences
    state->prefs.difficulty_level = AI_DIFFICULTY_EASY; // Easy Default
    state->prefs.swap_rule = SWAP_RULE_CLASSIC;
    state->prefs.current_puzzle_index = 0;
    state->prefs.color_scheme = 0; // Default theme
    state->prefs.ai_explanations_enabled = false;
    state->prefs.audio_enabled = true;
    state->prefs.volume_level = 7;

    // Difficulty not manually set initially
    state->difficulty_manually_set = false;

    // Initialize AI config - Classic swap rules, AI plays as Black (second player)
    game_state_configure_ai(state, state->prefs.swap_rule, state->prefs.difficulty_level, PLAYER_BLACK);

    // Initialize menu state
    game_state_update_menu_enables(state);
    state->menu.hovered_icon = -1;
    state->menu.selected_icon = -1;

    // Start at title
    state->phase = GAME_PHASE_TITLE;
}

void game_state_set_phase(game_state_t *state, game_phase_t phase)
{
    state->phase = phase;
}

game_phase_t game_state_get_phase(const game_state_t *state)
{
    return state->phase;
}

void game_state_set_game_mode(game_state_t *state, bool puzzle_mode)
{
    bool mode_changed = (state->is_puzzle_mode != puzzle_mode);
    state->is_puzzle_mode = puzzle_mode;

    if (state->is_puzzle_mode)
    {
        // Set AI difficulty to Expert when entering puzzle mode (always set default on mode change)
        if (mode_changed)
        {
            state->prefs.difficulty_level = AI_DIFFICULTY_EXPERT;
            state->ai_config.difficulty = AI_DIFFICULTY_EXPERT;
            state->ai_config.blunder_enabled = false;
            state->ai_config.blunder_chance_pct = 0u;
            state->ai_config.blunder_type = ai_allowed_blunder_type(AI_DIFFICULTY_EXPERT);
            state->ai_config.enable_forcing_check = true;
            state->difficulty_manually_set = false; // Reset manual flag since we're setting default
            
            print_ai_difficulty(state->prefs.difficulty_level);
        }

        // PUZZLE mode: initialize board to current puzzle
        if (!game_state_apply_current_puzzle(state, true))
        {
            // If no puzzles, fallback to freeplay
            board_set_starting_layout(&state->board, state->board.layout_id);
            board_clear_all_swapped(&state->board);
            clear_puzzle_info();
            clear_puzzle_hint();
            game_state_clear_win_path(state);
        }
    }
    else
    {
        // Set AI difficulty to Easy when entering Free Play mode (always set default on mode change)
        if (mode_changed)
        {
            state->prefs.difficulty_level = AI_DIFFICULTY_EASY;
            state->ai_config.difficulty = AI_DIFFICULTY_EASY;
            state->ai_config.blunder_enabled = true;
            state->ai_config.blunder_chance_pct = 15u;
            state->ai_config.blunder_type = ai_allowed_blunder_type(AI_DIFFICULTY_EASY);
            state->ai_config.enable_forcing_check = false;
            state->difficulty_manually_set = false; // Reset manual flag since we're setting default
            
            print_ai_difficulty(state->prefs.difficulty_level);
        }

        // FREEPLAY mode: standard initial position
        board_set_starting_layout(&state->board, state->board.layout_id);
        board_clear_all_swapped(&state->board);
        clear_puzzle_info();
        clear_puzzle_hint();
        game_state_clear_win_path(state);
    }

    game_state_deselect_piece(state);
    game_state_update_menu_enables(state);

    // Disable blunders in puzzle mode
    if (state->is_puzzle_mode) {
        state->ai_config.blunder_enabled = false;
        state->ai_config.use_hint_profile = true;
    } else {
        state->ai_config.use_hint_profile = false;
    }

    // Reset board cell colors to original checkerboard pattern
    video_reset_all_board_cell_colors();
    video_set_game_mode_icon_bitmap(state->is_puzzle_mode);
}

void game_state_start_new_game(game_state_t *state)
{
    clear_made_blunder();
    game_state_set_game_mode(state, false); // Always start in FREEPLAY mode
    state->phase = GAME_PHASE_PLAYING;
}

void game_state_select_piece(game_state_t *state, uint8_t row, uint8_t col)
{
    // Get piece at location
    piece_type_t piece = board_get_piece_unchecked(&state->board, row, col);

    // Check if it belongs to current player
    if (board_get_piece_owner(piece) != state->board.current_player)
    {
        return;
    }

    // Get legal moves first
    move_array_t soa_moves;
    uint8_t legal_move_count = board_get_legal_moves_soa(&state->board, row, col, &soa_moves);
    for (uint8_t i = 0; i < legal_move_count; ++i) {
        move_array_get_move(&soa_moves, i, &state->selection.legal_moves[i]);
    }

    // Only select if there are legal moves
    if (legal_move_count == 0)
    {
        return;
    }

    // Select the piece
    state->selection.has_selection = true;
    state->selection.selected_row = row;
    state->selection.selected_col = col;
    state->selection.hovered_move = -1;
    state->selection.legal_move_count = legal_move_count;
}

void game_state_deselect_piece(game_state_t *state)
{
    state->selection.has_selection = false;
    state->selection.legal_move_count = 0;
    state->selection.hovered_move = -1;
}

bool game_state_execute_selected_move(game_state_t *state, uint8_t move_index)
{
    if (!state->selection.has_selection ||
        move_index >= state->selection.legal_move_count)
    {
        return false;
    }

    move_t *move = &state->selection.legal_moves[move_index];

    if (board_execute_move(&state->board, move, state->prefs.swap_rule))
    {
        game_state_deselect_piece(state);

        // Check for win
        if (game_state_check_win_condition(state))
        {
            clear_made_blunder();
            state->phase = GAME_PHASE_GAME_OVER;
        }
        else
        {
            // Switch turns
            board_switch_turn(&state->board);

            // If AI's turn, switch to AI thinking phase
            if (state->board.current_player == PLAYER_BLACK)
            {
                state->phase = GAME_PHASE_AI_THINKING;
            }
        }

        // Update menu enables
        game_state_update_menu_enables(state);
        return true;
    }
    
    return false;
}

void game_state_update_menu_enables(game_state_t *state)
{

    state->menu.enabled[MENU_ICON_GAME_MODE] = true;  
    state->menu.enabled[MENU_ICON_RESET] = true;
    state->menu.enabled[MENU_ICON_PREVIOUS] = true;
    state->menu.enabled[MENU_ICON_NEXT] = true; 
    state->menu.enabled[MENU_ICON_SWAP] = true;  
    state->menu.enabled[MENU_ICON_DIFFICULTY] = true;
    state->menu.enabled[MENU_ICON_HINT] = true;
    state->menu.enabled[MENU_ICON_EXIT] = true;

}

void game_state_activate_menu_icon(game_state_t *state, menu_icon_t icon)
{
    bool new_mode=state->is_puzzle_mode;

    switch (icon)
    {
        case MENU_ICON_GAME_MODE:
    
            // Toggle mode and fall through to RESET
            new_mode = !state->is_puzzle_mode;

        case MENU_ICON_RESET:

            if (new_mode != state->is_puzzle_mode) {
                // Mode is changing, set new mode (which may change difficulty to default)
                game_state_set_game_mode(state, new_mode);
            } else {
                // Same mode, just reset the board without changing difficulty
                if (state->is_puzzle_mode) {
                    // Reset puzzle mode - reload current puzzle
                    game_state_apply_current_puzzle(state, false);
                } else {
                    // Reset freeplay mode - reset to starting layout
                    board_set_starting_layout(&state->board, state->board.layout_id);
                    board_clear_all_swapped(&state->board);
                    clear_puzzle_info();
                    clear_puzzle_hint();
                    game_state_clear_win_path(state);
                }
                game_state_deselect_piece(state);
                game_state_update_menu_enables(state);
                // Reset board cell colors to original checkerboard pattern
                video_reset_all_board_cell_colors();
            }
            print_game_mode(new_mode);
            if(!state->is_puzzle_mode) {
                clear_swap_unavailable();
            }
            print_current_player(state->board.current_player);
            print_swap_rule(state->prefs.swap_rule);
            state->phase = GAME_PHASE_PLAYING;
        break;
        
        case MENU_ICON_NEXT:
        case MENU_ICON_PREVIOUS:

            // if in puzzle mode, load next puzzle
            if (state->is_puzzle_mode)
            {
                const puzzle_collection_t *collection = get_puzzle_collection();

                if (!collection || collection->count == 0u)
                {
                    game_state_clear_win_path(state);
                    break;
                }
                
                if (icon == MENU_ICON_NEXT) {
                    state->prefs.current_puzzle_index = (state->prefs.current_puzzle_index + 1u) % collection->count;
                } else {
                    // PREVIOUS
                    if (state->prefs.current_puzzle_index == 0u) {
                        state->prefs.current_puzzle_index = collection->count - 1u;
                    } else {
                        state->prefs.current_puzzle_index = state->prefs.current_puzzle_index - 1u;
                    }
                }


                if (game_state_apply_current_puzzle(state, true))
                {
                    game_state_deselect_piece(state);
                    game_state_update_menu_enables(state);
                    print_swap_rule(state->prefs.swap_rule);
                    state->phase = GAME_PHASE_PLAYING;
                } 
            } else {
                // In freeplay mode, 
                // Increment layout_id and wrap around
                state->board.layout_id = (state->board.layout_id +
                    (icon == MENU_ICON_NEXT ? 1u : -1u)) % NUM_STARTING_LAYOUTS;
                    board_set_starting_layout(&state->board, state->board.layout_id);
                    board_clear_all_swapped(&state->board);
                    game_state_clear_win_path(state);
                    game_state_deselect_piece(state);
                    game_state_update_menu_enables(state);
                    state->phase = GAME_PHASE_PLAYING;
                    clear_puzzle_info();
                    clear_puzzle_hint();
            }
            print_current_player(state->board.current_player);
            break;
            
        case MENU_ICON_SWAP:
  
            game_state_toggle_swap_rule(state);
            state->phase = GAME_PHASE_PLAYING;
            break; 
            
        case MENU_ICON_DIFFICULTY:
            // Cycle difficulty
            state->prefs.difficulty_level = (state->prefs.difficulty_level + 1) % 4;
            // Mark that difficulty has been manually set
            state->difficulty_manually_set = true;
            // Update AI difficulty
            state->ai_config.difficulty = (ai_difficulty_t)state->prefs.difficulty_level;
            // Update blunder settings based on new difficulty
            switch (state->prefs.difficulty_level) {
                case AI_DIFFICULTY_LEARNING:
                    state->ai_config.blunder_enabled = true;
                    state->ai_config.blunder_chance_pct = 20u;
                    break;
                case AI_DIFFICULTY_EASY:
                    state->ai_config.blunder_enabled = true;
                    state->ai_config.blunder_chance_pct = 15u;
                    break;
                case AI_DIFFICULTY_STANDARD:
                    state->ai_config.blunder_enabled = true;
                    state->ai_config.blunder_chance_pct = 10u;
                    break;
                case AI_DIFFICULTY_EXPERT:
                default:
                    state->ai_config.blunder_enabled = false;
                    state->ai_config.blunder_chance_pct = 0u;
                    break;
            }
            state->ai_config.blunder_type = ai_allowed_blunder_type(state->ai_config.difficulty);
            print_ai_difficulty(state->prefs.difficulty_level);
            break;

        case MENU_ICON_HINT:
            {
                // Display the first solution move for the currently selected puzzle (hint)
                const puzzle_collection_t *collection = get_puzzle_collection();
                const puzzle_t *puzzle = NULL;
                if (collection && state->prefs.current_puzzle_index < collection->count)
                {
                    puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
                }
                // If in puzzle mode and puzzle has solution and no moves made, show solution
                // otherwise, show AI hint for Player A (White)
                if (state->is_puzzle_mode && state->board.move_count == 0)
                {
                    display_puzzle_solution(puzzle);
                } else {
                    // Display AI Agent suggested move for Player A (White)
                    set_mouse_cursor(MOUSE_CURSOR_BUSY);
                    move_t ai_move;
                    ai_config_t ai_config = state->ai_config;
                    ai_config.ai_player = PLAYER_WHITE;
                    ai_config.swap_rule = state->prefs.swap_rule;
                    ai_config.use_hint_profile = state->is_puzzle_mode;
                    bool ai_found = ai_agent_find_best_move(&state->board, &ai_config, &ai_move);
                    if (ai_found)
                    {
                        char ai_hint_buf[26];
                        format_move_string(ai_hint_buf, sizeof(ai_hint_buf), &ai_move);
                        print_AI_hint(ai_hint_buf);
                    }
                    else
                    {
                        print_AI_hint("No AI move available");
                    }
                    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
                }

                state->phase = GAME_PHASE_PLAYING;
            }
            break;

        case MENU_ICON_EXIT:
            state->phase = GAME_PHASE_EXIT;
            break;

        default:
            break;
    }
}

bool game_state_check_win_condition(game_state_t *state)
{
    // Check both players for win condition
    bool white_wins = board_check_win_with_path(&state->board, PLAYER_WHITE, &state->win_path);

    if (white_wins)
    {
        state->stats.white_wins++;
        return true;
    }

    win_path_t black_path;
    bool black_wins = board_check_win_with_path(&state->board, PLAYER_BLACK, &black_path);

    if (black_wins)
    {
        state->stats.black_wins++;
        state->win_path = black_path;
        return true;
    }

    return false;
}

void game_state_update(game_state_t *state, float delta_time)
{
    (void)delta_time;
    state->frame_count++;

    // Phase-specific updates
    switch (state->phase)
    {
    case GAME_PHASE_AI_THINKING:
    {
        // Clear any previous blunder message
        clear_made_blunder();
        
        // Add a small visual delay before AI makes move
        state->ai_think_frames++;
        // Wait at least 30 frames (~0.5 seconds) before executing AI move
        if (state->ai_think_frames >= 30)
        {
            move_t ai_move;
            state->ai_config.swap_rule = state->prefs.swap_rule;
            state->ai_config.ai_player = state->board.current_player;

            bool ai_moved = false;
            if (ai_agent_find_best_move(&state->board, &state->ai_config, &ai_move))
            {
                if (board_execute_move(&state->board, &ai_move, state->ai_config.swap_rule))
                {
                    ai_moved = true;
                    print_formatted_text(0, 21, "                    ");

                    if (game_state_check_win_condition(state))
                    {
                        clear_made_blunder();
                        state->phase = GAME_PHASE_GAME_OVER;
                    }
                    else
                    {
                        board_switch_turn(&state->board);
                        state->phase = GAME_PHASE_PLAYING;
                    }
                }
            }

            if (!ai_moved)
            {
                print_formatted_text(0, 21, "AI HAS NO MOVES     ");
                board_switch_turn(&state->board);
                state->phase = GAME_PHASE_PLAYING;
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
