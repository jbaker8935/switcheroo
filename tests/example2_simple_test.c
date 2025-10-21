#include <stdio.h>
#include <stdbool.h>
#include "../src/board.h"
#include "../src/ai_agent.h"

static void clear_board(board_t *board) {
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }
    board->history_count = 0;
    board->move_count = 0;
}

int main(void) {
    printf("=== EXAMPLE 2 TEST ===\n\n");
    
    board_t board;
    board_init(&board);
    clear_board(&board);
    
    board.current_player = PLAYER_WHITE;
    
    // Set up Example 2 position using board_set_piece
    board_set_piece(&board, 0, 3, PIECE_WHITE_NORMAL);     // D1
    board_set_piece(&board, 1, 1, PIECE_WHITE_NORMAL);     // B2
    board_set_piece(&board, 1, 2, PIECE_BLACK_NORMAL);     // C2
    board_set_piece(&board, 1, 3, PIECE_WHITE_NORMAL);     // D2
    board_set_piece(&board, 2, 2, PIECE_WHITE_SWAPPED);    // C3
    board_set_piece(&board, 3, 0, PIECE_BLACK_NORMAL);     // A4
    board_set_piece(&board, 3, 3, PIECE_WHITE_SWAPPED);    // D4
    board_set_piece(&board, 4, 0, PIECE_BLACK_NORMAL);     // A5
    board_set_piece(&board, 4, 2, PIECE_BLACK_SWAPPED);    // C5
    board_set_piece(&board, 4, 3, PIECE_BLACK_NORMAL);     // D5
    board_set_piece(&board, 5, 1, PIECE_BLACK_NORMAL);     // B6
    board_set_piece(&board, 5, 3, PIECE_WHITE_SWAPPED);    // D6
    board_set_piece(&board, 6, 0, PIECE_WHITE_NORMAL);     // A7
    board_set_piece(&board, 6, 3, PIECE_WHITE_SWAPPED);    // D7
    board_set_piece(&board, 7, 1, PIECE_BLACK_NORMAL);     // B8
    board_set_piece(&board, 7, 3, PIECE_BLACK_SWAPPED);    // D8
    
    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    
    // Increase search limits for this test
    config.search.node_limit = 500000;  // Much higher
    config.search.max_depth = 8;        // Deeper search
    config.search.use_transposition = false;  // Avoid stale TT entries
    config.use_hint_profile = true;
    
    printf("AI config: ai_player=%d (WHITE=%d, BLACK=%d)\n", 
           config.ai_player, PLAYER_WHITE, PLAYER_BLACK);
    printf("Testing with EXPERT mode (base_depth=%d, max_depth=%d, node_limit=%d)\n", 
           config.search.base_depth, config.search.max_depth, config.search.node_limit);
    
    move_t best_move;
    bool found = ai_agent_find_best_move(&board, &config, &best_move);
    
    if (found) {
        printf("\nAI selected: row %d, col %d -> row %d, col %d (%s)\n",
               best_move.from_row, best_move.from_col,
               best_move.to_row, best_move.to_col,
               best_move.type == MOVE_TYPE_SWAP ? "swap" : "empty");
               
        // Convert to chess notation
        char from_col = 'A' + best_move.from_col;
        int from_row = best_move.from_row + 1;
        char to_col = 'A' + best_move.to_col;
        int to_row = best_move.to_row + 1;
        
        printf("Chess notation: %c%d->%c%d\n", from_col, from_row, to_col, to_row);
        
        // Check if it's the expected winning move A7->B6
        if (best_move.from_row == 6 && best_move.from_col == 0 &&
            best_move.to_row == 5 && best_move.to_col == 1) {
            printf("\n✓ SUCCESS: AI found the winning move A7->B6!\n");
            return 0;
        } else {
            printf("\n✗ FAIL: Expected A7->B6 (row 6, col 0 -> row 5, col 1)\n");
            return 1;
        }
    } else {
        printf("\n✗ ERROR: No move found!\n");
        return 1;
    }
}
