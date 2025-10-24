#include "../src/text_display.h"
#include "../src/board.h"
#include "../src/ai_agent.h"
#include "../src/puzzle_data.h"
#include "../src/mouse_pointer.h"
#include "../src/twidget.h"

#include <stddef.h>
#include <string.h>

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

CharMap char_map;

// Helper function to count digits in a number
 uint16_t countDigits(uint16_t n) {
    uint16_t count = 0;
    do {
        count++;
        n /= 10;
    } while (n > 0);
    return count;
}


void print_formatted_text(uint8_t x, uint8_t y, const char *text) {
    
    textGotoXY(x, y);
    textPrint((char *) text);
}

/* Display character callback - puts character at (x,y) */
void display_char_callback(uint8_t x, uint8_t y, uint8_t character, uint8_t mode) {
    char buf[2] = { character, '\0' };
    textGotoXY(x, y);
    textPrint(buf);
}

/* Gets character at (x,y) */

uint8_t get_char_callback(uint8_t x, uint8_t y) {
    uint8_t c;
    POKE(0x0001,2);
    c = PEEK(0xC000 + x * 80 + y);
    POKE(0x0001,0);
    return c;
}
/* Gets mode at (x,y) */
uint8_t get_mode_callback(uint8_t x, uint8_t y) {
    return 0 ;
}

void text_display_init(void) {
    // Clear the entire text area
    for (uint8_t row = 0; row < 60; ++row) {
        print_formatted_text(0, row, "                         ");
    }

    char_map.radio_unselected = 179;      /* ○ */
    char_map.radio_selected = 225;        /* ● */
    char_map.checkbox_unchecked = 227;    /* ☐ */
    char_map.checkbox_checked = 222;      /* ☑ */
    char_map.box_down_right = 160;        /* ┌ */
    char_map.box_down_left = 161;         /* ┐ */
    char_map.box_horizontal = 150;        /* ─ */
    char_map.box_vertical = 130;          /* │ */
    char_map.box_up_right = 162;          /* └ */
    char_map.box_up_left = 163;           /* ┘ */

    GetCharCallback gccb = get_char_callback;
    GetModeCallback gmcb = get_mode_callback;
    DisplayCharCallback dccb = display_char_callback;

    widget_init(dccb, gccb, gmcb, &char_map);

    /* Text Colors */
    textDefineForegroundColor(1,170,170,170); // Normal Text
    textDefineForegroundColor(2,0x4e,0xff,0x89); // #4eff89 Win Text
    textDefineForegroundColor(3,0xff,0x6b,0x6b);  // #ff6b6b Loss Text
    textDefineForegroundColor(4,0x7a,0xba,0xed);  // #7abaed - Blue for Logo
    textDefineForegroundColor(5,0xc8,0x9d,0xdf);     // #c89ddf - Light Purple for Logo
    textSetColor(1,0);   // Default to normal text color
}

void print_win_loss(uint16_t win_count, uint16_t loss_count) {
    const char * win_loss_str = "Win      Loss";

    print_formatted_text(4, 3, win_loss_str);
    textGotoXY(5, 5);
    textSetColor(2,0);
    textPrintUInt(win_count);
    textGotoXY(14, 5);
    textSetColor(3,0);
    textPrintUInt(loss_count);
    textSetColor(1,0);     // Reset to normal text color
}

void print_game_winner(player_t winner) {
    const char *win_str = (winner == PLAYER_WHITE) ? "Player Wins!         " : "AI Wins!             ";
    print_formatted_text(3, 10, win_str);
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
}

void print_current_player(player_t player) {
    const char *player_str = (player == PLAYER_WHITE) ? "Player's Move        " : "AI Thinking ...      ";
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
    const char base[] = "AI Thinking";
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
}

void print_game_mode(bool is_puzzle_mode) {
    const char *mode_str = is_puzzle_mode ? "Mode: Puzzle   " : "Mode: Free Play";
    print_formatted_text(3, 15, mode_str);
}

void print_swap_rule(swap_rule_t rule) {
    const char *rule_str = "";
    switch (rule) {
        case SWAP_RULE_CLASSIC:
            rule_str = "Rule: Any - All ";
            break;
        case SWAP_RULE_CLEARS_OWN:
            rule_str = "Rule: Any - Own ";
            break;
        case SWAP_RULE_SWAPPED_CLEARS:
            rule_str = "Rule: Swap - All";
            break;
        case SWAP_RULE_SWAPPED_CLEARS_OWN:
            rule_str = "Rule: Swap - Own";
            break;
        default:
            rule_str = "Rule: Unknown   ";
            break;
    }
    print_formatted_text(3,19, rule_str);
}

void print_ai_difficulty(ai_difficulty_t difficulty) {
    const char *diff_str;
    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            diff_str = "AI: Learning";
            break;
        case AI_DIFFICULTY_EASY:
            diff_str = "AI: Easy    ";
            break;
        case AI_DIFFICULTY_STANDARD:
            diff_str = "AI: Standard";
            break;
        case AI_DIFFICULTY_EXPERT:
            diff_str = "AI: Expert  ";
            break;
        default:
            diff_str = "AI: Unknown ";
            break;
    }
    print_formatted_text(3, 17, diff_str);
}
void clear_puzzle_info() {
    print_formatted_text(3, 24, "                         ");
    print_formatted_text(3, 26, "                         ");
}

void print_puzzle_debug(const char *line1, const char *line2) {
    const char *first = line1 ? line1 : "                         ";
    const char *second = line2 ? line2 : "                         ";
    print_formatted_text(0, 40, first);
    print_formatted_text(0, 41, second);
}

void clear_puzzle_debug(void) {
    print_puzzle_debug("                         ", "                         ");
}

void print_puzzle_info(uint16_t puzzle_index, uint16_t total_puzzles, 
                       uint8_t puzzle_difficulty, bool is_solved) {
    char *buf = "";
    char checked[] = { 222, '\0'};
    const uint8_t start_row = 24;
    // Puzzle Number
    buf = "Puzzle:               ";
    print_formatted_text(3, start_row, buf);
    uint8_t index_digits = countDigits(puzzle_index + 1);
    uint8_t total_digits = countDigits(total_puzzles);
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
    buf = "Win In: ";
    print_formatted_text(3, start_row + 2, buf);
    textGotoXY(11, start_row + 2);
    textPrintUInt(puzzle_difficulty);
}

void print_swap_unavailable(void) {
    print_formatted_text(1, 40, "Clear Rule cannot be  ");
    print_formatted_text(1, 41, "changed in Puzzle Mode ");
}

void clear_swap_unavailable(void) {
    print_formatted_text(1, 40, "                         ");
    print_formatted_text(1, 41, "                         ");
}

void print_made_blunder(void) {
    print_formatted_text(3, 10, "AI Blunder!          ");
}  
void clear_made_blunder(void) {
    print_formatted_text(3, 10, "                     ");
}

void print_AI_hint(const char *hint) {
    char *buf = "Hint:                ";
    char * hint_str = hint ? (char *)hint : (char *)"N/A";
    print_formatted_text(3,10, buf);
    textGotoXY(9,10);
    textPrint(hint_str);
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


    print_formatted_text(5, start_row, "Move History");
    for (uint8_t i = 0; i < 8; ++i) {
        if (i < move_count) {
            const move_t *move = &history[i];
            char movestr[9] = "F1->T1 S";
            print_formatted_text(3, start_row + 3 + i*2, move->player == PLAYER_WHITE ? "Player: " : "AI:     ");
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

void print_mouse_position(uint16_t x, uint16_t y) {

    print_formatted_text(1, 40, "Mouse: X=    Y=   ");
    textGotoXY(8, 23);
    textPrintUInt(x);
    textGotoXY(16, 23);
    textPrintUInt(y);
}



// void display_test() {
//     CharMap char_map;
//     Checkbox checkbox1, checkbox2, checkbox3;
//     RadioGroup radio1, radio2;
//     Dropdown dropdown1, dropdown2;

//     char_map.radio_unselected = 179;      /* ○ */
//     char_map.radio_selected = 225;        /* ● */
//     char_map.checkbox_unchecked = 227;    /* ☐ */
//     char_map.checkbox_checked = 222;      /* ☑ */
//     char_map.box_down_right = 160;        /* ┌ */
//     char_map.box_down_left = 161;         /* ┐ */
//     char_map.box_horizontal = 150;        /* ─ */
//     char_map.box_vertical = 130;          /* │ */
//     char_map.box_up_right = 162;          /* └ */
//     char_map.box_up_left = 163;           /* ┘ */

//     GetCharCallback gccb = get_char_callback;
//     GetModeCallback gmcb = get_mode_callback;
//     DisplayCharCallback dccb = display_char_callback;

//     widget_init(dccb, gccb, gmcb, &char_map);

//     /* Checkbox without box - unchecked */
//     checkbox_create(&checkbox1, 2, 30, "Enable Sound", 0);
//     checkbox_draw(&checkbox1);
    
//     /* Checkbox with box - checked */
//     checkbox_create(&checkbox2, 2, 34, "Enable Music", 1);
//     checkbox_set_checked(&checkbox2, 1);
    
//     /* Checkbox with box - unchecked */
//     checkbox_create(&checkbox3, 2, 38, "Show FPS", 1);
//     checkbox_draw(&checkbox3);

//         /* Vertical radio group without box */
//     radio_create(&radio1, 2, 42, LAYOUT_VERTICAL, 0);
//     radio_add_item(&radio1, "Easy", 0, 0);
//     radio_add_item(&radio1, "Medium", 0, 1);
//     radio_add_item(&radio1, "Hard", 0, 2);
//     radio_add_item(&radio1, "Insane", 0, 3);
//     radio_set_selected(&radio1, 1);  /* Select "Medium" */
    
//     /* Horizontal radio group with box */
//     radio_create(&radio2, 2, 46, LAYOUT_HORIZONTAL, 1);
//     radio_add_item(&radio2, "1P", 0, 0);
//     radio_add_item(&radio2, "2P", 6, 0);
//     radio_add_item(&radio2, "3P", 12, 0);
//     radio_add_item(&radio2, "4P", 18, 0);
//     radio_set_selected(&radio2, 0);  /* Select "1P" */

//     dropdown_create(&dropdown1, 2, 50, 0);
//     dropdown_add_item(&dropdown1, "320x240");
//     dropdown_add_item(&dropdown1, "640x480");
//     dropdown_add_item(&dropdown1, "800x600");
//     dropdown_add_item(&dropdown1, "1024x768");
//     dropdown_set_selected(&dropdown1, 1);  /* Select "640x480" */
//     dropdown_draw(&dropdown1);
    
//     /* Dropdown with box - collapsed */
//     dropdown_create(&dropdown2, 2, 54, 1);
//     dropdown_add_item(&dropdown2, "NTSC");
//     dropdown_add_item(&dropdown2, "PAL");
//     dropdown_add_item(&dropdown2, "RGB");
//     dropdown_set_selected(&dropdown2, 0);  /* Select "NTSC" */
//     dropdown_draw(&dropdown2);   
//     getchar();
//     dropdown_expand(&dropdown1); /* Expand */
//     dropdown_draw(&dropdown1);
//     getchar();
//     dropdown_set_highlighted(&dropdown1, 2);
//     dropdown_draw(&dropdown1);
//     getchar();
//     dropdown_set_selected(&dropdown1, 2);

//     dropdown_draw(&dropdown1);
//     getchar();


// }
