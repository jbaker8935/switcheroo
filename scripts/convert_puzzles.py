#!/usr/bin/env python3
"""
Convert swap puzzle JSON data to C file format for F256 Switcharoo.

This script reads a JSON file containing puzzle data and generates the
corresponding C source file with compact binary representations suitable
for embedded systems.

Usage:
    python3 convert_puzzles.py output.c input1.json [input2.json ...]

JSON Format:
{
  "puzzles": [
    {
      "id": "puzzle_id",
      "swapRule": "classic|clears_own|swapped_clears|swapped_clears_own",
      "difficulty": 1-4,
      "startingPosition": [
        {"row": 1-8, "col": "A-D", "player": "A|B", "swapped": false|true}
      ],
      "exampleSolution": [
        {
          "player": "A|B",
          "move": {
            "from": {"row": 1-8, "col": "A-D"},
            "to": {"row": 1-8, "col": "A-D"},
            "type": "swap|empty",
            "priority": 0-7
          }
        }
      ]
    }
  ]
}
"""

import json
import sys
from typing import Dict, List, Any


def col_to_index(col: str) -> int:
    """Convert column letter A-D to index 0-3."""
    return ord(col.upper()) - ord('A')


def player_to_index(player: str) -> int:
    """Convert player A/B to index 0/1."""
    return 0 if player.upper() == 'A' else 1


def json_row_to_internal(json_row: int) -> int:
    """Convert JSON row (1-8, 1=bottom) to internal row (0-7, 0=top)."""
    return 8 - json_row


def pack_piece(player: int, col: int, swapped: bool) -> int:
    """Pack piece data: [swapped:1][player:1][col:2]"""
    return (int(swapped) << 3) | (player << 2) | col


def pack_position(row: int, col: int) -> int:
    """Pack position: [row:3][col:2]"""
    return (row << 2) | col


def pack_swap_move(from_row: int, from_col: int, to_row: int, to_col: int, priority: int) -> int:
    """Pack swap move: [priority:3][to_pos:6][from_pos:6][type:1] where type=0 for swap"""
    to_pos = pack_position(to_row, to_col)
    from_pos = pack_position(from_row, from_col)
    return (priority << 13) | (to_pos << 7) | (from_pos << 1) | 0


def pack_empty_move(from_row: int, from_col: int, to_row: int, to_col: int, priority: int) -> int:
    """Pack empty move: store both from and to positions but set type=1 (empty)
    Encoding: [priority:3][to_pos:6][from_pos:6][type:1] where type=1 for empty moves
    """
    to_pos = pack_position(to_row, to_col)
    from_pos = pack_position(from_row, from_col)
    return (priority << 13) | (to_pos << 7) | (from_pos << 1) | 1


def swap_rule_to_enum(swap_rule: str) -> str:
    """Convert swap rule string to enum value."""
    mapping = {
        "classic": "SWAP_RULE_CLASSIC",
        "clears_own": "SWAP_RULE_CLEARS_OWN",
        "swapped_clears": "SWAP_RULE_SWAPPED_CLEARS",
        "swapped_clears_own": "SWAP_RULE_SWAPPED_CLEARS_OWN"
    }
    return mapping.get(swap_rule, "SWAP_RULE_CLASSIC")


def generate_puzzle_c_code(puzzles: List[Dict[str, Any]]) -> str:
    """Generate C code for all puzzles."""

    lines = []
    lines.append("// Auto-generated from JSON puzzle data")
    lines.append("// Do not edit manually")
    lines.append("")
    lines.append("#include \"../src/puzzle_data.h\"")
    lines.append("#include \"../src/board.h\"")
    lines.append("#include <string.h>")
    lines.append("")

    # Generate individual puzzle data
    puzzle_pointers = []

    for i, puzzle in enumerate(puzzles):
        puzzle_id = puzzle["id"]
        swap_rule = swap_rule_to_enum(puzzle["swapRule"])
        difficulty = puzzle["difficulty"]

        # Generate pieces array
        pieces = []
        for piece in puzzle["startingPosition"]:
            internal_row = json_row_to_internal(piece["row"])
            col_idx = col_to_index(piece["col"])
            player_idx = player_to_index(piece["player"])
            swapped = piece["swapped"]

            packed_piece = pack_piece(player_idx, col_idx, swapped)
            pieces.extend([internal_row, packed_piece])

        lines.append(f"// Puzzle {i}: {puzzle_id}")
        lines.append(f"static const uint8_t puzzle_{i}_pieces[] = {{")
        for j in range(0, len(pieces), 2):
            row = pieces[j]
            packed = pieces[j + 1]
            lines.append(f"    {row}, 0x{packed:02X},")
        lines.append("};")
        lines.append("")

        # Generate solution array
        solution_moves = []
        for move_data in puzzle["exampleSolution"]:
            player_idx = player_to_index(move_data["player"])
            move = move_data["move"]

            if move["type"] == "swap":
                from_row = json_row_to_internal(move["from"]["row"])
                from_col = col_to_index(move["from"]["col"])
                to_row = json_row_to_internal(move["to"]["row"])
                to_col = col_to_index(move["to"]["col"])
                priority = int(move["priority"])  # Convert float to int if needed

                packed_move = pack_swap_move(from_row, from_col, to_row, to_col, priority)
            elif move["type"] == "empty":
                # For empty moves we preserve both from and to positions so the UI
                # can display the full from->to move. Pack both positions and set type=1.
                from_row = json_row_to_internal(move["from"]["row"])
                from_col = col_to_index(move["from"]["col"])
                to_row = json_row_to_internal(move["to"]["row"])
                to_col = col_to_index(move["to"]["col"])
                priority = int(move["priority"])  # Convert float to int if needed

                packed_move = pack_empty_move(from_row, from_col, to_row, to_col, priority)
            else:
                raise ValueError(f"Unknown move type: {move['type']}")

            solution_moves.extend([player_idx, packed_move])

        lines.append(f"static const uint16_t puzzle_{i}_solution[] = {{")
        for j in range(0, len(solution_moves), 2):
            player = solution_moves[j]
            packed = solution_moves[j + 1]
            lines.append(f"    0x{player:X}, 0x{packed:04X},")
        lines.append("};")
        lines.append("")

        # Generate puzzle struct
        lines.append(f"static const puzzle_t puzzle_{i} = {{")
        lines.append(f"    .id = \"{puzzle_id}\",")
        lines.append(f"    .swap_rule = {swap_rule},")
        lines.append(f"    .difficulty = {difficulty},")
        lines.append(f"    .is_solved = false,")  # Default to false; can be updated in-game
        # piece_count is the number of pieces (not the number of bytes in the array)
        lines.append(f"    .piece_count = {len(puzzle['startingPosition'])},")
        lines.append(f"    .pieces = puzzle_{i}_pieces,")
        # solution_length is the number of moves; the solution array stores pairs [player, packed_move]
        lines.append(f"    .solution_length = {len(puzzle['exampleSolution'])},")
        lines.append(f"    .solution = puzzle_{i}_solution")
        lines.append("};")
        lines.append("")

        puzzle_pointers.append(f"    &puzzle_{i}")

    # Generate puzzle collection
    lines.append("static const puzzle_t *all_puzzles[] = {")
    for ptr in puzzle_pointers:
        lines.append(f"{ptr},")
    lines.append("};")
    lines.append("")

    lines.append("static const puzzle_collection_t puzzle_collection = {")
    lines.append(f"    .count = {len(puzzles)},")
    lines.append("    .puzzles = all_puzzles")
    lines.append("};")
    lines.append("")

    # Generate functions
    lines.append("const puzzle_collection_t *get_puzzle_collection(void) {")
    lines.append("    return &puzzle_collection;")
    lines.append("}")
    lines.append("")

    lines.append("const puzzle_t *get_puzzle_by_index(uint8_t index) {")
    lines.append("    if (index >= puzzle_collection.count) return NULL;")
    lines.append("    return puzzle_collection.puzzles[index];")
    lines.append("}")
    lines.append("")

    lines.append("swap_rule_t swap_rule_from_string(const char *str) {")
    lines.append("    if (strcmp(str, \"classic\") == 0) return SWAP_RULE_CLASSIC;")
    lines.append("    if (strcmp(str, \"clears_own\") == 0) return SWAP_RULE_CLEARS_OWN;")
    lines.append("    if (strcmp(str, \"swapped_clears\") == 0) return SWAP_RULE_SWAPPED_CLEARS;")
    lines.append("    if (strcmp(str, \"swapped_clears_own\") == 0) return SWAP_RULE_SWAPPED_CLEARS_OWN;")
    lines.append("    return SWAP_RULE_CLASSIC; // default")
    lines.append("}")
    lines.append("")

    lines.append("const char *swap_rule_to_string(swap_rule_t rule) {")
    lines.append("    switch (rule) {")
    lines.append("        case SWAP_RULE_CLASSIC: return \"CLASSIC\";")
    lines.append("        case SWAP_RULE_CLEARS_OWN: return \"CLEARS_OWN\";")
    lines.append("        case SWAP_RULE_SWAPPED_CLEARS: return \"SWAPPED_CLEARS\";")
    lines.append("        case SWAP_RULE_SWAPPED_CLEARS_OWN: return \"SWAPPED_CLEARS_OWN\";")
    lines.append("        default: return \"UNKNOWN\";")
    lines.append("    }")
    lines.append("}")
    lines.append("")

    # Generate apply_puzzle_position function
    lines.append("void apply_puzzle_position(board_t *board, const puzzle_t *puzzle) {")
    lines.append("    // Clear the board first - set all cells to PIECE_NONE")
    lines.append("    for (uint8_t row = 0; row < BOARD_ROWS; row++) {")
    lines.append("        for (uint8_t col = 0; col < BOARD_COLS; col++) {")
    lines.append("            board_set_piece(board, row, col, PIECE_NONE);")
    lines.append("        }")
    lines.append("    }")
    lines.append("")
    lines.append("    // Apply puzzle pieces")
    lines.append("    const uint8_t *pieces = puzzle->pieces;")
    lines.append("    for (uint8_t p = 0; p < puzzle->piece_count; p++) {")
    lines.append("        uint8_t idx = p * 2;")
    lines.append("        uint8_t row = pieces[idx];")
    lines.append("        uint8_t packed_piece = pieces[idx + 1];")
    lines.append("")
    lines.append("        uint8_t swapped = PIECE_UNPACK_SWAPPED(packed_piece);")
    lines.append("        uint8_t player = PIECE_UNPACK_PLAYER(packed_piece);")
    lines.append("        uint8_t col = PIECE_UNPACK_COL(packed_piece);")
    lines.append("")
    lines.append("        piece_type_t piece_type;")
    lines.append("        if (player == PLAYER_WHITE) {")
    lines.append("            piece_type = swapped ? PIECE_WHITE_SWAPPED : PIECE_WHITE_NORMAL;")
    lines.append("        } else {")
    lines.append("            piece_type = swapped ? PIECE_BLACK_SWAPPED : PIECE_BLACK_NORMAL;")
    lines.append("        }")
    lines.append("")
    lines.append("        board_set_piece(board, row, col, piece_type);")
    lines.append("    }")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 convert_puzzles.py output.c input1.json [input2.json ...]")
        sys.exit(1)

    output_file = sys.argv[1]
    input_files = sys.argv[2:]

    puzzles = []
    for input_file in input_files:
        try:
            with open(input_file, 'r') as f:
                data = json.load(f)
            puzzles.extend(data.get('puzzles', []))
        except FileNotFoundError:
            print(f"Input file not found: {input_file}")
            sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"Invalid JSON in {input_file}: {e}")
            sys.exit(1)
        except Exception as e:
            print(f"Error reading {input_file}: {e}")
            sys.exit(1)

    if not puzzles:
        print("No puzzles found in any JSON file")
        sys.exit(1)

    print(f"Converting {len(puzzles)} puzzles from {len(input_files)} files...")

    c_code = generate_puzzle_c_code(puzzles)

    try:
        with open(output_file, 'w') as f:
            f.write(c_code)
            f.write('\n')  # Ensure file ends with newline for build system compatibility
        print(f"Generated C code written to {output_file}")
    except Exception as e:
        print(f"Error writing to {output_file}: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()