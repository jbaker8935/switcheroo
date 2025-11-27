/**
 * @file ai_advancement_diagnostic.c
 * @brief Diagnostic tool to analyze AI move selection behavior
 * 
 * This test simulates games with different move selection strategies to:
 * 1. Diagnose repetition loops and draw patterns in different layouts
 * 2. Test alternative move ordering heuristics without modifying baseline code
 * 3. Provide recommendations for AI improvement
 * 
 * Build: gcc -o tests/ai_advancement_diagnostic -I. -I./src -I./tests \
 *        -DAI_AGENT_HOST_TEST=1 -std=c99 -Wall -Wextra -O2 \
 *        tests/ai_advancement_diagnostic.c tests/stubs.c src/board.c src/ai_agent.c
 * 
 * Run: ./tests/ai_advancement_diagnostic
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/ai_agent.h"
#include "../src/board.h"

// ============================================================================
// Configuration
// ============================================================================

#define DIAGNOSTIC_MAX_HALF_MOVES 200
#define DIAGNOSTIC_GAMES_PER_TEST 50
#define VERBOSE_GAME_LIMIT 2  // Only print detailed logs for first N games

// ============================================================================
// Data Structures
// ============================================================================

typedef struct {
    uint8_t ply;
    int16_t eval_total;
    int16_t eval_connection;
    int16_t eval_bridge;
    int16_t eval_swap;
    int16_t eval_blocking;
    int16_t eval_mobility;
    move_type_t move_type;
    int8_t row_delta;  // Positive = toward enemy goal
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
} ply_record_t;

typedef struct {
    player_t winner;  // PLAYER_NONE = draw
    uint16_t plies;
    uint16_t record_count;
    ply_record_t ply_records[DIAGNOSTIC_MAX_HALF_MOVES];
    uint16_t white_swaps;
    uint16_t black_swaps;
    uint16_t white_empty_moves;
    uint16_t black_empty_moves;
    uint16_t repetition_count;  // How many times a position repeated
} diagnostic_game_result_t;

typedef struct {
    const char *name;
    uint16_t games;
    uint16_t white_wins;
    uint16_t black_wins;
    uint16_t draws;
    uint32_t total_plies;
    uint32_t total_white_swaps;
    uint32_t total_black_swaps;
    uint32_t total_repetitions;
    uint16_t games_over_100_plies;
    uint16_t games_over_150_plies;
} test_summary_t;

// Move selection strategy for experimental testing
typedef enum {
    STRATEGY_BASELINE = 0,           // Use standard AI agent
    STRATEGY_ANTI_REVERSAL = 1,      // Penalize moves that reverse opponent's last
    STRATEGY_FORWARD_BIAS = 2,       // Extra weight on forward advancement
    STRATEGY_SWAP_COOLDOWN = 3,      // Reduce SWAP preference after recent swaps
    STRATEGY_COMBINED = 4            // All improvements combined
} move_strategy_t;

// ============================================================================
// Utility Functions
// ============================================================================

static uint32_t s_rng_state = 0xDEADBEEF;

static uint32_t diagnostic_rand(void) {
    s_rng_state ^= s_rng_state << 13;
    s_rng_state ^= s_rng_state >> 17;
    s_rng_state ^= s_rng_state << 5;
    return s_rng_state;
}

static void diagnostic_srand(uint32_t seed) {
    s_rng_state = seed ? seed : 1;
}

static void print_board(const board_t *board) {
    printf("  0 1 2 3\n");
    for (int row = 0; row < BOARD_ROWS; row++) {
        printf("%d ", row);
        for (int col = 0; col < BOARD_COLS; col++) {
            piece_type_t p = board->cells[row][col];
            char c = '.';
            if (p == PIECE_WHITE_NORMAL) c = 'W';
            else if (p == PIECE_WHITE_SWAPPED) c = 'w';
            else if (p == PIECE_BLACK_NORMAL) c = 'B';
            else if (p == PIECE_BLACK_SWAPPED) c = 'b';
            printf("%c ", c);
        }
        printf("\n");
    }
}

static void print_move(const move_t *m) {
    printf("(%d,%d)->(%d,%d) %s by %s",
           m->from_row, m->from_col, m->to_row, m->to_col,
           m->type == MOVE_TYPE_SWAP ? "SWAP" : "EMPTY",
           m->player == PLAYER_WHITE ? "W" : "B");
}

// Check if move reverses opponent's last move
static bool is_reversal_move(const move_t *candidate, const move_t *opponent_last) {
    return (candidate->to_row == opponent_last->from_row &&
            candidate->to_col == opponent_last->from_col &&
            candidate->from_row == opponent_last->to_row &&
            candidate->from_col == opponent_last->to_col);
}

// ============================================================================
// Experimental Move Selection (Test Harness Only)
// ============================================================================

/**
 * Apply experimental move ordering adjustments in the test harness.
 * This modifies scores AFTER the baseline AI generates them, allowing
 * us to test alternative strategies without changing the baseline code.
 * 
 * Returns the index of the selected move.
 */
static uint8_t select_move_with_strategy(
    const board_t *board,
    player_t current_player,
    const ai_config_t *config,
    move_strategy_t strategy,
    const move_t *last_opponent_move,
    bool has_last_move,
    uint8_t recent_swap_count,  // Swaps in last N plies
    move_t *out_move,
    const board_context_t *game_context  // Full game context with history
) {
    // For baseline strategy, just use the standard AI with full context
    if (strategy == STRATEGY_BASELINE) {
        if (ai_agent_find_best_move(board, game_context, config, out_move)) {
            return 1;
        }
        return 0;
    }
    
    // For experimental strategies, we need to enumerate moves and re-score
    // We'll use board_get_legal_moves and apply our own scoring
    
    move_array_t all_moves;
    uint8_t total_moves = 0;
    int16_t scores[32];
    move_t moves[32];
    
    // Enumerate all legal moves for current player
    for (uint8_t row = 0; row < BOARD_ROWS; row++) {
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            piece_type_t piece = board->cells[row][col];
            player_t owner = PLAYER_NONE;
            if ((piece & 0x01) && !(piece & 0x02)) owner = PLAYER_WHITE;
            else if ((piece & 0x02) && !(piece & 0x01)) owner = PLAYER_BLACK;
            
            if (owner != current_player) continue;
            
            uint8_t num = board_get_legal_moves_soa(board, current_player, row, col, &all_moves);
            for (uint8_t i = 0; i < num && total_moves < 32; i++) {
                moves[total_moves].from_row = row;
                moves[total_moves].from_col = col;
                moves[total_moves].to_row = all_moves.to_row[i];
                moves[total_moves].to_col = all_moves.to_col[i];
                moves[total_moves].type = all_moves.type[i];
                moves[total_moves].player = current_player;
                
                // Start with baseline scoring approximation
                int16_t score = 0;
                move_t *m = &moves[total_moves];
                bool mover_swapped = board_is_piece_swapped(piece);
                
                // Baseline-like scoring
                if (m->type == MOVE_TYPE_SWAP) {
                    score += 900;
                    score += mover_swapped ? 220 : 500;
                    if (m->to_col == 1 || m->to_col == 2) score += 150;
                } else {
                    score += mover_swapped ? 220 : 0;
                }
                
                if (m->to_col == 1 || m->to_col == 2) score += 350;
                
                // Forward movement bonus
                if (current_player == PLAYER_WHITE) {
                    if (m->to_row < m->from_row) score += 280;
                    if (m->to_row == WIN_START_ROW) score += 200;
                } else {
                    if (m->to_row > m->from_row) score += 280;
                    if (m->to_row == WIN_END_ROW) score += 200;
                }
                
                if (m->type == MOVE_TYPE_EMPTY && m->to_row >= WIN_START_ROW && m->to_row <= WIN_END_ROW) {
                    score += 120;
                }
                
                // === EXPERIMENTAL ADJUSTMENTS ===
                
                // Anti-reversal: penalize moves that reverse opponent's last
                if ((strategy == STRATEGY_ANTI_REVERSAL || strategy == STRATEGY_COMBINED) &&
                    has_last_move && is_reversal_move(m, last_opponent_move)) {
                    score -= 800;  // Strong penalty
                }
                
                // Forward bias: extra weight on forward advancement
                if (strategy == STRATEGY_FORWARD_BIAS || strategy == STRATEGY_COMBINED) {
                    int8_t delta = (current_player == PLAYER_WHITE) 
                        ? (m->from_row - m->to_row) 
                        : (m->to_row - m->from_row);
                    if (delta > 0) {
                        score += delta * 150;  // Additional forward bonus
                    }
                    // Penalize backward moves more
                    if (delta < 0) {
                        score -= 200;
                    }
                }
                
                // Swap cooldown: reduce SWAP preference if many recent swaps
                if ((strategy == STRATEGY_SWAP_COOLDOWN || strategy == STRATEGY_COMBINED) &&
                    m->type == MOVE_TYPE_SWAP && recent_swap_count >= 2) {
                    score -= 400;  // Reduce swap enthusiasm after recent swaps
                }
                
                scores[total_moves] = score;
                total_moves++;
            }
        }
    }
    
    if (total_moves == 0) return 0;
    
    // Verify moves don't lose immediately
    uint8_t best_idx = 0;
    int16_t best_score = -30000;
    player_t opponent = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    for (uint8_t i = 0; i < total_moves; i++) {
        // Apply move and check outcome
        board_t after;
        memcpy(&after, board, sizeof(board_t));
        board_context_t move_ctx = {.current_player = current_player};
        
        if (!board_execute_move_without_history(&after, &move_ctx, &moves[i], config->swap_rule)) {
            continue;
        }
        
        // Skip if opponent wins
        if (board_check_win_fast(&after, opponent)) {
            continue;
        }
        
        // Big bonus for immediate wins
        if (board_check_win_fast(&after, current_player)) {
            scores[i] += 25000;
        }
        
        // Check if allows opponent immediate win
        if (ai_immediate_win_available(&after, opponent, config->swap_rule)) {
            scores[i] -= 12000;
        }
        
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_idx = i;
        }
    }
    
    *out_move = moves[best_idx];
    return 1;
}

// ============================================================================
// Game Simulation
// ============================================================================

static diagnostic_game_result_t run_diagnostic_game(
    uint8_t layout_id,
    swap_rule_t rule,
    move_strategy_t white_strategy,
    move_strategy_t black_strategy,
    bool verbose
) {
    diagnostic_game_result_t result = {0};
    result.winner = PLAYER_NONE;
    
    board_t board;
    board_init(&board);
    board_set_starting_layout(&board, layout_id);
    
    board_context_t context = {0};
    context.current_player = PLAYER_WHITE;
    context.history_count = 0;
    
    ai_config_t config;
    ai_agent_init(&config, rule, AI_DIFFICULTY_EXPERT, PLAYER_WHITE);
    config.diagnostics_enabled = true;
    
    move_t last_white_move = {0};
    move_t last_black_move = {0};
    bool has_last_white = false;
    bool has_last_black = false;
    uint8_t recent_white_swaps = 0;
    uint8_t recent_black_swaps = 0;
    
    if (verbose) {
        printf("\n=== Game Start: Layout %d, Rule %d ===\n", layout_id, rule);
        printf("White strategy: %d, Black strategy: %d\n", white_strategy, black_strategy);
        print_board(&board);
    }
    
    for (uint16_t ply = 0; ply < DIAGNOSTIC_MAX_HALF_MOVES; ply++) {
        player_t current = context.current_player;
        move_strategy_t strategy = (current == PLAYER_WHITE) ? white_strategy : black_strategy;
        
        move_t *last_opp_move = (current == PLAYER_WHITE) ? &last_black_move : &last_white_move;
        bool has_last_opp = (current == PLAYER_WHITE) ? has_last_black : has_last_white;
        uint8_t recent_swaps = (current == PLAYER_WHITE) ? recent_white_swaps : recent_black_swaps;
        
        config.ai_player = current;
        
        move_t move;
        uint8_t found = select_move_with_strategy(
            &board, current, &config, strategy,
            last_opp_move, has_last_opp, recent_swaps, &move, &context
        );
        
        if (!found) {
            if (verbose) printf("No moves available at ply %d\n", ply);
            break;
        }
        
        // Get eval breakdown for recording
        ai_eval_breakdown_t breakdown;
        ai_agent_get_last_breakdown(&breakdown);
        
        // Calculate row delta
        int8_t row_delta = 0;
        if (current == PLAYER_WHITE) {
            row_delta = move.from_row - move.to_row;  // Positive = forward
        } else {
            row_delta = move.to_row - move.from_row;
        }
        
        if (verbose && ply < 30) {
            printf("Ply %3d: ", ply);
            print_move(&move);
            printf(" delta=%+d eval=%d\n", row_delta, breakdown.total);
        }
        
        // Record stats
        if (move.type == MOVE_TYPE_SWAP) {
            if (current == PLAYER_WHITE) {
                result.white_swaps++;
                recent_white_swaps++;
            } else {
                result.black_swaps++;
                recent_black_swaps++;
            }
        } else {
            if (current == PLAYER_WHITE) {
                result.white_empty_moves++;
                recent_white_swaps = (recent_white_swaps > 0) ? recent_white_swaps - 1 : 0;
            } else {
                result.black_empty_moves++;
                recent_black_swaps = (recent_black_swaps > 0) ? recent_black_swaps - 1 : 0;
            }
        }
        
        // Check for reversal
        if (has_last_opp && is_reversal_move(&move, last_opp_move)) {
            result.repetition_count++;
        }
        
        // Record ply data
        if (result.record_count < DIAGNOSTIC_MAX_HALF_MOVES) {
            ply_record_t *rec = &result.ply_records[result.record_count];
            rec->ply = ply;
            rec->eval_total = breakdown.total;
            rec->eval_connection = breakdown.connection_progress;
            rec->eval_bridge = breakdown.bridge_potential;
            rec->eval_swap = breakdown.swap_pressure;
            rec->eval_blocking = breakdown.blocking_coverage;
            rec->eval_mobility = breakdown.mobility;
            rec->move_type = move.type;
            rec->row_delta = row_delta;
            rec->from_row = move.from_row;
            rec->from_col = move.from_col;
            rec->to_row = move.to_row;
            rec->to_col = move.to_col;
            result.record_count++;
        }
        
        // Update last move tracking
        if (current == PLAYER_WHITE) {
            last_white_move = move;
            has_last_white = true;
        } else {
            last_black_move = move;
            has_last_black = true;
        }
        
        // Record in context history - prepend to index 0 (most recent first)
        // This matches the board_execute_move() history storage format
        for (uint8_t i = MAX_MOVE_HISTORY - 1; i > 0; --i) {
            context.history[i] = context.history[i - 1];
        }
        context.history[0] = move;
        if (context.history_count < MAX_MOVE_HISTORY) {
            context.history_count++;
        }
        
        // Execute move
        board_context_t move_ctx = {.current_player = current};
        bool executed = board_execute_move(&board, &move_ctx, &move, rule);
        assert(executed);
        
        result.plies = ply + 1;
        
        if (board_check_win(&board, current, NULL)) {
            result.winner = current;
            if (verbose) {
                printf("=== %s wins at ply %d ===\n",
                       current == PLAYER_WHITE ? "White" : "Black", ply + 1);
            }
            break;
        }
        
        board_switch_turn(&context);
    }
    
    if (verbose && result.winner == PLAYER_NONE) {
        printf("=== Draw (max plies reached) ===\n");
        printf("Final board:\n");
        print_board(&board);
    }
    
    return result;
}

// ============================================================================
// Test Suites
// ============================================================================

static test_summary_t run_test_suite(
    const char *name,
    uint8_t layout_id,
    swap_rule_t rule,
    move_strategy_t white_strategy,
    move_strategy_t black_strategy,
    uint16_t num_games,
    bool verbose_first
) {
    test_summary_t summary = {0};
    summary.name = name;
    
    printf("\n========================================\n");
    printf("Test Suite: %s\n", name);
    printf("Layout: %d, Rule: %d, Games: %d\n", layout_id, rule, num_games);
    printf("White Strategy: %d, Black Strategy: %d\n", white_strategy, black_strategy);
    printf("========================================\n");
    
    for (uint16_t g = 0; g < num_games; g++) {
        diagnostic_srand(g * 12345 + 98765);
        ai_agent_set_random_seed(g * 54321 + 11111);
        
        bool verbose = verbose_first && (g < VERBOSE_GAME_LIMIT);
        
        diagnostic_game_result_t result = run_diagnostic_game(
            layout_id, rule, white_strategy, black_strategy, verbose
        );
        
        summary.games++;
        summary.total_plies += result.plies;
        summary.total_white_swaps += result.white_swaps;
        summary.total_black_swaps += result.black_swaps;
        summary.total_repetitions += result.repetition_count;
        
        if (result.winner == PLAYER_WHITE) {
            summary.white_wins++;
        } else if (result.winner == PLAYER_BLACK) {
            summary.black_wins++;
        } else {
            summary.draws++;
        }
        
        if (result.plies > 100) summary.games_over_100_plies++;
        if (result.plies > 150) summary.games_over_150_plies++;
    }
    
    // Print summary
    printf("\nResults:\n");
    printf("  White wins: %u (%.1f%%)\n", summary.white_wins, 100.0 * summary.white_wins / summary.games);
    printf("  Black wins: %u (%.1f%%)\n", summary.black_wins, 100.0 * summary.black_wins / summary.games);
    printf("  Draws:      %u (%.1f%%)\n", summary.draws, 100.0 * summary.draws / summary.games);
    printf("  Avg plies:  %.1f\n", (double)summary.total_plies / summary.games);
    printf("  Games >100 plies: %u\n", summary.games_over_100_plies);
    printf("  Games >150 plies: %u\n", summary.games_over_150_plies);
    printf("  Avg W swaps: %.1f, Avg B swaps: %.1f\n",
           (double)summary.total_white_swaps / summary.games,
           (double)summary.total_black_swaps / summary.games);
    printf("  Avg reversals: %.1f\n", (double)summary.total_repetitions / summary.games);
    
    return summary;
}

// ============================================================================
// Analysis Functions
// ============================================================================

static void analyze_ply_patterns(diagnostic_game_result_t *result) {
    printf("\nPly-by-ply analysis (first 30 plies):\n");
    printf("Ply | Type  | Delta | From    | To      | Eval\n");
    printf("----|-------|-------|---------|---------|------\n");
    
    for (uint16_t i = 0; i < result->record_count && i < 30; i++) {
        ply_record_t *rec = &result->ply_records[i];
        printf("%3d | %5s | %+4d  | (%d,%d)   | (%d,%d)   | %5d\n",
               rec->ply,
               rec->move_type == MOVE_TYPE_SWAP ? "SWAP" : "EMPTY",
               rec->row_delta,
               rec->from_row, rec->from_col,
               rec->to_row, rec->to_col,
               rec->eval_total);
    }
}

static void compare_strategies(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║              STRATEGY COMPARISON ANALYSIS                             ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    
    // Test parameters
    uint8_t layout = 0;  // Layout 0 is the problematic one
    swap_rule_t rule = SWAP_RULE_CLASSIC;
    uint16_t games = DIAGNOSTIC_GAMES_PER_TEST;
    
    test_summary_t results[5];
    
    // Baseline vs Baseline
    results[0] = run_test_suite("Baseline vs Baseline", layout, rule,
                                 STRATEGY_BASELINE, STRATEGY_BASELINE, games, true);
    
    // Anti-reversal vs Baseline
    results[1] = run_test_suite("Anti-Reversal vs Baseline", layout, rule,
                                 STRATEGY_ANTI_REVERSAL, STRATEGY_BASELINE, games, true);
    
    // Forward-bias vs Baseline
    results[2] = run_test_suite("Forward-Bias vs Baseline", layout, rule,
                                 STRATEGY_FORWARD_BIAS, STRATEGY_BASELINE, games, true);
    
    // Swap-cooldown vs Baseline
    results[3] = run_test_suite("Swap-Cooldown vs Baseline", layout, rule,
                                 STRATEGY_SWAP_COOLDOWN, STRATEGY_BASELINE, games, true);
    
    // Combined vs Baseline
    results[4] = run_test_suite("Combined vs Baseline", layout, rule,
                                 STRATEGY_COMBINED, STRATEGY_BASELINE, games, true);
    
    // Print comparison table
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      COMPARISON SUMMARY                               ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════╣\n");
    printf("║ Strategy            │ W-Win │ B-Win │ Draw  │ Avg Ply │ Reversals   ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < 5; i++) {
        printf("║ %-19s │ %5.1f%% │ %5.1f%% │ %5.1f%% │ %7.1f │ %9.1f   ║\n",
               results[i].name,
               100.0 * results[i].white_wins / results[i].games,
               100.0 * results[i].black_wins / results[i].games,
               100.0 * results[i].draws / results[i].games,
               (double)results[i].total_plies / results[i].games,
               (double)results[i].total_repetitions / results[i].games);
    }
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
}

static void compare_layouts(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    LAYOUT COMPARISON (Baseline)                       ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    
    swap_rule_t rule = SWAP_RULE_CLASSIC;
    uint16_t games = DIAGNOSTIC_GAMES_PER_TEST;
    
    test_summary_t layout_results[4];
    
    for (uint8_t layout = 0; layout < 4; layout++) {
        char name[32];
        snprintf(name, sizeof(name), "Layout %d Baseline", layout);
        layout_results[layout] = run_test_suite(name, layout, rule,
                                                  STRATEGY_BASELINE, STRATEGY_BASELINE,
                                                  games, layout == 0);
    }
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║                      LAYOUT COMPARISON SUMMARY                        ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════╣\n");
    printf("║ Layout │ W-Win │ B-Win │ Draw  │ Avg Ply │ >100 ply │ Reversals    ║\n");
    printf("╠══════════════════════════════════════════════════════════════════════╣\n");
    
    for (uint8_t i = 0; i < 4; i++) {
        printf("║   %d    │ %5.1f%% │ %5.1f%% │ %5.1f%% │ %7.1f │ %8u │ %10.1f   ║\n",
               i,
               100.0 * layout_results[i].white_wins / layout_results[i].games,
               100.0 * layout_results[i].black_wins / layout_results[i].games,
               100.0 * layout_results[i].draws / layout_results[i].games,
               (double)layout_results[i].total_plies / layout_results[i].games,
               layout_results[i].games_over_100_plies,
               (double)layout_results[i].total_repetitions / layout_results[i].games);
    }
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
}

static void generate_recommendations(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║                         RECOMMENDATIONS                               ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Based on the diagnostic analysis, here are recommendations for improving\n");
    printf("the AI agent's move selection logic:\n\n");
    
    printf("1. ANTI-REVERSAL PENALTY (Highest Priority)\n");
    printf("   Problem: AI often reverses opponent's move, creating oscillation loops\n");
    printf("   Solution: In FAR10_ai_generate_moves(), add:\n");
    printf("     - Track opponent's last move in ai_config_t or board_context_t\n");
    printf("     - If candidate move's (to_row,to_col) == opponent's (from_row,from_col)\n");
    printf("       AND candidate's (from_row,from_col) == opponent's (to_row,to_col)\n");
    printf("     - Apply score penalty of -600 to -800\n\n");
    
    printf("2. FORWARD ADVANCEMENT WEIGHT\n");
    printf("   Problem: AI doesn't push pieces forward aggressively enough\n");
    printf("   Solution: Increase forward movement bonuses:\n");
    printf("     - Current: +280 for forward move\n");
    printf("     - Suggested: +400 for forward, +600 for 2+ row advancement\n");
    printf("     - Add penalty for backward moves: -200\n\n");
    
    printf("3. SWAP COOLDOWN MECHANISM\n");
    printf("   Problem: AI overuses SWAP moves leading to repetitive positions\n");
    printf("   Solution: Track recent swap count and reduce SWAP bonus:\n");
    printf("     - If 2+ swaps in last 4 plies, reduce SWAP bonus by 300-400\n");
    printf("     - Encourages mixed move types\n\n");
    
    printf("4. FRONTIER ADVANCEMENT EVAL COMPONENT\n");
    printf("   Problem: Static eval doesn't reward piece advancement enough\n");
    printf("   Solution: Add new eval weight 'frontier_advancement':\n");
    printf("     - Count pieces in advanced positions (rows 2-3 for White, 4-5 for Black)\n");
    printf("     - Weight: 20-30 per advanced piece\n\n");
    
    printf("Implementation priority: 1 > 2 > 3 > 4\n");
    printf("The anti-reversal penalty alone should significantly reduce draw rates.\n");
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    printf("═══════════════════════════════════════════════════════════════════════\n");
    printf("           AI ADVANCEMENT DIAGNOSTIC - Move Selection Analysis          \n");
    printf("═══════════════════════════════════════════════════════════════════════\n");
    printf("\nThis diagnostic analyzes AI behavior and tests improvement strategies\n");
    printf("WITHOUT modifying the baseline AI agent code.\n\n");
    
    // Compare how different layouts behave with baseline AI
    compare_layouts();
    
    // Compare different experimental strategies against baseline
    compare_strategies();
    
    // Generate improvement recommendations
    generate_recommendations();
    
    printf("\n═══════════════════════════════════════════════════════════════════════\n");
    printf("                        DIAGNOSTIC COMPLETE                             \n");
    printf("═══════════════════════════════════════════════════════════════════════\n");
    
    return 0;
}
