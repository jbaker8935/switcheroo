/**
 * @file game_state.h
 * @brief Game state management for F256 Switcharoo
 * 
 * Integrates board model, menu state, session stats, and user preferences.
 */

#ifndef GAME_STATE_H
#define GAME_STATE_H

#ifdef AI_AGENT_HOST_TEST
#include "../tests/include/f256lib_host.h"
#else
#include "platform_f256.h"
#endif
#include "../src/board.h"
#include "../src/ai_agent.h"
#include "../src/ui_progress.h"
#include <stdint.h>

// Game phases
typedef enum {
    GAME_PHASE_TITLE,
    GAME_PHASE_PLAYING,
    GAME_PHASE_AI_THINKING,
    GAME_PHASE_GAME_OVER,
    GAME_PHASE_MENU_OVERLAY,
    GAME_PHASE_EXIT
} game_phase_t;

// Menu icons
typedef enum {
    MENU_ICON_GAME_MODE = 0,
    MENU_ICON_RESET = 1,
    MENU_ICON_PREVIOUS = 2,
    MENU_ICON_NEXT = 3,
    MENU_ICON_SWAP = 4,
    MENU_ICON_DIFFICULTY = 5,
    MENU_ICON_HINT = 6,
    MENU_ICON_EXIT = 7,
    MENU_ICON_COUNT = 8
} menu_icon_t;

// Session statistics
typedef struct {
    uint8_t white_wins;
    uint8_t black_wins;
} session_stats_t;

// User preferences
typedef struct {
    uint8_t difficulty_level;     // 0=Learning, 1=Easy, 2=Standard, 3=Expert
    swap_rule_t swap_rule;        // Current swap rule preference
    uint16_t current_puzzle_index; // Index of currently selected puzzle
    uint8_t color_scheme;         // Theme index
    bool ai_explanations_enabled;
    bool audio_enabled;
    uint8_t volume_level;         // 0-10
} user_preferences_t;

// Menu state
typedef struct {
    bool enabled[MENU_ICON_COUNT];
    int8_t hovered_icon;          // -1 if none
    int8_t selected_icon;         // -1 if none
} menu_state_t;

// Selection state
typedef struct {
    bool has_selection;
    uint8_t selected_row;
    uint8_t selected_col;
    uint8_t legal_move_count;
    move_t legal_moves[8];        // Max 8 adjacent cells
    int8_t hovered_move;          // Index into legal_moves, -1 if none
} selection_state_t;

// Complete game state
typedef struct {
    game_phase_t phase;
    board_t board;
    board_context_t context;
    session_stats_t stats;
    user_preferences_t prefs;
    menu_state_t menu;
    selection_state_t selection;
    win_path_t win_path;
    uint32_t frame_count;
    ai_config_t ai_config;
    uint8_t ai_think_frames;      // Frames spent in AI thinking (for visual delay)
    bool is_puzzle_mode;          // true = PUZZLE mode, false = FREEPLAY mode
    bool difficulty_manually_set; // true if user has manually changed difficulty
    ui_progress_state_t ui_progress;
} game_state_t;

// Initialize game state
void game_state_init(game_state_t *state);

// Phase management
void game_state_set_phase(game_state_t *state, game_phase_t phase);
game_phase_t game_state_get_phase(const game_state_t *state);

// Board operations
void game_state_set_game_mode(game_state_t *state, bool puzzle_mode);
void game_state_start_new_game(game_state_t *state);

// Selection management
void game_state_select_piece(game_state_t *state, uint8_t row, uint8_t col);
void game_state_deselect_piece(game_state_t *state);
bool game_state_execute_selected_move(game_state_t *state, uint8_t move_index);

// Menu management
void game_state_update_menu_enables(game_state_t *state);
void game_state_activate_menu_icon(game_state_t *state, menu_icon_t icon);

// Win detection
bool game_state_check_win_condition(game_state_t *state);

// Update
void game_state_update(game_state_t *state, float delta_time);

#endif // GAME_STATE_H
