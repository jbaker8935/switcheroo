// Auto-generated from JSON puzzle data
// Do not edit manually

#include "../src/puzzle_data.h"
#include "../src/board.h"
#include <string.h>

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

    pos = pd_append_char(buf, buf_size, pos, (player == PLAYER_WHITE) ? 'W' : 'B');
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

// Puzzle 0: classic_depth3_1
static const uint8_t puzzle_0_pieces[] = {
    7, 0x0E,
    7, 0x0F,
    6, 0x04,
    6, 0x0A,
    6, 0x0B,
    5, 0x04,
    5, 0x05,
    5, 0x0B,
    4, 0x04,
    4, 0x09,
    4, 0x0E,
    3, 0x09,
    3, 0x03,
    1, 0x00,
    1, 0x05,
    1, 0x02,
};

static const uint16_t puzzle_0_solution[] = {
    0x0, 0xA489,
    0x1, 0x848A,
    0x0, 0x8922,
    0x1, 0x6213,
    0x0, 0x651B,
};

static const puzzle_t puzzle_0 = {
    .id = "classic_depth3_1",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_0_pieces,
    .solution_length = 5,
    .solution = puzzle_0_solution
};

// Puzzle 1: classic_depth3_2
static const uint8_t puzzle_1_pieces[] = {
    6, 0x09,
    6, 0x06,
    6, 0x03,
    5, 0x0A,
    4, 0x04,
    4, 0x0D,
    4, 0x0A,
    3, 0x07,
    2, 0x04,
    2, 0x06,
    2, 0x03,
    1, 0x04,
    1, 0x02,
    1, 0x03,
    0, 0x05,
    0, 0x02,
};

static const uint16_t puzzle_1_solution[] = {
    0x0, 0xA796,
    0x1, 0x8302,
    0x0, 0x850E,
    0x1, 0x618F,
    0x0, 0x6283,
};

static const puzzle_t puzzle_1 = {
    .id = "classic_depth3_2",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_1_pieces,
    .solution_length = 5,
    .solution = puzzle_1_solution
};

// Puzzle 2: classic_depth3_3
static const uint8_t puzzle_2_pieces[] = {
    7, 0x00,
    6, 0x04,
    6, 0x06,
    6, 0x03,
    5, 0x04,
    5, 0x0A,
    4, 0x0F,
    3, 0x00,
    3, 0x05,
    3, 0x0E,
    3, 0x0B,
    2, 0x00,
    2, 0x06,
    2, 0x0F,
    1, 0x01,
    1, 0x0B,
};

static const uint16_t puzzle_2_solution[] = {
    0x0, 0xA698,
    0x1, 0x8418,
    0x0, 0x851E,
    0x1, 0x6317,
    0x0, 0x6899,
};

static const puzzle_t puzzle_2 = {
    .id = "classic_depth3_3",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_2_pieces,
    .solution_length = 5,
    .solution = puzzle_2_solution
};

// Puzzle 3: classic_depth3_4
static const uint8_t puzzle_3_pieces[] = {
    7, 0x06,
    6, 0x04,
    6, 0x02,
    5, 0x00,
    5, 0x01,
    5, 0x02,
    4, 0x04,
    3, 0x00,
    3, 0x0E,
    2, 0x0A,
    1, 0x0C,
    1, 0x09,
    1, 0x03,
    0, 0x0C,
    0, 0x05,
    0, 0x0E,
};

static const uint16_t puzzle_3_solution[] = {
    0x0, 0xA8A9,
    0x1, 0x8D3C,
    0x0, 0x8CBD,
    0x1, 0x6185,
    0x0, 0x6699,
};

static const puzzle_t puzzle_3 = {
    .id = "classic_depth3_4",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_3_pieces,
    .solution_length = 5,
    .solution = puzzle_3_solution
};

// Puzzle 4: classic_depth3_5
static const uint8_t puzzle_4_pieces[] = {
    7, 0x00,
    7, 0x05,
    6, 0x04,
    6, 0x01,
    5, 0x04,
    5, 0x03,
    4, 0x0A,
    3, 0x04,
    3, 0x09,
    3, 0x0E,
    3, 0x0B,
    2, 0x0A,
    2, 0x0F,
    1, 0x00,
    1, 0x05,
    0, 0x07,
};

static const uint16_t puzzle_4_solution[] = {
    0x0, 0xA288,
    0x1, 0x8CA8,
    0x0, 0x8EB8,
    0x1, 0x6107,
    0x0, 0x6D3B,
};

static const puzzle_t puzzle_4 = {
    .id = "classic_depth3_5",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_4_pieces,
    .solution_length = 5,
    .solution = puzzle_4_solution
};

// Puzzle 5: classic_depth3_6
static const uint8_t puzzle_5_pieces[] = {
    7, 0x06,
    6, 0x0C,
    6, 0x09,
    6, 0x0A,
    5, 0x0C,
    5, 0x09,
    4, 0x01,
    4, 0x02,
    4, 0x03,
    3, 0x04,
    3, 0x03,
    2, 0x05,
    2, 0x06,
    1, 0x05,
    1, 0x02,
    0, 0x05,
};

static const uint16_t puzzle_5_solution[] = {
    0x0, 0xA51E,
    0x1, 0x8302,
    0x0, 0x8282,
    0x1, 0x6103,
    0x0, 0x66A3,
};

static const puzzle_t puzzle_5 = {
    .id = "classic_depth3_6",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_5_pieces,
    .solution_length = 5,
    .solution = puzzle_5_solution
};

// Puzzle 6: classic_depth3_7
static const uint8_t puzzle_6_pieces[] = {
    7, 0x04,
    7, 0x07,
    6, 0x06,
    6, 0x03,
    5, 0x0D,
    5, 0x0B,
    4, 0x08,
    4, 0x0A,
    3, 0x08,
    3, 0x06,
    3, 0x0F,
    2, 0x06,
    2, 0x03,
    1, 0x01,
    1, 0x06,
    0, 0x02,
};

static const uint16_t puzzle_6_solution[] = {
    0x0, 0xA716,
    0x1, 0x8DBE,
    0x0, 0x8D3E,
    0x1, 0x618D,
    0x0, 0x6499,
};

static const puzzle_t puzzle_6 = {
    .id = "classic_depth3_7",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_6_pieces,
    .solution_length = 5,
    .solution = puzzle_6_solution
};

// Puzzle 7: classic_depth3_8
static const uint8_t puzzle_7_pieces[] = {
    7, 0x0D,
    6, 0x08,
    6, 0x09,
    6, 0x0E,
    6, 0x0B,
    5, 0x08,
    5, 0x01,
    4, 0x04,
    4, 0x05,
    3, 0x00,
    3, 0x06,
    3, 0x07,
    2, 0x01,
    2, 0x06,
    1, 0x03,
    0, 0x05,
};

static const uint16_t puzzle_7_solution[] = {
    0x0, 0xA8AA,
    0x1, 0x849C,
    0x0, 0x851C,
    0x1, 0x6103,
    0x0, 0x6699,
};

static const puzzle_t puzzle_7 = {
    .id = "classic_depth3_8",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_7_pieces,
    .solution_length = 5,
    .solution = puzzle_7_solution
};

// Puzzle 8: classic_depth3_9
static const uint8_t puzzle_8_pieces[] = {
    7, 0x05,
    7, 0x07,
    6, 0x04,
    6, 0x06,
    6, 0x03,
    5, 0x00,
    4, 0x05,
    4, 0x02,
    4, 0x07,
    3, 0x08,
    3, 0x01,
    2, 0x0C,
    2, 0x09,
    1, 0x08,
    1, 0x0A,
    0, 0x06,
};

static const uint16_t puzzle_8_solution[] = {
    0x0, 0xAD36,
    0x1, 0x8690,
    0x0, 0x88A4,
    0x1, 0x6185,
    0x0, 0x6CB5,
};

static const puzzle_t puzzle_8 = {
    .id = "classic_depth3_9",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_8_pieces,
    .solution_length = 5,
    .solution = puzzle_8_solution
};

// Puzzle 9: classic_depth3_10
static const uint8_t puzzle_9_pieces[] = {
    7, 0x00,
    6, 0x0C,
    6, 0x05,
    6, 0x02,
    5, 0x0D,
    5, 0x0A,
    4, 0x0A,
    3, 0x0E,
    3, 0x0B,
    2, 0x04,
    2, 0x05,
    2, 0x07,
    1, 0x04,
    1, 0x02,
    0, 0x02,
    0, 0x03,
};

static const uint16_t puzzle_9_solution[] = {
    0x0, 0xA58C,
    0x1, 0x8D2A,
    0x0, 0x8CB8,
    0x1, 0x608D,
    0x0, 0x6305,
};

static const puzzle_t puzzle_9 = {
    .id = "classic_depth3_10",
    .swap_rule = SWAP_RULE_CLASSIC,
    .difficulty = 3,
    .piece_count = 16,
    .pieces = puzzle_9_pieces,
    .solution_length = 5,
    .solution = puzzle_9_solution
};

static const puzzle_t *all_puzzles[] = {
    &puzzle_0,
    &puzzle_1,
    &puzzle_2,
    &puzzle_3,
    &puzzle_4,
    &puzzle_5,
    &puzzle_6,
    &puzzle_7,
    &puzzle_8,
    &puzzle_9,
};

static const puzzle_collection_t puzzle_collection = {
    .count = 10,
    .puzzles = all_puzzles
};

const puzzle_collection_t *get_puzzle_collection(void) {
    return &puzzle_collection;
}

const puzzle_t *get_puzzle_by_index(uint8_t index) {
    if (index >= puzzle_collection.count) return NULL;
    return puzzle_collection.puzzles[index];
}

swap_rule_t swap_rule_from_string(const char *str) {
    if (strcmp(str, "classic") == 0) return SWAP_RULE_CLASSIC;
    if (strcmp(str, "clears_own") == 0) return SWAP_RULE_CLEARS_OWN;
    if (strcmp(str, "swapped_clears") == 0) return SWAP_RULE_SWAPPED_CLEARS;
    if (strcmp(str, "swapped_clears_own") == 0) return SWAP_RULE_SWAPPED_CLEARS_OWN;
    return SWAP_RULE_CLASSIC; // default
}

const char *swap_rule_to_string(swap_rule_t rule) {
    switch (rule) {
        case SWAP_RULE_CLASSIC: return "CLASSIC";
        case SWAP_RULE_CLEARS_OWN: return "CLEARS_OWN";
        case SWAP_RULE_SWAPPED_CLEARS: return "SWAPPED_CLEARS";
        case SWAP_RULE_SWAPPED_CLEARS_OWN: return "SWAPPED_CLEARS_OWN";
        default: return "UNKNOWN";
    }
}

void apply_puzzle_position(board_t *board, const puzzle_t *puzzle) {
    // Clear the board first - set all cells to PIECE_NONE
    for (uint8_t row = 0; row < BOARD_ROWS; row++) {
        for (uint8_t col = 0; col < BOARD_COLS; col++) {
            board_set_piece(board, row, col, PIECE_NONE);
        }
    }

    // Apply puzzle pieces
    const uint8_t *pieces = puzzle->pieces;
    for (uint8_t p = 0; p < puzzle->piece_count; p++) {
        uint8_t idx = p * 2;
        uint8_t row = pieces[idx];
        uint8_t packed_piece = pieces[idx + 1];

        uint8_t swapped = PIECE_UNPACK_SWAPPED(packed_piece);
        uint8_t player = PIECE_UNPACK_PLAYER(packed_piece);
        uint8_t col = PIECE_UNPACK_COL(packed_piece);

        piece_type_t piece_type;
        if (player == PLAYER_WHITE) {
            piece_type = swapped ? PIECE_WHITE_SWAPPED : PIECE_WHITE_NORMAL;
        } else {
            piece_type = swapped ? PIECE_BLACK_SWAPPED : PIECE_BLACK_NORMAL;
        }

        board_set_piece(board, row, col, piece_type);
    }
}

void display_puzzle_solution(const puzzle_t *puzzle) {
    const uint16_t *solution = puzzle->solution;
    
    for (uint8_t i = 0; i < puzzle->solution_length; i++) {
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
        
        textGotoXY(0, 20 + i);
        textPrint(buf);
    }
}
