#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

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

static void setup_board(board_t *board, const char *placement) {
    if (!board || !placement || *placement == '\0') {
        return;
    }

    char buffer[256];
    strncpy(buffer, placement, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';

    char *token = strtok(buffer, ",");
    while (token) {
        while (*token == ' ') {
            ++token;
        }

        size_t len = strlen(token);
        if (len < 4u || token[2] != ':') {
            fprintf(stderr, "Invalid token in setup string: %s\n", token);
            token = strtok(NULL, ",");
            continue;
        }

        char column_char = (char)toupper((unsigned char)token[0]);
        char row_char = token[1];
        char piece_char = token[3];

        if (column_char < 'A' || column_char >= ('A' + BOARD_COLS)) {
            fprintf(stderr, "Invalid column in setup string: %c\n", column_char);
            token = strtok(NULL, ",");
            continue;
        }

        if (row_char < '1' || row_char > '8') {
            fprintf(stderr, "Invalid row in setup string: %c\n", row_char);
            token = strtok(NULL, ",");
            continue;
        }

        uint8_t col = (uint8_t)(column_char - 'A');
        uint8_t row = (uint8_t)(row_char - '1');

        piece_type_t piece = PIECE_NONE;
        switch (piece_char) {
            case 'W':
                piece = PIECE_WHITE_NORMAL;
                break;
            case 'w':
                piece = PIECE_WHITE_SWAPPED;
                break;
            case 'B':
                piece = PIECE_BLACK_NORMAL;
                break;
            case 'b':
                piece = PIECE_BLACK_SWAPPED;
                break;
            default:
                fprintf(stderr, "Invalid piece type in setup string: %c\n", piece_char);
                token = strtok(NULL, ",");
                continue;
        }

        board_set_piece(board, row, col, piece);
        token = strtok(NULL, ",");
    }
}

static const char *difficulty_label(ai_difficulty_t difficulty) {
    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            return "Learning";
        case AI_DIFFICULTY_EASY:
            return "Easy    ";
        case AI_DIFFICULTY_STANDARD:
            return "Standard";
        case AI_DIFFICULTY_EXPERT:
            return "Expert  ";
        default:
            return "Unknown ";
    }
}

static bool run_scenario(const char *label, const char *placement, ai_difficulty_t difficulty) {
    board_t board;
    board_init(&board);
    clear_board(&board);
    board.current_player = PLAYER_BLACK;

    setup_board(&board, placement);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, difficulty, PLAYER_BLACK);

    config.search.node_limit = 500000u;
    config.search.max_depth = 8u;
    config.search.use_transposition = true;
    config.use_hint_profile = true;

    printf("-- %s --\n", label);
    printf("AI config: ai_player=%d (WHITE=%d, BLACK=%d)\n",
           config.ai_player, PLAYER_WHITE, PLAYER_BLACK);
    if (config.ai_player != PLAYER_BLACK) {
        fprintf(stderr, "✗ ERROR: config.ai_player is not PLAYER_BLACK (value=%d)\n", config.ai_player);
        return false;
    }

    printf("Board perspective: current_player=%d (expect %d)\n",
           board.current_player, PLAYER_BLACK);
    if (board.current_player != PLAYER_BLACK) {
        fprintf(stderr, "✗ ERROR: board current player is not PLAYER_BLACK\n");
        return false;
    }

    printf("Testing with %s mode (base_depth=%d, max_depth=%d, node_limit=%u)\n",
           difficulty_label(difficulty),
           config.search.base_depth,
           config.search.max_depth,
           config.search.node_limit);

    move_t best_move;
    bool found = ai_agent_find_best_move(&board, &config, &best_move);
    if (!found) {
        printf("\n✗ ERROR: No move found!\n");
        return false;
    }

    printf("\nAI selected: row %d, col %d -> row %d, col %d (%s)\n",
           best_move.from_row, best_move.from_col,
           best_move.to_row, best_move.to_col,
           best_move.type == MOVE_TYPE_SWAP ? "swap" : "empty");

    char from_col = (char)('A' + best_move.from_col);
    int from_row = best_move.from_row + 1;
    char to_col = (char)('A' + best_move.to_col);
    int to_row = best_move.to_row + 1;

    printf("Chess notation: %c%d->%c%d\n\n", from_col, from_row, to_col, to_row);
    return true;
}

int main(void) {
    printf("=== SEARCH TEST ===\n\n");

    const ai_difficulty_t difficulty = AI_DIFFICULTY_EXPERT;

    // const bool first_ok = run_scenario(
    //     "Scenario 1",
    //     "A3:w,B3:w,D3:w,A4:w,B5:w,C5:w,A6:w,C6:w,B2:b,B4:b,C4:b,D4:b,A5:b,D5:b,B6:b,D6:b",
    //     difficulty);

    // const bool second_ok = run_scenario(
    //     "Scenario 2",
    //     "A3:w,A4:w,A5:W,A6:w,B3:w,B5:w,C6:w,D4:W,B2:b,B4:b,B6:b,C4:B,C5:b,D3:B,D5:B,D6:b",
    //     difficulty);

    // const bool third_ok = run_scenario(
    //     "Scenario 3",
    //     "C2:W,A3:W,B3:w,C3:w,B4:W,A5:w,C6:w,D7:w,A4:b,C4:b,D4:B,B5:B,C5:B,D5:b,B6:b,C7:b",
    //     difficulty);

    const bool forth_ok = run_scenario(
        "Scenario 4",
        "B2:b,C2:w,A3:w,B3:b,A4:b,B4:b,C4:B,D4:b,A5:w,B5:w,C5:b,D5:w,A6:W,B6:b,C6:w,D6:W",
        difficulty);



    return 0;
}
