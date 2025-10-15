#include "../src/platform_f256.h"
#include "../src/game_state.h"


/*
 * @file text_display.h
 * @brief Text display utilities for F256 Switcharoo
 * Text Layout: 25 characters wide
 * Row: 0 Win / Loss Information
 * Row: 1 Player To Move OR Winning Player
 * Row: 2 Game Mode: Puzzle or Free Play
 * Row: 3 Swap Rule
 * Row: 4 AI Difficulty
 * Row: 5 Puzzle Number M of N (if in puzzle mode)
 * Row: 6 Puzzle Difficulty (if in puzzle mode) 
 * Row: 7 Puzzle Solve Status (if in puzzle mode)
 * Row: 8 Puzzle Hint (if in puzzle mode)
 * Row: 9 "Move History"
 * Row: 10-17 Move History (8 moves, 1 per row)
 * 
 */

void print_formatted_text(uint8_t x, uint8_t y, const char *text);
void print_win_loss(uint16_t win_count, uint16_t loss_count);
void print_current_player(player_t player);
void print_game_winner(player_t winner);
void print_game_mode(bool is_puzzle_mode);
void print_swap_rule(swap_rule_t rule);
void print_puzzle_info(uint16_t puzzle_index, uint16_t total_puzzles,
                      uint8_t puzzle_difficulty, bool is_solved);
void print_ai_difficulty(ai_difficulty_t difficulty);
void print_puzzle_hint(const char *hint);
void clear_puzzle_hint(void);
void print_move_history(const move_t *history, uint8_t move_count);
void clear_puzzle_info(void);
void print_puzzle_debug(const char *line1, const char *line2);
void clear_puzzle_debug(void);
void print_mouse_position(uint16_t x, uint16_t y);
uint8_t format_move_string(char *buf, size_t buf_size, const move_t *move);

