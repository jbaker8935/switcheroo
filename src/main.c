#define F256LIB_IMPLEMENTATION
#include "f256lib.h"
#include "../src/game_state.h"
#include "../src/input.h"
#include "../src/input_handler.h"
#include "../src/render.h"
#include "../src/platform_f256.h"
#include "../src/text_display.h"
#include "../src/puzzle_data.h"
#include "../src/ai_agent.h"
#include "stddef.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Forward declarations
extern void platform_bootstrap(void);
extern void platform_idle(void);
extern void video_init(const void *config);
extern void video_reset(void);
extern void display_test(void);
// Global game state
static game_state_t g_game_state;

#ifndef WITHOUT_FILE
#if defined(__llvm_mos__)
uint8_t write_row = 1u;
uint8_t write_col = 0u;
uint32_t far_mem_address = 0x60000u; // Example far memory address for file storage

static void trace_write_literal(FILE *file, const char *text)
{
    if (!file || !text)
    {
        return;
    }

    size_t length = strlen(text);
    if (length == 0u)
    {
        return;
    }
    // int16_t      fileWrite(void *buf, uint16_t nbytes, uint16_t nmemb, uint8_t *fd);
    // if (fileWrite((void *)text, (uint16_t) 1u, (uint16_t)length, (uint8_t *) file) == (uint16_t)length)
    // {
    //     // Success
    // }
    // else
    // {
    //     textGotoXY(write_col, write_row);
    //     textPrint("TRACE SAVE ERR");
    //     return;
    // }
    for (size_t i = 0u; i < length; ++i)
    {
        FAR_POKE(far_mem_address, (uint8_t)*(text+i));
        far_mem_address++;
    }
    // textGotoXY(write_col, write_row);
    // textPrint((char *)text);
    // if (*(text + length - 1u) == '\n') {
    //     write_col = 0u;
    //     write_row = (write_row + 1u) % 60u;
    //     return;
    // } else {
    //     write_col += (uint8_t)length;
    // }
    // write_row = (write_row + 1u) % 60u;
}

static void trace_write_decimal_u32(FILE *file, uint32_t value)
{
    char buffer[11];
    uint8_t index = 0u;

    if (value == 0u)
    {
        buffer[index++] = '0';
    }
    else
    {
        char digits[10];
        while (value != 0u && index < sizeof(digits))
        {
            digits[index++] = (char)('0' + (value % 10u));
            value /= 10u;
        }

        for (uint8_t i = 0u; i < index; ++i)
        {
            buffer[i] = digits[index - 1u - i];
        }
    }

    buffer[index] = '\0';
    trace_write_literal(file, buffer);
}

static void trace_write_decimal_u8(FILE *file, uint8_t value)
{
    trace_write_decimal_u32(file, (uint32_t)value);
}

static void trace_write_decimal_i16(FILE *file, int16_t value)
{
    int32_t temp = (int32_t)value;
    if (temp < 0)
    {
        trace_write_literal(file, "-");
        temp = -temp;
    }
    trace_write_decimal_u32(file, (uint32_t)temp);
}

static void trace_write_perspective(FILE *file, player_t perspective)
{
    switch (perspective)
    {
        case PLAYER_WHITE:
            trace_write_literal(file, "WHITE");
            break;
        case PLAYER_BLACK:
            trace_write_literal(file, "BLACK");
            break;
        case PLAYER_NONE:
        default:
            trace_write_literal(file, "NONE");
            break;
    }
}

static void dump_hint_trace_to_file(void)
{
    const ai_hint_eval_record_t *records = NULL;
    uint8_t count = ai_agent_hint_trace_get(&records);
    if (!records)
    {
        print_puzzle_debug("TRACE BUFFER MISSING", "");
        return;
    }

    // fileReset();
    FILE *file = fopen("HINTTRACE.CSV", "w");
    if (!file)
    {
        print_puzzle_debug("TRACE SAVE FAILED", "OPEN ERR");
        return;
    }


    trace_write_literal(file, "index,board_hash,perspective,total,swap,block,goal,depth,ply\n");

    for (uint8_t i = 0u; i < count; ++i)
    {
        const ai_hint_eval_record_t *entry = &records[i];

        trace_write_decimal_u8(file, i);
        trace_write_literal(file, ",");
        trace_write_decimal_u32(file, entry->board_hash);
        trace_write_literal(file, ",");
        trace_write_perspective(file, entry->perspective);
        trace_write_literal(file, ",");
        trace_write_decimal_i16(file, entry->total);
        trace_write_literal(file, ",");
        trace_write_decimal_i16(file, entry->swap_contrib);
        trace_write_literal(file, ",");
        trace_write_decimal_i16(file, entry->block_contrib);
        trace_write_literal(file, ",");
        trace_write_decimal_i16(file, entry->goal_contrib);
        trace_write_literal(file, ",");
        trace_write_decimal_u8(file, entry->depth);
        trace_write_literal(file, ",");
        trace_write_decimal_u8(file, entry->ply);
        trace_write_literal(file, "\n");
    }

    fclose(file);
    // print_puzzle_debug("TRACE SAVED", "HINTTRACE.CSV");
}
#else
static const char *ai_perspective_label(player_t perspective)
{
    switch (perspective)
    {
        case PLAYER_WHITE:
            return "WHITE";
        case PLAYER_BLACK:
            return "BLACK";
        case PLAYER_NONE:
        default:
            return "NONE";
    }
}

static void dump_hint_trace_to_file(void)
{
    const ai_hint_eval_record_t *records = NULL;
    uint8_t count = ai_agent_hint_trace_get(&records);
    if (!records)
    {
        print_puzzle_debug("TRACE BUFFER MISSING", "");
        return;
    }

    FILE *file = fopen("HINTTRACE.CSV", "w");
    if (!file)
    {
        print_puzzle_debug("TRACE SAVE FAILED", "OPEN ERR");
        return;
    }

    fprintf(file, "index,board_hash,perspective,total,swap,block,goal,depth,ply\n");

    for (uint8_t i = 0u; i < count; ++i)
    {
        const ai_hint_eval_record_t *entry = &records[i];

        fprintf(file,
                "%u,%lu,%s,%d,%d,%d,%d,%u,%u\n",
                (unsigned)i,
                (unsigned long)entry->board_hash,
                ai_perspective_label(entry->perspective),
                (int)entry->total,
                (int)entry->swap_contrib,
                (int)entry->block_contrib,
                (int)entry->goal_contrib,
                (unsigned)entry->depth,
                (unsigned)entry->ply);
    }

    fclose(file);
    print_puzzle_debug("TRACE SAVED", "HINTTRACE.CSV");
}
#endif
#else
static void dump_hint_trace_to_file(void)
{
}
#endif

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint16_t old_move_count = 0;

    // Initialize f256lib (includes kernelReset and all subsystems)
    f256Init();
    // Initialize subsystems
    video_init(NULL); // Use default config
    text_display_init();
    input_init();
    input_handler_init();

    // Initialize game state
    game_state_init(&g_game_state);
    render_update_score(&g_game_state.stats);
    game_state_start_new_game(&g_game_state);

    // Initialize rendering
    render_init();

    // Main game loop

    // display_test();

    print_ai_difficulty(g_game_state.ai_config.difficulty);
    ai_agent_hint_trace_enable(false);
    print_game_mode(g_game_state.is_puzzle_mode);
    print_swap_rule(g_game_state.ai_config.swap_rule);
    print_current_player(g_game_state.board.current_player);
    print_move_history(g_game_state.board.history, g_game_state.board.history_count);    
    while (game_state_get_phase(&g_game_state) != GAME_PHASE_EXIT)
    {
        // print_game_mode(g_game_state.is_puzzle_mode);
        // print_swap_rule(g_game_state.ai_config.swap_rule);
        // print_current_player(g_game_state.board.current_player);
        // print_move_history(g_game_state.board.history, g_game_state.board.history_count);
        // Update game state
        game_state_update(&g_game_state, 1.0f / 60.0f);

        if (g_game_state.phase == GAME_PHASE_GAME_OVER)
        {
            print_game_winner(g_game_state.win_path.winner);
            if (g_game_state.is_puzzle_mode)
            {
                // In puzzle mode, mark puzzle as solved if player won
                // in N Player A moves or less 
                // retrieve current puzzle and its difficulty
                if (g_game_state.win_path.winner == PLAYER_WHITE)
                {
                    const puzzle_collection_t *collection = get_puzzle_collection();
                    const puzzle_t *puzzle = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                    if (puzzle && !puzzle->is_solved && ((g_game_state.board.move_count+1)/2) <= puzzle->difficulty)
                    {
                        // Mark puzzle as solved in persistent storage
                        mark_puzzle_solved(g_game_state.prefs.current_puzzle_index);
                        // confirm write.
                        const puzzle_t * puzzle_updated = get_puzzle_by_index(g_game_state.prefs.current_puzzle_index);
                        print_puzzle_info(g_game_state.prefs.current_puzzle_index, collection->count, puzzle_updated->difficulty, puzzle_updated->is_solved);
                    }
                }
            }
        }

        if (g_game_state.board.move_count != old_move_count)
        {
            print_move_history(g_game_state.board.history, g_game_state.board.history_count);
            print_current_player(g_game_state.board.current_player);
            old_move_count = g_game_state.board.move_count;
        }

        // Process input events - drain all pending events like sprites example
        do
        {
            // Get next kernel event (always call, like the example)
            kernelNextEvent();

            // Translate kernel event to input event
            input_event_t event;
            if (input_translate_event(&event))
            {
                // Process event through input handler
                input_handler_process_event(&g_game_state, &event);
                if (g_game_state.board.move_count != old_move_count)
                {
                    print_move_history(g_game_state.board.history, g_game_state.board.history_count);
                    print_current_player(g_game_state.board.current_player);
                    old_move_count = g_game_state.board.move_count;
                }
            }
        } while (kernelGetPending() > 0);

        // Diagnostic text output disabled in release builds to conserve ROM/RAM.

        // Update rendering
        render_update(&g_game_state);

        // Idle/wait for next frame
        platform_idle();
    }

    textClear();
    // dump_hint_trace_to_file();
    // getchar();

    // soft reset
    POKE(0xD6A2, 0xDE);
    POKE(0xD6A3, 0xAD);
    POKE(0xD6A0, 0x80); // arm reset
    POKE(0xD6A0, 0x00); // trigger reset

    return 0;
}
