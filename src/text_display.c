#include "../src/text_display.h"
#include "../src/board.h"
#include "../src/ai_agent.h"
#include "../src/puzzle_data.h"
#include "../src/mouse_pointer.h"
#include "../src/help_text.h"
#include "../src/video.h"
#include "../src/timer.h"

#include <stddef.h>
#include <string.h>

// Static flag to track if blunder message is currently displayed
static bool blunder_message_active = false;

/*
 * @file text_display.c
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

// Helper function to count digits in a number
 uint8_t count_digits(uint16_t n) {
    uint8_t count = 0;
    do {
        count++;
        n /= 10;
    } while (n > 0);
    return count;
}


void display_hide_help_screen(void) {
    // Clear help text area
    clear_text_matrix();
    textEnableBackgroundColors(false);
}

void display_show_help_screen(void) {
    // Clear text area
    clear_text_matrix();

    textEnableBackgroundColors(true);

    textSetColor(1,1);
    // Display help text
    const char *line_start = help_text;
    uint8_t row = 0;

    while (*line_start != '\0' && row < 60) {
        const char *line_end = line_start;
        // Find the end of the current line
        while (*line_end != '\n' && *line_end != '\0') {
            line_end++;
        }

        // Calculate line length
        size_t line_length = line_end - line_start;


        // Create a buffer for the line
        char buffer[81]; // 80 chars + null terminator
        strncpy(buffer, line_start, line_length);
        buffer[line_length] = '\0';

        // Print the line
        print_formatted_text(0, row, buffer);
        row++;

        // Move to the next line
        if (*line_end == '\n') {
            line_start = line_end + 1;
        } else {
            break; // End of text
        }
    }

}

void print_formatted_text(uint8_t x, uint8_t y, const char *text) {
    char buf[] = {' ','\0'};
    char current_x = x;
    size_t len = strlen(text);
    for (uint8_t i = 0; i < len; ++i) {
        // test for ^ followed by a digit
        // for digits 1,2,3,4,5,6 convert to foreground color change
        // if ^ is followed by % then print a single %
        if (text[i] == '^' && (i + 1) < len) {
            char next_char = text[i + 1];
            if (next_char >= '1' && next_char <= '6') {
                uint8_t color_index = (uint8_t)(next_char - '0');
                textSetColor(color_index, 1);
                i++; // Skip next char
                continue;
            } else if (next_char == '%') {
                // Print a single ^
                buf[0] = '^';
                textGotoXY(current_x, y);
                textPrint(buf);
                current_x++;
                i++; // Skip next char
                continue;
            }
        }
        buf[0] = text[i];
        textGotoXY(current_x, y);
        textPrint(buf);
        current_x++;
    }
}


void text_display_init(void) {
    // Clear the entire text area
    for (uint8_t row = 0; row < 60; ++row) {
        print_formatted_text(0, row, "                         ");
    }

    /* Text Colors */
    textDefineForegroundColor(1,0xaa,0xaa,0xaa); // #aaaaaa Normal Text
    textDefineForegroundColor(2,0x4e,0xff,0x89); // #4eff89 Win Text
    textDefineForegroundColor(3,0xff,0x6b,0x6b);  // #ff6b6b Loss Text
    textDefineForegroundColor(4,0x7a,0xba,0xed);  // #7abaed - Blue for Logo
    textDefineForegroundColor(5,0xc8,0x9d,0xdf);     // #c89ddf - Light Purple for Logo
    textDefineForegroundColor(6,0xee,0xee,0xee);     // #eeeeee - Lighter Text    
    textDefineBackgroundColor(1, 40,40,40);    
    textSetColor(1,1);   // Default to normal text color
}

void print_win_loss(uint16_t win_count, uint16_t loss_count) {
    const char * win_loss_str = "^6Win      Loss^1";

    print_formatted_text(4, 3, win_loss_str);
    textGotoXY(5, 5);
    textSetColor(2,0);
    textPrintUInt(win_count);
    textGotoXY(14, 5);
    textSetColor(3,0);
    textPrintUInt(loss_count);
    textSetColor(1,0);     // Reset to normal text color
}

void print_game_exit(void) {
        print_formatted_text(3, 10, "^1Game Exit ...        ");
}

void print_puzzle_clock(uint16_t elapsed_ticks) {
    // Convert ticks to seconds
    uint16_t total_seconds = elapsed_ticks / T0_TICK_FREQ; // Assuming 30 ticks per second

    print_formatted_text(3, 26, "^6Timer: ^1");
    textGotoXY(10, 26);
    textPrintUInt(total_seconds);
    uint8_t index_digits = count_digits(total_seconds);
    print_formatted_text(10 + index_digits, 26, " sec       ");
}

void print_game_winner(player_t winner) {
    const char *win_str = (winner == PLAYER_WHITE) ? "^2Player Wins!^1         " : "^3Engine Wins!^1         ";
    print_formatted_text(3, 10, win_str);
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    // Clear blunder message when game ends
    blunder_message_active = false;
}

void print_current_player(player_t player) {
    // Don't overwrite blunder message
    if (blunder_message_active) {
        return;
    }
    
    const char *player_str = (player == PLAYER_WHITE) ? "^6Player's Move^1        " : "^6Thinking ...^1         ";
    if (player == PLAYER_WHITE) {
        set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    } else {
        set_mouse_cursor(MOUSE_CURSOR_BUSY);
    }
    print_formatted_text(3, 10, player_str);
}

void text_display_update_ai_thinking_indicator(uint8_t dot_count) {
    if (dot_count > 3u) {
        dot_count = 3u;
    }

    char buffer[22];
    const char base[] = "^6Thinking";
    size_t idx = 0;

    while (base[idx] != '\0' && idx < sizeof(buffer) - 1u) {
        buffer[idx] = base[idx];
        ++idx;
    }

    if (idx < sizeof(buffer) - 1u) {
        buffer[idx++] = ' ';
    }

    for (uint8_t i = 0; i < dot_count && idx < sizeof(buffer) - 1u; ++i) {
        buffer[idx++] = '.';
    }

    while (idx < sizeof(buffer) - 1u) {
        buffer[idx++] = ' ';
    }

    buffer[sizeof(buffer) - 1u] = '\0';
    print_formatted_text(3, 10, buffer);
    textSetColor(1,1);
}

void print_game_mode(bool is_puzzle_mode) {
    const char *mode_str = is_puzzle_mode ? "^6Mode: ^1Puzzle   " : "^6Mode: ^1Free Play";
    print_formatted_text(3, 14, mode_str);

    // sneak F1 for Help message
    print_formatted_text(2, 58, "Press [^6F1^1] for Help");
}

void print_swap_rule(swap_rule_t rule) {
    const char *rule_str = "";
    switch (rule) {
        case SWAP_RULE_CLASSIC:
            rule_str = "^6Rule: ^1Any - All ";
            break;
        case SWAP_RULE_CLEARS_OWN:
            rule_str = "^6Rule: ^1Any - Own ";
            break;
        case SWAP_RULE_SWAPPED_CLEARS:
            rule_str = "^6Rule: ^1Swap - All";
            break;
        case SWAP_RULE_SWAPPED_CLEARS_OWN:
            rule_str = "^6Rule: ^1Swap - Own";
            break;
        default:
            rule_str = "^6Rule: ^1Unknown   ";
            break;
    }
    print_formatted_text(3,18, rule_str);
}

void print_ai_difficulty(ai_difficulty_t difficulty) {
    const char *diff_str;
    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            diff_str = "^6Engine: ^1Learn   ";
            break;
        case AI_DIFFICULTY_EASY:
            diff_str = "^6Engine: ^1Easy    ";
            break;
        case AI_DIFFICULTY_STANDARD:
            diff_str = "^6Engine: ^1Standard";
            break;
        case AI_DIFFICULTY_EXPERT:
            diff_str = "^6Engine: ^1Expert  ";
            break;
        default:
            diff_str = "^6Engine: ^1Unknown ";
            break;
    }
    print_formatted_text(3, 16, diff_str);
}
void clear_puzzle_info() {
    print_formatted_text(3, 22, "                         ");
    print_formatted_text(3, 24, "                         ");
    print_formatted_text(3, 26, "                         ");    
}

void print_puzzle_debug(const char *line1, const char *line2) {
    const char *first = line1 ? line1 : "                         ";
    const char *second = line2 ? line2 : "                         ";
    print_formatted_text(3, 55, first);
    print_formatted_text(3, 56, second);
}

void clear_puzzle_debug(void) {
    print_puzzle_debug("                         ", "                         ");
}

void print_puzzle_info(uint16_t puzzle_index, uint16_t total_puzzles, 
                       uint8_t puzzle_difficulty, bool is_solved) {
    char *buf = "";
    char checked[] = { 222, '\0'};
    const uint8_t start_row = 22;
    // Puzzle Number
    buf = "^6Puzzle:^1               ";
    print_formatted_text(3, start_row, buf);
    uint8_t index_digits = count_digits(puzzle_index + 1);
    textGotoXY(11, start_row);
    textPrintUInt(puzzle_index + 1);
    textGotoXY(11 + index_digits, start_row);
    textPrint("/");
    textGotoXY(11 + index_digits + 1, start_row);
    textPrintUInt(total_puzzles);
    textGotoXY(10, start_row);
    if(is_solved) {
        textPrint(checked);
    } else {
        textPrint(" ");
    }

    // Win In
    buf = "^6Win In:^1 ";
    print_formatted_text(3, start_row + 2, buf);
    textGotoXY(11, start_row + 2);
    textPrintUInt(puzzle_difficulty);
}

void print_swap_unavailable(void) {
    print_formatted_text(3, 55, "^3Rule cannot be  ");
    print_formatted_text(3, 56, "changed mid-game^1");
}

void clear_swap_unavailable(void) {
    print_formatted_text(3, 55, "                         ");
    print_formatted_text(3, 56, "                         ");
}

void print_made_blunder(void) {
    print_formatted_text(3, 10, "^3Blunder!^1             ");
    blunder_message_active = true;
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
}  
void clear_made_blunder(void) {
    print_formatted_text(3, 10, "                     ");
    blunder_message_active = false;
}

void print_AI_hint(const char *hint) {
    char *buf = "^6Hint:^1                ";
    char * hint_str = hint ? (char *)hint : (char *)"N/A";
    print_formatted_text(3,10, buf);
    textGotoXY(9,10);
    textPrint(hint_str);
    // Clear blunder message when showing hint
    blunder_message_active = false;
}

void print_puzzle_hint(const char *hint) {
    print_AI_hint(hint);
}

static uint8_t pd_append_char(char *buf, size_t buf_size, uint8_t pos, char ch) {
    if (buf && pos + 1 < buf_size) {
        buf[pos] = ch;
    }
    return (uint8_t)(pos + 1u);
}


static uint8_t pd_format_move(char *buf, size_t buf_size, player_t player,
    uint8_t from_col, uint8_t from_row,
    uint8_t to_col, uint8_t to_row, bool is_swap) {
        uint8_t pos = 0;
        if (buf_size == 0) {
            return 0;
        }
        
        pos = pd_append_char(buf, buf_size, pos, (char)('A' + from_col));
        pos = pd_append_char(buf, buf_size, pos, (char)('0' + from_row));
        pos = pd_append_char(buf, buf_size, pos, '-');
        pos = pd_append_char(buf, buf_size, pos, '>');
        pos = pd_append_char(buf, buf_size, pos, (char)('A' + to_col));
        pos = pd_append_char(buf, buf_size, pos, (char)('0' + to_row));
        
        if (is_swap) {
            pos = pd_append_char(buf, buf_size, pos, ' ');
            pos = pd_append_char(buf, buf_size, pos, 'S');            
        }
        
        if (buf) {
            if (pos < buf_size) {
                buf[pos] = '\0';
            } else {
                buf[buf_size - 1] = '\0';
            }
        }
        
        return pos;
    }
    
    
    void display_puzzle_solution(const puzzle_t *puzzle) {
        const uint16_t *solution = puzzle->solution;
        uint8_t i = 0;  // Only show first move - future function to show longer hint ... maybe.
        // for (uint8_t i = 0; i < puzzle->solution_length; i++) {
            uint8_t player_packed = solution[i * 2];
            uint16_t move_packed = solution[i * 2 + 1];
            
            player_t player = (player_t)player_packed;
            uint8_t move_type = MOVE_UNPACK_TYPE(move_packed);
            
            char buf[26]; // 25 chars + null terminator
            uint8_t len;
            
            uint8_t from_pos = MOVE_UNPACK_FROM_POS(move_packed);
            uint8_t to_pos = MOVE_UNPACK_TO_POS(move_packed);
            
            uint8_t from_row = POS_UNPACK_ROW(from_pos);
            uint8_t from_col = POS_UNPACK_COL(from_pos);
            uint8_t to_row = POS_UNPACK_ROW(to_pos);
            uint8_t to_col = POS_UNPACK_COL(to_pos);
            
            uint8_t chess_from_row = (uint8_t)(8u - from_row);
            uint8_t chess_to_row = (uint8_t)(8u - to_row);
            
            len = pd_format_move(buf, sizeof(buf), player,
            from_col, chess_from_row,
            to_col, chess_to_row,
            move_type == 0);
            
            // Right-fill with spaces to exactly 25 characters
            while (len < 25) {
                buf[len++] = ' ';
            }
            buf[25] = '\0';
            
            print_puzzle_hint(buf);
        // }
    }
    
    
void clear_puzzle_hint() {
    print_formatted_text(3,10, "                         ");
} 

uint8_t format_move_string(char *buf, size_t buf_size, const move_t *move) {
    if (!buf || buf_size == 0 || !move) {
        return 0;
    }
    return pd_format_move(buf, buf_size, move->player,
        move->from_col, 8 - move->from_row,
        move->to_col, 8 - move->to_row,
        move->type == MOVE_TYPE_SWAP);
}

void print_move_history(const move_t *history, uint8_t move_count) {
    const uint8_t start_row = 31;


    print_formatted_text(5, start_row, "^6Move History^1");
    for (uint8_t i = 0; i < 8; ++i) {
        if (i < move_count) {
            const move_t *move = &history[i];
            char movestr[9] = "F1->T1 S";
            print_formatted_text(3, start_row + 3 + i*2, move->player == PLAYER_WHITE ? "^4Player: ^1" : "^5Engine: ^1");
            movestr[0] = (char) ( 'A' + move->from_col);
            movestr[1] = (char) ('0' + (8 - move->from_row));
            movestr[4] = (char) ('A' + move->to_col);
            movestr[5] = (char) ('0' + (8 - move->to_row));
            movestr[7] = (char) ((move->type == MOVE_TYPE_SWAP) ? 'S' : ' ');
            movestr[8] = '\0';
            textGotoXY(11, start_row + 3 + i*2);
            textPrint(movestr);

        } else {
            print_formatted_text(3, start_row + 3 + i*2, "                     ");
        }
    }
}

