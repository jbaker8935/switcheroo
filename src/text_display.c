#include "../src/text_display.h"
#include "../src/board.h"
#include "../src/ai_agent.h"
#include "../src/puzzle_data.h"
#include "../src/mouse_pointer.h"
// #include "../src/twidget.h"
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

// Helper function to count digits in a number
 uint16_t countDigits(uint16_t n) {
    uint16_t count = 0;
    do {
        count++;
        n /= 10;
    } while (n > 0);
    return count;
}

// Helper function to format strings to exactly 25 characters with right-padding
void print_formatted_text(uint8_t x, uint8_t y, const char *text) {
    char buf[26]; // 25 chars + null terminator
    uint8_t len = strlen(text);
    if (len > 25) len = 25; // Truncate if too long
    
    // Copy the text
    memcpy(buf, text, len);
    
    // Right-fill with spaces
    while (len < 25) {
        buf[len++] = ' ';
    }
    buf[25] = '\0';
    
    textGotoXY(x, y);
    textPrint(buf);
}

void print_win_loss(uint16_t win_count, uint16_t loss_count) {
    char * win_loss_str = "Wins:     Losses:     ";

    print_formatted_text(0, 0, win_loss_str);
    textGotoXY(6, 0);
    textPrintUInt(win_count);
    textGotoXY(18, 0);
    textPrintUInt(loss_count);
}

void print_game_winner(player_t winner) {
    const char *win_str = (winner == PLAYER_WHITE) ? "Human Player Wins!     " : "AI Agent Wins!         ";
    print_formatted_text(0, 1, win_str);
    set_mouse_cursor(MOUSE_CURSOR_NORMAL);
}

void print_current_player(player_t player) {
    const char *player_str = (player == PLAYER_WHITE) ? "Human Player's Move" : "AI Agent Thinking ...";
    if (player == PLAYER_WHITE) {
        set_mouse_cursor(MOUSE_CURSOR_NORMAL);
    } else {
        set_mouse_cursor(MOUSE_CURSOR_BUSY);
    }
    print_formatted_text(0,1, player_str);
}

void print_game_mode(bool is_puzzle_mode) {
    const char *mode_str = is_puzzle_mode ? "Game Mode: Puzzle" : "Game Mode: Free Play";
    print_formatted_text(0,2, mode_str);
}

void print_swap_rule(swap_rule_t rule) {
    const char *rule_str = "";
    switch (rule) {
        case SWAP_RULE_CLASSIC:
            rule_str = "Rule: Clears All";
            break;
        case SWAP_RULE_CLEARS_OWN:
            rule_str = "Rule: Clears Own";
            break;
        case SWAP_RULE_SWAPPED_CLEARS:
            rule_str = "Rule: Swapped Clears";
            break;
        case SWAP_RULE_SWAPPED_CLEARS_OWN:
            rule_str = "Rule: Swapped Clears Own";
            break;
        default:
            rule_str = "Rule: Unknown";
            break;
    }
    print_formatted_text(0,3, rule_str);
}

void print_ai_difficulty(ai_difficulty_t difficulty) {
    const char *diff_str = "AI Difficulty: ";
    switch (difficulty) {
        case AI_DIFFICULTY_LEARNING:
            diff_str = "AI Difficulty: Learning";
            break;
        case AI_DIFFICULTY_EASY:
            diff_str = "AI Difficulty: Easy";
            break;
        case AI_DIFFICULTY_STANDARD:
            diff_str = "AI Difficulty: Standard";
            break;
        case AI_DIFFICULTY_EXPERT:
            diff_str = "AI Difficulty: Expert";
            break;
        default:
            diff_str = "AI Difficulty: Unknown";
            break;
    }
    print_formatted_text(0, 4, diff_str);
}
void clear_puzzle_info() {
    print_formatted_text(0,5, "");
    print_formatted_text(0,6, "");
    print_formatted_text(0,7, "");
    print_formatted_text(0,8, "");
}

void print_puzzle_debug(const char *line1, const char *line2) {
    const char *first = line1 ? line1 : "";
    const char *second = line2 ? line2 : "";
    print_formatted_text(0, 20, first);
    print_formatted_text(0, 21, second);
}

void clear_puzzle_debug(void) {
    print_puzzle_debug("", "");
}

void print_puzzle_info(uint16_t puzzle_index, uint16_t total_puzzles, 
                       uint8_t puzzle_difficulty, bool is_solved) {
    char *buf = "";
    
    // Puzzle Number
    buf = "Puzzle:";
    print_formatted_text(0,5, buf);
    uint16_t index_digits = countDigits(puzzle_index + 1);
    textGotoXY(8,5);
    textPrintUInt(puzzle_index + 1);
    textGotoXY(8 + index_digits, 5);
    textPrint(" of ");
    textGotoXY(8 + index_digits + 4, 5);
    textPrintUInt(total_puzzles);

    // Win In
    buf = "Win In: ";
    print_formatted_text(0, 6, buf);
    textGotoXY(8,6);
    textPrintUInt(puzzle_difficulty);
    
    // Solve Status
    buf = is_solved ? "Status: Solved" : "Status: Unsolved";
    print_formatted_text(0,7, buf);
    
}

void print_puzzle_hint(const char *hint) {
    char *buf = "Hint: ";
    char * hint_str = hint ? (char *)hint : (char *)"No hint available";
    print_formatted_text(0,8, buf);
    textGotoXY(6,8);
    textPrint(hint_str);
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
        
        pos = pd_append_char(buf, buf_size, pos, (player == PLAYER_WHITE) ? 'A' : 'B');
        pos = pd_append_char(buf, buf_size, pos, ':');
        pos = pd_append_char(buf, buf_size, pos, ' ');
        pos = pd_append_char(buf, buf_size, pos, (char)('A' + from_col));
        pos = pd_append_char(buf, buf_size, pos, (char)('0' + from_row));
        pos = pd_append_char(buf, buf_size, pos, '-');
        pos = pd_append_char(buf, buf_size, pos, '>');
        pos = pd_append_char(buf, buf_size, pos, (char)('A' + to_col));
        pos = pd_append_char(buf, buf_size, pos, (char)('0' + to_row));
        
        if (is_swap) {
            pos = pd_append_char(buf, buf_size, pos, '(');
            pos = pd_append_char(buf, buf_size, pos, 's');
            pos = pd_append_char(buf, buf_size, pos, 'w');
            pos = pd_append_char(buf, buf_size, pos, 'a');
            pos = pd_append_char(buf, buf_size, pos, 'p');
            pos = pd_append_char(buf, buf_size, pos, ')');
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
    print_formatted_text(0,8, "");
} 

void print_move_history(const move_t *history, uint8_t move_count) {
    print_formatted_text(0, 9, "Move History");
    for (uint8_t i = 0; i < 8; ++i) {
        if (i < move_count) {
            const move_t *move = &history[i];
            char movestr[9] = "F1->T1 S";
            print_formatted_text(0, 10 + i, move->player == PLAYER_WHITE ? "Human: " : "Agent: ");
            movestr[0] = (char) ( 'A' + move->from_col);
            movestr[1] = (char) ('0' + (8 - move->from_row));
            movestr[4] = (char) ('A' + move->to_col);
            movestr[5] = (char) ('0' + (8 - move->to_row));
            movestr[7] = (char) ((move->type == MOVE_TYPE_SWAP) ? 'S' : ' ');
            movestr[8] = '\0';
            textGotoXY(7, 10 + i);
            textPrint(movestr);

        } else {
            print_formatted_text(0, 10 + i, "");
        }
    }
}

/* Display character callback - puts character at (x,y) */
void display_char_callback(unsigned char x, unsigned char y, unsigned char character, unsigned char mode) {
    char buf[2] = { character, '\0' };
    textGotoXY(x, y);
    textPrint(buf);
}

/* Gets character at (x,y) */
char get_char_callback(unsigned char x, unsigned char y) {
    char c;
    POKE(0x0001,2);
    c = PEEK(0xC000 + x * 80 + y);
    POKE(0x0001,0);
    return c;
}
/* Gets mode at (x,y) */
char get_mode_callback(unsigned char x, unsigned char y) {
    return 0 ;
}


// void display_test() {
//     CharMap char_map;
//     Checkbox checkbox1, checkbox2, checkbox3;
//     RadioGroup radio1, radio2;
//     Dropdown dropdown1, dropdown2;

//     char_map.radio_unselected = 'o';      /* ○ */
//     char_map.radio_selected = '*';        /* ● */
//     char_map.checkbox_unchecked = '[';    /* ☐ */
//     char_map.checkbox_checked = 'X';      /* ☑ */
//     char_map.box_down_right = '+';        /* ┌ */
//     char_map.box_down_left = '+';         /* ┐ */
//     char_map.box_horizontal = '-';        /* ─ */
//     char_map.box_vertical = '|';          /* │ */
//     char_map.box_up_right = '+';          /* └ */
//     char_map.box_up_left = '+';           /* ┘ */

//     widget_init(display_char_callback, get_char_callback, get_mode_callback, &char_map);

//     /* Checkbox without box - unchecked */
//     checkbox_create(&checkbox1, 2, 30, "Enable Sound", 0);
//     checkbox_draw(&checkbox1);
    
//     /* Checkbox with box - checked */
//     checkbox_create(&checkbox2, 2, 32, "Enable Music", 1);
//     checkbox_set_checked(&checkbox2, 1);
    
//     /* Checkbox with box - unchecked */
//     checkbox_create(&checkbox3, 2, 34, "Show FPS", 1);
//     checkbox_draw(&checkbox3);

//         /* Vertical radio group without box */
//     radio_create(&radio1, 2, 36, LAYOUT_VERTICAL, 0);
//     radio_add_item(&radio1, "Easy", 0, 0);
//     radio_add_item(&radio1, "Medium", 0, 1);
//     radio_add_item(&radio1, "Hard", 0, 2);
//     radio_add_item(&radio1, "Insane", 0, 3);
//     radio_set_selected(&radio1, 1);  /* Select "Medium" */
    
//     /* Horizontal radio group with box */
//     radio_create(&radio2, 2, 42, LAYOUT_HORIZONTAL, 1);
//     radio_add_item(&radio2, "1P", 0, 0);
//     radio_add_item(&radio2, "2P", 6, 0);
//     radio_add_item(&radio2, "3P", 12, 0);
//     radio_add_item(&radio2, "4P", 18, 0);
//     radio_set_selected(&radio2, 0);  /* Select "1P" */

//     dropdown_create(&dropdown1, 2, 44, 0);
//     dropdown_add_item(&dropdown1, "320x240");
//     dropdown_add_item(&dropdown1, "640x480");
//     dropdown_add_item(&dropdown1, "800x600");
//     dropdown_add_item(&dropdown1, "1024x768");
//     dropdown_set_selected(&dropdown1, 1);  /* Select "640x480" */
//     dropdown_draw(&dropdown1);
    
//     /* Dropdown with box - collapsed */
//     dropdown_create(&dropdown2, 2, 46, 1);
//     dropdown_add_item(&dropdown2, "NTSC");
//     dropdown_add_item(&dropdown2, "PAL");
//     dropdown_add_item(&dropdown2, "RGB");
//     dropdown_set_selected(&dropdown2, 0);  /* Select "NTSC" */
//     dropdown_draw(&dropdown2);   


// }
