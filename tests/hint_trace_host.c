#define AI_AGENT_HOST_TEST

#include "../src/board.h"
#include "../src/ai_agent.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void textGotoXY(uint8_t x, uint8_t y)
{
    (void)x;
    (void)y;
}

void textPrint(char *text)
{
    (void)text;
}

void render_invalidate_cache(void)
{
}

void print_made_blunder(void)
{
}

static uint8_t column_index(char col)
{
    switch (col)
    {
        case 'A':
            return 0u;
        case 'B':
            return 1u;
        case 'C':
            return 2u;
        case 'D':
        default:
            return 3u;
    }
}

static void place_piece(board_t *board, char col, uint8_t row1_based, char player, bool swapped)
{
    const uint8_t row = (uint8_t)(row1_based - 1u);
    const uint8_t col_idx = column_index(col);

    piece_type_t piece = PIECE_NONE;
    if (player == 'A')
    {
        piece = swapped ? PIECE_WHITE_SWAPPED : PIECE_WHITE_NORMAL;
    }
    else if (player == 'B')
    {
        piece = swapped ? PIECE_BLACK_SWAPPED : PIECE_BLACK_NORMAL;
    }

    board_set_piece(board, row, col_idx, piece);
}

static void load_classic_depth3_14(board_t *board)
{
    for (uint8_t row = 0u; row < BOARD_ROWS; ++row)
    {
        for (uint8_t col = 0u; col < BOARD_COLS; ++col)
        {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }

    place_piece(board, 'D', 1u, 'A', false);
    place_piece(board, 'B', 2u, 'A', false);
    place_piece(board, 'C', 2u, 'B', false);
    place_piece(board, 'D', 2u, 'A', false);
    place_piece(board, 'C', 3u, 'A', true);
    place_piece(board, 'A', 4u, 'B', false);
    place_piece(board, 'D', 4u, 'A', true);
    place_piece(board, 'A', 5u, 'B', false);
    place_piece(board, 'C', 5u, 'B', true);
    place_piece(board, 'D', 5u, 'B', false);
    place_piece(board, 'B', 6u, 'B', false);
    place_piece(board, 'D', 6u, 'A', true);
    place_piece(board, 'A', 7u, 'A', false);
    place_piece(board, 'D', 7u, 'A', true);
    place_piece(board, 'B', 8u, 'B', false);
    place_piece(board, 'D', 8u, 'B', true);

    board->current_player = PLAYER_BLACK;  // BLACK's turn
    board->move_count = 0u;
    board->history_count = 0u;

    for (uint8_t i = 0u; i < MAX_MOVE_HISTORY; ++i)
    {
        board->history[i] = (move_t){0};
    }
}

static void print_trace(const ai_hint_eval_record_t *records, uint8_t count)
{
    for (uint8_t i = 0u; i < count; ++i)
    {
        const ai_hint_eval_record_t *entry = &records[i];
        const char *perspective = "NONE";
        if (entry->perspective == PLAYER_WHITE)
        {
            perspective = "WHITE";
        }
        else if (entry->perspective == PLAYER_BLACK)
        {
            perspective = "BLACK";
        }

        printf("%02u,%lu,%s,%d,%d,%d,%d,%u,%u\n",
               (unsigned)i,
               (unsigned long)entry->board_hash,
               perspective,
               (int)entry->total,
               (int)entry->swap_contrib,
               (int)entry->block_contrib,
               (int)entry->goal_contrib,
               (unsigned)entry->depth,
               (unsigned)entry->ply);
    }
}

int main(void)
{
    board_t board;
    board_init(&board);
    load_classic_depth3_14(&board);

    ai_config_t config;
    ai_agent_init(&config, SWAP_RULE_CLASSIC, AI_DIFFICULTY_EXPERT, PLAYER_BLACK);
    config.use_hint_profile = true;  // Puzzle mode setting

    ai_agent_hint_trace_enable(true);
    ai_agent_hint_trace_clear();

    move_t best_move;
    bool found = ai_agent_find_best_move(&board, &config, &best_move);
    printf("found=%s\n", found ? "true" : "false");
    if (found)
    {
        printf("best_move: (%u,%u)->(%u,%u) type=%u\n",
               best_move.from_row,
               best_move.from_col,
               best_move.to_row,
               best_move.to_col,
               (unsigned)best_move.type);
    }

    print_trace(s_hint_trace.records, s_hint_trace.count);
    return 0;
}

    const ai_hint_eval_record_t *records = NULL;
    uint8_t count = ai_agent_hint_trace_get(&records);
    printf("trace_count=%u\n", (unsigned)count);
    print_trace(records, count);

    return 0;
}