#include "../src/text_display.h"
#include "../src/board.h"
#include "../src/ai_agent.h"

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

void print_current_player(player_t player) {
    const char *player_str = (player == PLAYER_WHITE) ? "Human Player's Move" : "AI Agent Thinking ...";
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

void print_puzzle_info(uint8_t puzzle_index, uint8_t total_puzzles, 
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

    // Puzzle Difficulty
    buf = "Puzzle Difficulty: ";
    print_formatted_text(0, 6, buf);
    textGotoXY(19,6);
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

void clear_puzzle_hint() {
    print_formatted_text(0,8, "");
} 

void print_move_history(const move_t *history, uint8_t move_count) {
    print_formatted_text(0, 9, "Move History");
    for (uint8_t i = 0; i < 8; ++i) {
        if (i < move_count) {
            const move_t *move = &history[move_count - i];
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

