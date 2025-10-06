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

// Simulate a move on a board copy, respecting the provided swap_rule
static void board_simulate_move(board_t *board, const move_t *move, swap_rule_t swap_rule) {
    piece_type_t from_piece = board_get_piece(board, move->from_row, move->from_col);
    piece_type_t to_piece = board_get_piece(board, move->to_row, move->to_col);

    if (move->type == MOVE_TYPE_EMPTY) {
        // Move to empty cell
        board_set_piece(board, move->to_row, move->to_col, from_piece);
        board_set_piece(board, move->from_row, move->from_col, PIECE_NONE);

        // Clear swapped pieces based on provided swap rule
        switch (swap_rule) {
            case SWAP_RULE_CLASSIC:
                board_clear_all_swapped(board);
                break;
            case SWAP_RULE_CLEARS_OWN: {
                player_t mover = board_get_piece_owner(from_piece);
                if (mover == PLAYER_WHITE) {
                    for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                        for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                            if (board_get_piece(board, r, c) == PIECE_WHITE_SWAPPED) {
                                board_set_piece(board, r, c, PIECE_WHITE_NORMAL);
                            }
                        }
                    }
                } else if (mover == PLAYER_BLACK) {
                    for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                        for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                            if (board_get_piece(board, r, c) == PIECE_BLACK_SWAPPED) {
                                board_set_piece(board, r, c, PIECE_BLACK_NORMAL);
                            }
                        }
                    }
                }
            } break;
            case SWAP_RULE_SWAPPED_CLEARS:
                if (board_is_piece_swapped(from_piece)) {
                    board_clear_all_swapped(board);
                }
                break;
            case SWAP_RULE_SWAPPED_CLEARS_OWN: {
                if (board_is_piece_swapped(from_piece)) {
                    player_t mover = board_get_piece_owner(from_piece);
                    if (mover == PLAYER_WHITE) {
                        for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                            for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                if (board_get_piece(board, r, c) == PIECE_WHITE_SWAPPED) {
                                    board_set_piece(board, r, c, PIECE_WHITE_NORMAL);
                                }
                            }
                        }
                    } else if (mover == PLAYER_BLACK) {
                        for (uint8_t r = 0; r < BOARD_ROWS; ++r) {
                            for (uint8_t c = 0; c < BOARD_COLS; ++c) {
                                if (board_get_piece(board, r, c) == PIECE_BLACK_SWAPPED) {
                                    board_set_piece(board, r, c, PIECE_BLACK_NORMAL);
                                }
                            }
                        }
                    }
                }
            } break;
            default:
                board_clear_all_swapped(board);
                break;
        }

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

// Count blocked back-row pieces (pieces on back row with no forward mobility)
static uint8_t count_blocked_back_row_pieces(const board_t *board, player_t player) {
    uint8_t count = 0;
    uint8_t back_row = (player == PLAYER_BLACK) ? 0 : 7;
    int8_t forward_dir = (player == PLAYER_BLACK) ? 1 : -1;
    uint8_t second_row = (uint8_t)((int8_t)back_row + forward_dir);
    
    for (uint8_t col = 0; col < BOARD_COLS; col++) {
        piece_type_t piece = board_get_piece(board, back_row, col);
        if (board_get_piece_owner(piece) != player) continue;
        
        // Check if all forward-adjacent cells are blocked
        bool can_move_forward = false;
        for (int8_t dc = -1; dc <= 1; dc++) {
            int8_t next_col = (int8_t)col + dc;
            if (next_col >= 0 && next_col < BOARD_COLS) {
                piece_type_t next_piece = board_get_piece(board, second_row, (uint8_t)next_col);
                if (next_piece == PIECE_NONE || board_get_piece_owner(next_piece) != player) {
                    can_move_forward = true;
                    break;
                }
            }
        }
        
        if (!can_move_forward) {
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
    score -= count_blocked_back_row_pieces(board, player) * 15;  // Blocked back-row pieces are worse
    score -= count_connected_win_rows(board, opponent) * 45;     // Opponent connectivity
    score -= count_occupied_win_rows(board, opponent) * 15;      // Opponent presence
    
    return score;
}

// Check if a move leads to immediate opponent win
static bool move_allows_opponent_win(const board_t *board, const move_t *move,
                                     player_t opponent, swap_rule_t swap_rule) {
    board_t temp_board;
    board_copy(&temp_board, board);
    board_simulate_move(&temp_board, move, swap_rule);
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
        board_simulate_move(&test_board, &opponent_moves[i], swap_rule);
        
        if (board_check_win(&test_board, opponent, NULL)) {
            return true;
        }
    }
    
    return false;
}

// Check if opponent has a forcing move (guarantees win regardless of AI response)
// This is a 2-ply lookahead: opponent move -> all AI responses -> check if all lead to opponent win
static bool move_allows_forcing_opponent_move(const board_t *board, const move_t *move,
                                               player_t ai_player, player_t opponent,
                                               swap_rule_t swap_rule) {
    board_t temp_board;
    board_copy(&temp_board, board);
    board_simulate_move(&temp_board, move, swap_rule);
    temp_board.current_player = opponent;
    
    // Collect all opponent moves
    move_t opponent_moves[64];
    uint8_t opp_move_count = 0;
    
    for (uint8_t row = 0; row < BOARD_ROWS; row++) {
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            piece_type_t piece = board_get_piece(&temp_board, row, col);
            if (board_get_piece_owner(piece) == opponent) {
                uint8_t legal_count = board_get_legal_moves(&temp_board, row, col,
                                                            &opponent_moves[opp_move_count],
                                                            64 - opp_move_count);
                opp_move_count += legal_count;
            }
        }
    }
    
    // Check if opponent has a forcing move (one that wins no matter AI's response)
    for (uint8_t i = 0; i < opp_move_count; i++) {
        board_t opp_board;
        board_copy(&opp_board, &temp_board);
        board_simulate_move(&opp_board, &opponent_moves[i], swap_rule);
        
        // Check if opponent wins immediately
        if (board_check_win(&opp_board, opponent, NULL)) {
            return true;  // Forcing move found
        }
        
        // Check if opponent position is so strong that all AI responses lose
        opp_board.current_player = ai_player;
        
        // Collect all AI responses
        move_t ai_responses[64];
        uint8_t ai_response_count = 0;
        
        for (uint8_t row = 0; row < BOARD_ROWS; row++) {
            for (uint8_t col = 0; col < BOARD_COLS; col++) {
                piece_type_t piece = board_get_piece(&opp_board, row, col);
                if (board_get_piece_owner(piece) == ai_player) {
                    uint8_t legal_count = board_get_legal_moves(&opp_board, row, col,
                                                                &ai_responses[ai_response_count],
                                                                64 - ai_response_count);
                    ai_response_count += legal_count;
                }
            }
        }
        
        // Check if all AI responses allow opponent to win on next move
        bool all_responses_lose = true;
        for (uint8_t j = 0; j < ai_response_count; j++) {
            board_t response_board;
            board_copy(&response_board, &opp_board);
            board_simulate_move(&response_board, &ai_responses[j], swap_rule);
            
            // After AI response, can opponent win?
            if (!move_allows_opponent_win(&response_board, &ai_responses[j], opponent, swap_rule)) {
                all_responses_lose = false;
                break;
            }
        }
        
        if (all_responses_lose && ai_response_count > 0) {
            return true;  // This opponent move is forcing
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
    uint8_t back_row = (ai_player == PLAYER_BLACK) ? 0 : 7;
    
    for (uint8_t i = 0; i < move_count; i++) {
        board_t temp_board;
        board_copy(&temp_board, board);
        board_simulate_move(&temp_board, &all_moves[i], config->swap_rule);
        
        // Check if this move wins immediately
        if (board_check_win(&temp_board, ai_player, NULL)) {
            *out_move = all_moves[i];
            return true;  // Always take winning move
        }
        
        // Evaluate the resulting position
        int16_t score = ai_agent_evaluate_board(&temp_board, ai_player, config);
        
        // Penalty for sideways back-rank moves (doesn't advance pieces forward)
        if (all_moves[i].from_row == back_row && all_moves[i].to_row == back_row) {
            score -= 25;  // Discourage lateral back-rank movement
        }
        
        // Bonus for moves that free up back-rank pieces (move from second row forward)
        uint8_t second_row = (ai_player == PLAYER_BLACK) ? 1 : 6;
        if (all_moves[i].from_row == second_row) {
            // Check if this clears a path for a back-rank piece
            int8_t forward_dir = (ai_player == PLAYER_BLACK) ? -1 : 1;
            uint8_t check_row = (uint8_t)((int8_t)all_moves[i].from_row + forward_dir);
            for (int8_t dc = -1; dc <= 1; dc++) {
                int8_t check_col = (int8_t)all_moves[i].from_col + dc;
                if (check_col >= 0 && check_col < BOARD_COLS) {
                    piece_type_t check_piece = board_get_piece(board, check_row, (uint8_t)check_col);
                    if (board_get_piece_owner(check_piece) == ai_player) {
                        score += 20;  // Bonus for unblocking back-rank pieces
                        break;
                    }
                }
            }
        }
        
        // Standard difficulty: avoid moves that allow opponent to win next turn
        if (config->difficulty >= AI_DIFFICULTY_STANDARD) {
            if (move_allows_opponent_win(board, &all_moves[i], opponent, config->swap_rule)) {
                score -= 5000;  // Heavy penalty
            }
        }
        
        // Expert difficulty: avoid moves that allow opponent forcing moves (2-ply lookahead)
        if (config->difficulty >= AI_DIFFICULTY_EXPERT) {
            if (move_allows_forcing_opponent_move(board, &all_moves[i], ai_player, opponent, config->swap_rule)) {
                score -= 8000;  // Severe penalty for allowing forcing moves
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
