#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "../src/board.h"

static void setup_scenario4(board_t *board) {
    // Clear board
    for (uint8_t row = 0; row < BOARD_ROWS; ++row) {
        for (uint8_t col = 0; col < BOARD_COLS; ++col) {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }
    
    // B2:b,C2:w,A3:w,B3:b,A4:b,B4:b,C4:B,D4:b,A5:w,B5:w,C5:b,D5:w,A6:W,B6:b,C6:w,D6:W
    board_set_piece(board, 1, 1, PIECE_BLACK_NORMAL);  // B2:b
    board_set_piece(board, 1, 2, PIECE_WHITE_NORMAL);  // C2:w
    board_set_piece(board, 2, 0, PIECE_WHITE_NORMAL);  // A3:w
    board_set_piece(board, 2, 1, PIECE_BLACK_NORMAL);  // B3:b
    board_set_piece(board, 3, 0, PIECE_BLACK_NORMAL);  // A4:b
    board_set_piece(board, 3, 1, PIECE_BLACK_NORMAL);  // B4:b
    board_set_piece(board, 3, 2, PIECE_BLACK_SWAPPED); // C4:B
    board_set_piece(board, 3, 3, PIECE_BLACK_NORMAL);  // D4:b
    board_set_piece(board, 4, 0, PIECE_WHITE_NORMAL);  // A5:w
    board_set_piece(board, 4, 1, PIECE_WHITE_NORMAL);  // B5:w
    board_set_piece(board, 4, 2, PIECE_BLACK_NORMAL);  // C5:b
    board_set_piece(board, 4, 3, PIECE_WHITE_NORMAL);  // D5:w
    board_set_piece(board, 5, 0, PIECE_WHITE_SWAPPED); // A6:W
    board_set_piece(board, 5, 1, PIECE_BLACK_NORMAL);  // B6:b
    board_set_piece(board, 5, 2, PIECE_WHITE_NORMAL);  // C6:w
    board_set_piece(board, 5, 3, PIECE_WHITE_SWAPPED); // D6:W
    
    board->current_player = PLAYER_BLACK;
}

int main(void) {
    board_t board;
    board_init(&board);
    setup_scenario4(&board);
    
    const int ITERATIONS = 100000;
    clock_t start, end;
    double cpu_time_used;
    
    printf("=== Win Check Performance Test ===\n\n");
    printf("Testing scenario 4 position with %d iterations\n\n", ITERATIONS);
    
    // Test optimized board_check_win_fast
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        bool white_wins = board_check_win_fast(&board, PLAYER_WHITE);
        bool black_wins = board_check_win_fast(&board, PLAYER_BLACK);
        (void)white_wins;
        (void)black_wins;
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Original board_check_win:\n");
    printf("  Time: %.3f seconds\n", cpu_time_used);
    printf("  Per check: %.1f µs\n", (cpu_time_used * 1000000) / (ITERATIONS * 2));
    
    // Test with cache (board_check_win_incremental)
    board.win_cache.cache_valid = false;
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        bool white_wins = board_check_win_incremental(&board, PLAYER_WHITE, NULL);
        bool black_wins = board_check_win_incremental(&board, PLAYER_BLACK, NULL);
        (void)white_wins;
        (void)black_wins;
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("\nIncremental board_check_win_incremental:\n");
    printf("  Time: %.3f seconds\n", cpu_time_used);
    printf("  Per check: %.1f µs\n", (cpu_time_used * 1000000) / (ITERATIONS * 2));
    printf("  Note: Cache hit rate should be very high after first check\n");
    
    // Verify correctness
    printf("\n=== Correctness Check ===\n");
    bool white_orig = board_check_win(&board, PLAYER_WHITE, NULL);
    bool black_orig = board_check_win(&board, PLAYER_BLACK, NULL);
    board.win_cache.cache_valid = false;
    bool white_incr = board_check_win_incremental(&board, PLAYER_WHITE, NULL);
    bool black_incr = board_check_win_incremental(&board, PLAYER_BLACK, NULL);
    
    printf("White wins - Original: %d, Incremental: %d %s\n", 
           white_orig, white_incr, (white_orig == white_incr) ? "✓" : "✗");
    printf("Black wins - Original: %d, Incremental: %d %s\n", 
           black_orig, black_incr, (black_orig == black_incr) ? "✓" : "✗");
    
    // Test inline board_get_piece_owner optimization
    printf("\n=== board_get_piece_owner Test ===\n");
    start = clock();
    volatile player_t owner;
    for (int i = 0; i < ITERATIONS * 100; i++) {
        owner = board_get_piece_owner(PIECE_WHITE_NORMAL);
        owner = board_get_piece_owner(PIECE_WHITE_SWAPPED);
        owner = board_get_piece_owner(PIECE_BLACK_NORMAL);
        owner = board_get_piece_owner(PIECE_BLACK_SWAPPED);
        owner = board_get_piece_owner(PIECE_NONE);
    }
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Inline board_get_piece_owner (optimized with (piece-1)>>1):\n");
    printf("  Time: %.3f seconds for %d calls\n", cpu_time_used, ITERATIONS * 500);
    printf("  Per call: %.1f ns\n", (cpu_time_used * 1000000000) / (ITERATIONS * 500));
    printf("  Note: Should be much faster than switch/case\n");
    (void)owner;
    
    return 0;
}
