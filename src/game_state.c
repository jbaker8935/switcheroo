/**
 * @file game_state.c
 * @brief Game state management implementation
 */

#include "game_state.h"
#include "puzzle_data.h"
#include "ai_agent.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "text_display.h"
#include "mouse_pointer.h"
#include "board.h"
#include "sound.h"
#include "overlay_config.h"

extern void render_invalidate_cache(void);
extern void video_reset_all_board_cell_colors(void);
extern void video_set_game_mode_icon_bitmap(bool is_puzzle_mode);
extern void refresh_win_path(const win_path_t *path);

static void game_state_clear_win_path(game_state_t *state);
static bool game_state_restore_win_path(game_state_t *state);
static void game_state_record_freeplay_snapshot(game_state_t *state, bool allow_ai_turn);

static void game_state_history_refresh_ui(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    const bool is_free_play = !state->is_puzzle_mode;
    const bool has_start_entry = is_free_play && freeplay_history_has_start_entry(&state->history_state);
    uint16_t board_move_count = state->board.move_count;
    uint16_t live_move_count = board_move_count;

    if (is_free_play && state->history_state.count > 0u)
    {
        live_move_count = state->history_state.entries[0].board.move_count;
    }

    // Print either the current player or the puzzle move-limit message
    // using a centralized helper so that the logic is not duplicated.
    game_state_print_current_player(state);
    print_move_history(state->context.history,
                       state->context.history_count,
                       board_move_count,
                       live_move_count,
                       is_free_play,
                       has_start_entry);
}

bool game_state_has_exceeded_puzzle_moves(const game_state_t *state)
{
    if (!state || !state->is_puzzle_mode)
    {
        return false;
    }

    const puzzle_t *puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
    if (!puzzle)
    {
        return false;
    }

    // Player (WHITE) move count is floor((move_count + 1) / 2)
    const uint8_t white_moves = (uint8_t)((state->board.move_count + 1u) / 2u);
    return (white_moves > puzzle->difficulty);
}

void game_state_print_current_player(const game_state_t *state)
{
    if (!state) return;

    if (state->is_puzzle_mode && state->context.current_player == PLAYER_WHITE &&
        game_state_has_exceeded_puzzle_moves(state))
    {
        // Show the disqualification message instead of player's move.
        // Delegate to text_display so it respects existing state such as
        // the blunder message.
        print_too_many_moves();
        return;
    }

    print_current_player(state->context.current_player);
}

static void game_state_record_freeplay_snapshot(game_state_t *state, bool allow_ai_turn)
{
    if (!state || state->is_puzzle_mode)
    {
        return;
    }

    if (!allow_ai_turn &&
        state->ai_config.ai_player != PLAYER_NONE &&
        state->context.current_player == state->ai_config.ai_player)
    {
        return;
    }

    freeplay_history_capture_live(&state->history_state, &state->board, &state->context);
}

static void game_state_handle_history_branch(game_state_t *state)
{
    if (!state || state->is_puzzle_mode)
    {
        return;
    }

    const uint8_t target_view = freeplay_history_view_index(&state->history_state);

    if (target_view == 0u)
    {
        return;
    }

    uint16_t moves_to_trim = 0u;
    if (target_view < freeplay_history_snapshot_count(&state->history_state))
    {
        const freeplay_history_entry_t *latest_entry = &state->history_state.entries[0];
        const freeplay_history_entry_t *target_entry = &state->history_state.entries[target_view];

        if (latest_entry->board.move_count > target_entry->board.move_count)
        {
            moves_to_trim = (uint16_t)(latest_entry->board.move_count - target_entry->board.move_count);
        }
    }

    const uint8_t removed = freeplay_history_prepare_branch(&state->history_state);
    if (removed == 0u)
    {
        return;
    }

    achievements_on_freeplay_history_branch(&state->achievements);

    if (moves_to_trim == 0u)
    {
        moves_to_trim = (uint16_t)removed * 2u;
    }

    if (moves_to_trim >= state->context.history_count)
    {
        memset(state->context.history, 0, sizeof(state->context.history));
        state->context.history_count = 0u;
        state->context.last_moving_player = PLAYER_NONE;
        return;
    }

    const uint8_t trimmed_count = (uint8_t)moves_to_trim;
    const uint8_t remaining = (uint8_t)(state->context.history_count - trimmed_count);
    memmove(state->context.history,
            state->context.history + trimmed_count,
            (size_t)remaining * sizeof(state->context.history[0]));
    memset(state->context.history + remaining,
           0,
           (size_t)(MAX_MOVE_HISTORY - remaining) * sizeof(state->context.history[0]));
    state->context.history_count = remaining;
    state->context.last_moving_player = remaining > 0u
                                            ? state->context.history[0].player
                                            : PLAYER_NONE;
}

static void game_state_after_history_navigation(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    state->phase = GAME_PHASE_PLAYING;
    state->ai_think_frames = 0u;
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    clear_made_blunder();
    game_state_deselect_piece(state);
    game_state_clear_win_path(state);
    game_state_update_menu_enables(state);
    game_state_history_refresh_ui(state);

    if (game_state_restore_win_path(state))
    {
        state->phase = GAME_PHASE_GAME_OVER;
        if (state->is_puzzle_mode && state->win_path.winner == PLAYER_WHITE &&
            game_state_has_exceeded_puzzle_moves(state)) {
            print_game_over();
        } else {
            print_game_winner(state->win_path.winner);
        }
    }
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

static bool game_state_restore_win_path(game_state_t *state)
{
    if (!state)
    {
        return false;
    }

    win_path_t path;
    if (board_check_win_with_path(&state->board, PLAYER_WHITE, &path))
    {
        state->win_path = path;
        refresh_win_path(&state->win_path);
        return true;
    }

    if (board_check_win_with_path(&state->board, PLAYER_BLACK, &path))
    {
        state->win_path = path;
        refresh_win_path(&state->win_path);
        return true;
    }

    return false;
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

static void game_state_play_sound(game_state_t *state, sound_id_t id)
{
    if (!state)
    {
        return;
    }

    if (!state->prefs.audio_enabled)
    {
        return;
    }

    play_sound(id);
}

static void game_state_reset_move_history(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    memset(state->context.history, 0, sizeof(state->context.history));
    state->context.history_count = 0;
    state->context.last_moving_player = PLAYER_NONE;

    if (state->is_puzzle_mode)
    {
        freeplay_history_reset(&state->history_state, NULL, NULL);
    }
    else
    {
        state->context.current_player = PLAYER_WHITE;
        freeplay_history_reset(&state->history_state, &state->board, &state->context);
        achievements_on_freeplay_history_reset(&state->achievements);
    }
}

static void game_state_focus_first_unsolved_puzzle(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    set_current_puzzle_swap_rule(state->prefs.swap_rule);

    const puzzle_collection_t *collection = get_puzzle_collection();
    if (!collection || collection->count == 0u)
    {
        state->prefs.current_puzzle_index = 0u;
        return;
    }

    uint16_t first_unsolved_index = 0u;
    bool found_unsolved = false;
    for (uint16_t i = 0u; i < collection->count; ++i)
    {
        const puzzle_t *puzzle = get_puzzle_by_index(i);
        if (puzzle && !puzzle->is_solved)
        {
            first_unsolved_index = i;
            found_unsolved = true;
            break;
        }
    }

    state->prefs.current_puzzle_index = found_unsolved ? first_unsolved_index : 0u;
}

bool game_state_apply_current_puzzle(game_state_t *state, bool announce)
{
    achievements_on_puzzle_failed(&state->achievements);
    game_state_clear_win_path(state);
    set_current_puzzle_swap_rule(state->prefs.swap_rule);

    const puzzle_collection_t *collection = get_puzzle_collection();
    if (!collection || collection->count == 0u)
    {
        return false;
    }

    if (state->prefs.current_puzzle_index >= collection->count)
    {
        return false;
    }

    const puzzle_t *puzzle = get_puzzle_by_index(state->prefs.current_puzzle_index);
    if (!puzzle)
    {
        return false;
    }

    apply_puzzle_position(&state->board, puzzle);
    game_state_reset_move_history(state);
    state->context.current_player = PLAYER_WHITE;

    ai_difficulty_t difficulty = AI_DIFFICULTY_STANDARD; // Default for puzzle mode
    if (state->difficulty_manually_set) {
        difficulty = (ai_difficulty_t)state->prefs.difficulty_level;
    } 

    player_t ai_player = puzzle->is_solved ? PLAYER_BLACK : PLAYER_WHITE;
    game_state_configure_ai(state, state->prefs.swap_rule, difficulty, ai_player);
    if (state->is_puzzle_mode) {
        state->ai_config.blunder_enabled = false;
        state->ai_config.blunder_chance_pct = 0u;
        state->ai_config.use_hint_profile = true;
    }

    if (announce)
    {
        uint16_t total_puzzles = (collection->count > UINT16_MAX)
                                      ? UINT16_MAX
                                      : (uint16_t)collection->count;
        print_puzzle_info(state->prefs.current_puzzle_index, total_puzzles, puzzle->difficulty, puzzle->is_solved);
    }

    achievements_on_puzzle_loaded(&state->achievements, puzzle);


    return true;
}

static void game_state_toggle_swap_rule(game_state_t *state)
{
    if (!state)
    {
        return;
    }

    if (state->is_puzzle_mode) {
        // In puzzle mode, allow changing swap rule and reload puzzle
        state->prefs.swap_rule = (state->prefs.swap_rule + 1) % NUMBER_OF_SWAP_RULES;
        game_state_focus_first_unsolved_puzzle(state);
        game_state_apply_current_puzzle(state, true);
        print_swap_rule(state->prefs.swap_rule);
    } else if (state->phase == GAME_PHASE_PLAYING && state->board.move_count == 0) {
        // In free play, only change if no moves made
        state->prefs.swap_rule = (state->prefs.swap_rule + 1) % NUMBER_OF_SWAP_RULES;
        ai_difficulty_t difficulty = state->ai_config.difficulty;
        player_t ai_player = state->ai_config.ai_player;
        game_state_configure_ai(state, state->prefs.swap_rule, difficulty, ai_player);
        print_swap_rule(state->prefs.swap_rule);
        clear_swap_unavailable();
        game_state_reset_move_history(state);
        game_state_history_refresh_ui(state);
    } else {
        // Cannot change swap rule in free play after moves
        print_swap_unavailable();
    }
}


void game_state_init(game_state_t *state)
{
    memset(state, 0, sizeof(game_state_t));

    freeplay_history_init(&state->history_state);
    achievements_init(&state->achievements);
    ui_progress_init(&state->ui_progress);

    // Initialize board
    board_init(&state->board);
    game_state_reset_move_history(state);
    state->context.layout_id = 0;
    state->context.current_player = PLAYER_WHITE;

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

    achievements_refresh_catalog(&state->achievements,
                                  state->prefs.swap_rule,
                                  state->prefs.current_puzzle_index);

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

#pragma code(ovl12_code)
void FAR_game_state_set_game_mode(game_state_t *state, bool puzzle_mode)
{
    bool mode_changed = (state->is_puzzle_mode != puzzle_mode);
    bool previous_mode = state->is_puzzle_mode;
    state->is_puzzle_mode = puzzle_mode;

    if (mode_changed && !previous_mode && puzzle_mode)
    {
        achievements_on_freeplay_history_reset(&state->achievements);
    }

    achievements_on_game_mode_changed(&state->achievements, previous_mode, puzzle_mode);

    if (state->is_puzzle_mode)
    {
        // Set AI difficulty to Standard when entering puzzle mode (always set default on mode change)
        if (mode_changed)
        {
            state->prefs.difficulty_level = AI_DIFFICULTY_STANDARD;
            state->ai_config.difficulty = AI_DIFFICULTY_STANDARD;
            state->ai_config.blunder_enabled = false;
            state->ai_config.blunder_chance_pct = 0u;
            state->ai_config.blunder_type = ai_allowed_blunder_type(AI_DIFFICULTY_STANDARD);
            state->ai_config.enable_forcing_check = true;
            state->difficulty_manually_set = false; // Reset manual flag since we're setting default

            print_ai_difficulty(state->prefs.difficulty_level);

            game_state_focus_first_unsolved_puzzle(state);
        }

        // PUZZLE mode: initialize board to current puzzle

        if (!game_state_apply_current_puzzle(state, true))
        {
            // If no puzzles, fallback to freeplay
            board_set_starting_layout(&state->board, state->context.layout_id);
            board_clear_all_swapped(&state->board);
            clear_puzzle_info();
            clear_puzzle_hint();
            game_state_clear_win_path(state);
            set_mouse_cursor(MOUSE_CURSOR_NORMAL);

        }

        // Clear move history when switching to puzzle mode
        game_state_reset_move_history(state);
        state->context.current_player = PLAYER_WHITE;
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

            // Ensure free play snapshots only capture player-to-move states by resetting the AI side.
            game_state_configure_ai(state,
                                    state->prefs.swap_rule,
                                    state->prefs.difficulty_level,
                                    PLAYER_BLACK);
        }

        // FREEPLAY mode: standard initial position
        board_set_starting_layout(&state->board, state->context.layout_id);
        board_clear_all_swapped(&state->board);
        clear_puzzle_info();
        clear_puzzle_hint();
        game_state_clear_win_path(state);
        set_mouse_cursor(MOUSE_CURSOR_NORMAL);

        // Clear move history when switching to freeplay mode
        game_state_reset_move_history(state);
        state->context.current_player = PLAYER_WHITE;
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
    game_state_history_refresh_ui(state);
}
#pragma code(code)

void game_state_set_game_mode(game_state_t *state, bool puzzle_mode) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_12);
    FAR_game_state_set_game_mode(state, puzzle_mode);
    POKE(OVERLAY_MMU_REG, saved);
}

void game_state_start_new_game(game_state_t *state)
{
    clear_made_blunder();
    game_state_set_game_mode(state, true); // Always start in PUZZLE mode
    state->phase = GAME_PHASE_PLAYING;
}

void game_state_select_piece(game_state_t *state, uint8_t row, uint8_t col)
{
    // Get piece at location
    piece_type_t piece = board_get_piece_unchecked(&state->board, row, col);

    // Check if it belongs to current player
    if (board_get_piece_owner(piece) != state->context.current_player)
    {
        return;
    }

    // Get legal moves first
    move_array_t soa_moves;
    uint8_t legal_move_count = board_get_legal_moves_soa(&state->board, state->context.current_player, row, col, &soa_moves);
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

    if (!state->is_puzzle_mode && freeplay_history_view_index(&state->history_state) > 0u)
    {
        game_state_handle_history_branch(state);
    }

    if (board_execute_move(&state->board, &state->context, move, state->prefs.swap_rule))
    {
        game_state_deselect_piece(state);

        // Check for win
        bool player_won = game_state_check_win_condition(state);
        if (player_won)
        {
            if (state->win_path.winner == PLAYER_WHITE)
            {
                game_state_play_sound(state, SOUND_ID_WIN);
            }
            else if (state->win_path.winner == PLAYER_BLACK)
            {
                game_state_play_sound(state, SOUND_ID_LOSS);
            }
            clear_made_blunder();
            state->phase = GAME_PHASE_GAME_OVER;
        }
        else
        {
            game_state_play_sound(state, SOUND_ID_MOVE);
            // Switch turns
            board_switch_turn(&state->context);

            // If AI's turn, switch to AI thinking phase
            if (state->context.current_player == PLAYER_BLACK)
            {
                state->phase = GAME_PHASE_AI_THINKING;
            }
        }

        if (!state->is_puzzle_mode)
        {
            clear_swap_unavailable();
            game_state_record_freeplay_snapshot(state, false);
        }

        // Update menu enables
        game_state_update_menu_enables(state);
        game_state_history_refresh_ui(state);
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

#pragma code(ovl12_code)
void FAR_game_state_activate_menu_icon(game_state_t *state, menu_icon_t icon)
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
                    state->phase = GAME_PHASE_PLAYING;
                } else {
                    // Reset freeplay mode - reset to starting layout
                    board_set_starting_layout(&state->board, state->context.layout_id);
                    board_clear_all_swapped(&state->board);
                    clear_puzzle_info();
                    clear_puzzle_hint();
                    game_state_clear_win_path(state);
                    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
                    game_state_reset_move_history(state);
                    state->context.current_player = PLAYER_WHITE;
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
            game_state_history_refresh_ui(state);
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
                state->context.layout_id = (state->context.layout_id +
                    (icon == MENU_ICON_NEXT ? 1u : -1u)) % NUM_STARTING_LAYOUTS;
                board_set_starting_layout(&state->board, state->context.layout_id);
                board_clear_all_swapped(&state->board);
                game_state_clear_win_path(state);
                game_state_reset_move_history(state);
                state->context.current_player = PLAYER_WHITE;
                game_state_deselect_piece(state);
                game_state_update_menu_enables(state);
                state->phase = GAME_PHASE_PLAYING;
                clear_puzzle_info();
                clear_puzzle_hint();
                set_mouse_cursor(MOUSE_CURSOR_NORMAL);
            }
            game_state_history_refresh_ui(state);
            break;
            
        case MENU_ICON_SWAP:
  
            if (state->phase != GAME_PHASE_GAME_OVER) {
                game_state_toggle_swap_rule(state);
                state->phase = GAME_PHASE_PLAYING;
            }
            break; 
            
        case MENU_ICON_DIFFICULTY:
            // Cycle difficulty
            state->prefs.difficulty_level = (state->prefs.difficulty_level + 1) % 4;
            // Mark that difficulty has been manually set
            state->difficulty_manually_set = true;
            {
                ai_difficulty_t new_difficulty = (ai_difficulty_t)state->prefs.difficulty_level;
                player_t ai_player = state->ai_config.ai_player;
                game_state_configure_ai(state, state->prefs.swap_rule, new_difficulty, ai_player);
                if (state->is_puzzle_mode) {
                    state->ai_config.blunder_enabled = false;
                    state->ai_config.blunder_chance_pct = 0u;
                    state->ai_config.use_hint_profile = true;
                }
            }
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
                    ai_config.use_hint_profile = true;
                    ai_config.blunder_enabled = false;
                    ai_config.random_top_k = 1u;
                    ai_config.random_epsilon_pct = 0u;
                    bool ai_found = ai_agent_find_best_move(&state->board, &state->context, &ai_config, &ai_move);
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

                if (state->is_puzzle_mode)
                {
                    achievements_on_puzzle_hint(&state->achievements);
                }
            }

            break;

        case MENU_ICON_EXIT:
            state->phase = GAME_PHASE_EXIT;
            break;

        default:
            break;
    }
}
#pragma code(code)

void game_state_activate_menu_icon(game_state_t *state, menu_icon_t icon)
{
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_12);
    FAR_game_state_activate_menu_icon(state, icon);
    POKE(OVERLAY_MMU_REG, saved);
}

bool game_state_check_win_condition(game_state_t *state)
{
    // Check both players for win condition
    bool white_wins = board_check_win_with_path(&state->board, PLAYER_WHITE, &state->win_path);

    if (white_wins)
    {
        // In puzzle mode, a player win that occurs after exceeding the
        // allowable number of moves should not count as a win in the
        // session statistics. Achievements are marked as failed.
        if (state->is_puzzle_mode && game_state_has_exceeded_puzzle_moves(state))
        {
            achievements_on_puzzle_failed(&state->achievements);
            render_invalidate_cache();
            return true;
        }

        state->stats.white_wins++;
        if (!state->is_puzzle_mode)
        {
            achievements_on_freeplay_win(&state->achievements,
                                          state->context.layout_id,
                                          state->prefs.swap_rule,
                                          state->ai_config.difficulty,
                                          state->board.move_count);
        }
        render_invalidate_cache();
        return true;
    }

    win_path_t black_path;
    bool black_wins = board_check_win_with_path(&state->board, PLAYER_BLACK, &black_path);

    if (black_wins)
    {
        if (state->is_puzzle_mode)
        {
            achievements_on_puzzle_failed(&state->achievements);
        }

        state->stats.black_wins++;
        state->win_path = black_path;
        render_invalidate_cache();
        return true;
    }

    return false;
}

#pragma code(ovl12_code)
void FAR_game_state_update(game_state_t *state, float delta_time)
{
    (void)delta_time;


    // Phase-specific updates
    switch (state->phase)
    {
    case GAME_PHASE_AI_THINKING:
    {
        // Clear any previous blunder message
        clear_made_blunder();
        
        // Set cursor to busy during AI thinking
        set_mouse_cursor(MOUSE_CURSOR_BUSY);
        
        // Add a small visual delay before AI makes move
        state->ai_think_frames++;
        // Wait at least 30 frames (~0.5 seconds) before executing AI move
        if (state->ai_think_frames >= 30)
        {
            move_t ai_move;
            state->ai_config.swap_rule = state->prefs.swap_rule;
            state->ai_config.ai_player = state->context.current_player;

            bool ai_moved = false;
            if (ai_agent_find_best_move(&state->board, &state->context, &state->ai_config, &ai_move))

            {
                if (board_execute_move(&state->board, &state->context, &ai_move, state->ai_config.swap_rule))
                {
                    ai_moved = true;
                    
                    bool ai_won = game_state_check_win_condition(state);
                    if (ai_won)
                    {
                        if (state->win_path.winner == PLAYER_BLACK)
                        {
                            game_state_play_sound(state, SOUND_ID_LOSS);
                        }
                        else if (state->win_path.winner == PLAYER_WHITE)
                        {
                            game_state_play_sound(state, SOUND_ID_WIN);
                        }
                        if (!state->is_puzzle_mode)
                        {
                            game_state_record_freeplay_snapshot(state, true);
                        }
                        game_state_history_refresh_ui(state);
                        clear_made_blunder();
                        state->phase = GAME_PHASE_GAME_OVER;
                        set_mouse_cursor(MOUSE_CURSOR_NORMAL);
                        state->ai_think_frames = 0;
                        game_state_update_menu_enables(state);
                        break;
                    }
                    else
                    {
                        game_state_play_sound(state, SOUND_ID_MOVE);
                        board_switch_turn(&state->context);
                        state->phase = GAME_PHASE_PLAYING;
                        if (!state->is_puzzle_mode)
                        {
                            game_state_record_freeplay_snapshot(state, false);
                        }
                        game_state_history_refresh_ui(state);
                    }
                }
            }

            if (!ai_moved)
            {
                board_switch_turn(&state->context);
                state->phase = GAME_PHASE_PLAYING;
                game_state_history_refresh_ui(state);
            }

            set_mouse_cursor(MOUSE_CURSOR_NORMAL);
            state->ai_think_frames = 0;
            game_state_update_menu_enables(state);
        }
        break;
    }

    default:
        break;
    }
}
#pragma code(code)

void game_state_update(game_state_t *state, float delta_time) {
    volatile uint8_t saved = PEEK(OVERLAY_MMU_REG);
    POKE(OVERLAY_MMU_REG, BLOCK_12);
    FAR_game_state_update(state, delta_time);
    POKE(OVERLAY_MMU_REG, saved);
}

bool game_state_step_history_back(game_state_t *state)
{
    if (!state || state->is_puzzle_mode || state->phase == GAME_PHASE_AI_THINKING)
    {
        return false;
    }

    if (!freeplay_history_step_backward(&state->history_state, &state->board, &state->context))
    {
        return false;
    }

    game_state_after_history_navigation(state);
    return true;
}

bool game_state_step_history_forward(game_state_t *state)
{
    if (!state || state->is_puzzle_mode || state->phase == GAME_PHASE_AI_THINKING)
    {
        return false;
    }

    if (!freeplay_history_step_forward(&state->history_state, &state->board, &state->context))
    {
        return false;
    }

    game_state_after_history_navigation(state);
    return true;
}

uint8_t game_state_get_history_view_index(const game_state_t *state)
{
    if (!state || state->is_puzzle_mode)
    {
        return 0u;
    }

    return freeplay_history_view_index(&state->history_state);
}

bool game_state_is_history_live(const game_state_t *state)
{
    if (!state || state->is_puzzle_mode)
    {
        return true;
    }

    return freeplay_history_is_live(&state->history_state);
}

