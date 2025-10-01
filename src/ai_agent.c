/**
 * @file ai_agent.c
 * @brief AI agent implementation for F256 Switcharoo
 */

#include "../src/ai_agent.h"
#include <string.h>
#include <stdlib.h>

// Helper function to make a copy of the board
static void board_copy(board_t *dest, const board_t *src) {
    memcpy(dest, src, sizeof(board_t));
}

// Simulate a move on a board copy
static void board_simulate_move(board_t *board, const move_t *move) {
    piece_type_t from_piece = board_get_piece(board, move->from_row, move->from_col);
    piece_type_t to_piece = board_get_piece(board, move->to_row, move->to_col);
    
    if (move->type == MOVE_TYPE_EMPTY) {
        // Move to empty cell
        board_set_piece(board, move->to_row, move->to_col, from_piece);
        board_set_piece(board, move->from_row, move->from_col, PIECE_NONE);
        
        // Clear swapped pieces based on swap rule (for now, Classic rule)
        board_clear_all_swapped(board);
        
    } else if (move->type == MOVE_TYPE_SWAP) {
        // Swap pieces and mark both as swapped
        player_t from_owner = board_get_piece_owner(from_piece);
        player_t to_owner = board_get_piece_owner(to_piece);
        
        piece_type_t from_swapped = (from_owner == PLAYER_WHITE) ? 
            PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
        piece_type_t to_swapped = (to_owner == PLAYER_WHITE) ?
            PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
        
        board_set_piece(board, move->to_row, move->to_col, from_swapped);
        board_set_piece(board, move->from_row, move->from_col, to_swapped);
    }
}

// Count rows occupied by player in win zone (rows 2-7)
static uint8_t count_occupied_win_rows(const board_t *board, player_t player) {
    uint8_t occupied_rows = 0;
    
    for (uint8_t row = WIN_START_ROW; row <= WIN_END_ROW; row++) {
        bool has_piece = false;
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) == player) {
                has_piece = true;
                break;
            }
        }
        if (has_piece) {
            occupied_rows++;
        }
    }
    
    return occupied_rows;
}

// Count connected row pairs in win zone
static uint8_t count_connected_win_rows(const board_t *board, player_t player) {
    uint8_t connected = 0;
    
    for (uint8_t row = WIN_START_ROW; row < WIN_END_ROW; row++) {
        // Check if row and row+1 have connected pieces
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) != player) continue;
            
            // Check all 3 directions that connect to next row (SW, S, SE)
            for (int8_t dc = -1; dc <= 1; dc++) {
                int8_t next_col = (int8_t)col + dc;
                if (next_col >= 0 && next_col < BOARD_COLS) {
                    piece_type_t next_piece = board_get_piece(board, row + 1, (uint8_t)next_col);
                    if (board_get_piece_owner(next_piece) == player) {
                        connected++;
                        goto next_row;  // Count each row pair only once
                    }
                }
            }
        }
        next_row:;
    }
    
    return connected;
}

// Count swapped pieces for player
static uint8_t count_swapped_pieces(const board_t *board, player_t player) {
    uint8_t count = 0;
    piece_type_t swapped_piece = (player == PLAYER_WHITE) ? 
        PIECE_WHITE_SWAPPED : PIECE_BLACK_SWAPPED;
    
    for (uint8_t row = 0; row < BOARD_ROWS; row++) {
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            if (board_get_piece(board, row, col) == swapped_piece) {
                count++;
            }
        }
    }
    
    return count;
}

// Count pieces on back row (row 0 for black, row 7 for white)
static uint8_t count_back_row_pieces(const board_t *board, player_t player) {
    uint8_t count = 0;
    uint8_t back_row = (player == PLAYER_BLACK) ? 0 : 7;
    
    for (uint8_t col = 0; col < BOARD_COLS; col++) {
        piece_type_t piece = board_get_piece(board, back_row, col);
        if (board_get_piece_owner(piece) == player) {
            count++;
        }
    }
    
    return count;
}

void ai_agent_init(ai_config_t *config, swap_rule_t swap_rule,
                   ai_difficulty_t difficulty, player_t ai_player) {
    config->swap_rule = swap_rule;
    config->difficulty = difficulty;
    config->ai_player = ai_player;
}

int16_t ai_agent_evaluate_board(const board_t *board, player_t player,
                                const ai_config_t *config) {
    (void)config;  // For future use with swap rules
    
    // Check for immediate win
    win_path_t win_path;
    if (board_check_win(board, player, &win_path)) {
        return 10000;  // Winning position
    }
    
    // Check for opponent win
    player_t opponent = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    if (board_check_win(board, opponent, &win_path)) {
        return -10000;  // Losing position
    }
    
    int16_t score = 0;
    
    // Positive factors
    score += count_occupied_win_rows(board, player) * 20;        // Presence in win zone
    score += count_connected_win_rows(board, player) * 50;       // Connectivity
    score += count_swapped_pieces(board, player) * 5;            // Swapped pieces limit opponent
    
    // Negative factors
    score -= count_back_row_pieces(board, player) * 10;          // Pieces on back row are bad
    score -= count_connected_win_rows(board, opponent) * 45;     // Opponent connectivity
    score -= count_occupied_win_rows(board, opponent) * 15;      // Opponent presence
    
    return score;
}

// Check if a move leads to immediate opponent win
static bool move_allows_opponent_win(const board_t *board, const move_t *move,
                                     player_t opponent) {
    board_t temp_board;
    board_copy(&temp_board, board);
    board_simulate_move(&temp_board, move);
    temp_board.current_player = opponent;
    
    // Check all opponent moves
    move_t opponent_moves[64];  // Max possible moves
    uint8_t move_count = 0;
    
    for (uint8_t row = 0; row < BOARD_ROWS; row++) {
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            piece_type_t piece = board_get_piece(&temp_board, row, col);
            if (board_get_piece_owner(piece) == opponent) {
                uint8_t legal_count = board_get_legal_moves(&temp_board, row, col,
                                                            &opponent_moves[move_count],
                                                            64 - move_count);
                move_count += legal_count;
            }
        }
    }
    
    // Check if any opponent move wins
    for (uint8_t i = 0; i < move_count; i++) {
        board_t test_board;
        board_copy(&test_board, &temp_board);
        board_simulate_move(&test_board, &opponent_moves[i]);
        
        if (board_check_win(&test_board, opponent, NULL)) {
            return true;
        }
    }
    
    return false;
}

bool ai_agent_find_best_move(const board_t *board, const ai_config_t *config,
                             move_t *out_move) {
    player_t ai_player = config->ai_player;
    
    // Collect all legal moves for AI player
    move_t all_moves[64];  // Max possible moves
    uint8_t move_count = 0;
    
    for (uint8_t row = 0; row < BOARD_ROWS; row++) {
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            piece_type_t piece = board_get_piece(board, row, col);
            if (board_get_piece_owner(piece) == ai_player) {
                uint8_t legal_count = board_get_legal_moves(board, row, col,
                                                            &all_moves[move_count],
                                                            64 - move_count);
                
                // Debug: Check each legal move for invalid swaps
                for (uint8_t i = 0; i < legal_count; i++) {
                    move_t *m = &all_moves[move_count + i];
                    if (m->type == MOVE_TYPE_SWAP) {
                        piece_type_t target = board_get_piece(board, m->to_row, m->to_col);
                        if (board_is_piece_swapped(target)) {
                            // This should NEVER happen!
                            extern void textGotoXY(uint8_t x, uint8_t y);
                            extern int printf(const char *format, ...);
                            textGotoXY(0, 8);
                            printf("BUG: Swap w/swapped!");
                        }
                    }
                }
                
                move_count += legal_count;
            }
        }
    }
    
    if (move_count == 0) {
        return false;  // No legal moves
    }
    
    // Learning difficulty - just pick a random move
    if (config->difficulty == AI_DIFFICULTY_LEARNING) {
        uint8_t random_index = (uint8_t)(board->move_count % move_count);
        *out_move = all_moves[random_index];
        return true;
    }
    
    // Evaluate each move
    int16_t best_score = -30000;
    uint8_t best_move_index = 0;
    player_t opponent = (ai_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    for (uint8_t i = 0; i < move_count; i++) {
        board_t temp_board;
        board_copy(&temp_board, board);
        board_simulate_move(&temp_board, &all_moves[i]);
        
        // Check if this move wins immediately
        if (board_check_win(&temp_board, ai_player, NULL)) {
            *out_move = all_moves[i];
            return true;  // Always take winning move
        }
        
        // Evaluate the resulting position
        int16_t score = ai_agent_evaluate_board(&temp_board, ai_player, config);
        
        // Penalty for moves that allow opponent to win (loss avoidance)
        if (config->difficulty >= AI_DIFFICULTY_STANDARD) {
            if (move_allows_opponent_win(board, &all_moves[i], opponent)) {
                score -= 5000;  // Heavy penalty
            }
        }
        
        if (score > best_score) {
            best_score = score;
            best_move_index = i;
        }
    }
    
    *out_move = all_moves[best_move_index];
    return true;
}
