#!/usr/bin/env python3
"""Convert puzzle JSON files into a fixed-width binary catalog for F256 Switcharoo.

Usage:
    python3 convert_puzzles.py output.bin input1.json [input2.json ...]

Input files must provide a top-level "puzzles" array whose records are described
in `requirements.md`. Each puzzle is normalised and written to a binary catalog
whose layout matches the runtime deserialiser documented in `design.md`.
"""

from __future__ import annotations

import json
import struct
import sys
import time
from pathlib import Path
from typing import Any, Dict, Iterable, List, Sequence
import traceback

PUZZLE_ID_BYTES = 32
PUZZLE_MAX_PIECES = 16
PUZZLE_PIECE_BYTES = PUZZLE_MAX_PIECES * 2
PUZZLE_MAX_SOLUTION_MOVES = 9
PUZZLE_SOLUTION_WORDS = PUZZLE_MAX_SOLUTION_MOVES * 2
PUZZLE_SOLUTION_BYTES = PUZZLE_SOLUTION_WORDS * 2
PUZZLE_RECORD_BYTES = (
    PUZZLE_ID_BYTES
    + 2  # swap rule
    + 1  # difficulty
    + 1  # solved flag
    + 1  # piece count
    + PUZZLE_PIECE_BYTES
    + 1  # solution length
    + PUZZLE_SOLUTION_BYTES
)

PuzzleDict = Dict[str, Any]


def col_to_index(col: str) -> int:
    """Convert a column letter (A-D) to zero-based index."""

    if len(col) != 1 or col.upper() not in {"A", "B", "C", "D"}:
        raise ValueError(f"Column must be A-D, got '{col}'")
    return ord(col.upper()) - ord("A")


def player_to_index(player: str) -> int:
    """Convert a player designator (A/B) to zero-based index."""

    if player.upper() not in {"A", "B"}:
        raise ValueError(f"Player must be 'A' or 'B', got '{player}'")
    return 0 if player.upper() == "A" else 1


def json_row_to_internal(json_row: int) -> int:
    """Convert JSON row (1=bottom, 8=top) to internal index (0=top)."""

    if json_row < 1 or json_row > 8:
        raise ValueError(f"Row must be between 1 and 8, got '{json_row}'")
    return 8 - json_row


def pack_piece(player: int, col: int, swapped: bool) -> int:
    """Pack piece flags into a single byte."""

    return (int(swapped) << 3) | (player << 2) | col


def pack_position(row: int, col: int) -> int:
    """Pack a board position into five bits (row:3, col:2)."""

    return (row << 2) | col


def pack_swap_move(
    from_row: int, from_col: int, to_row: int, to_col: int, priority: int
) -> int:
    """Pack a swap move into a 16-bit word."""

    to_pos = pack_position(to_row, to_col)
    from_pos = pack_position(from_row, from_col)
    return (priority << 13) | (to_pos << 7) | (from_pos << 1)


def pack_empty_move(
    from_row: int, from_col: int, to_row: int, to_col: int, priority: int
) -> int:
    """Pack an empty move into a 16-bit word with the type bit set."""

    return pack_swap_move(from_row, from_col, to_row, to_col, priority) | 1


def swap_rule_to_value(swap_rule: str) -> int:
    """Translate the swap rule string into the ordinal stored in the catalog."""

    mapping = {
        "classic": 0,
        "clears_own": 1,
        "swapped_clears": 2,
        "swapped_clears_own": 3,
    }
    normalised = swap_rule.lower()
    if normalised not in mapping:
        raise ValueError(f"Unsupported swapRule '{swap_rule}'")
    return mapping[normalised]


def load_puzzle_documents(paths: Sequence[Path]) -> List[PuzzleDict]:
    """Load and concatenate puzzle entries from the provided JSON files."""

    puzzles: List[PuzzleDict] = []
    for path in paths:
        with path.open("r", encoding="utf-8") as handle:
            data = json.load(handle)

        file_puzzles = data.get("puzzles", [])
        if not isinstance(file_puzzles, list):
            raise ValueError(f"Expected 'puzzles' array in {path}")

        puzzles.extend(file_puzzles)

    return puzzles


def serialise_puzzle(puzzle: PuzzleDict) -> bytes:
    """Serialise a single puzzle dictionary into the fixed-width record."""

    record = bytearray(PUZZLE_RECORD_BYTES)

    puzzle_id = puzzle.get("id", "")
    if not isinstance(puzzle_id, str):
        raise ValueError("Puzzle id must be a string")
    encoded_id = puzzle_id.encode("ascii")
    if len(encoded_id) >= PUZZLE_ID_BYTES:
        raise ValueError(
            f"Puzzle id '{puzzle_id}' exceeds {PUZZLE_ID_BYTES - 1} characters"
        )
    record[0 : len(encoded_id)] = encoded_id

    swap_rule = swap_rule_to_value(puzzle.get("swapRule", "classic"))
    struct.pack_into("<H", record, PUZZLE_ID_BYTES, swap_rule)

    difficulty = int(puzzle.get("difficulty", 1))
    if difficulty < 1 or difficulty > 4:
        raise ValueError(
            f"Unsupported difficulty '{difficulty}' in puzzle '{puzzle_id}'"
        )
    record[PUZZLE_ID_BYTES + 2] = difficulty & 0xFF

    is_solved = 1 if puzzle.get("isSolved", False) else 0
    record[PUZZLE_ID_BYTES + 3] = is_solved

    starting_position = puzzle.get("startingPosition", [])
    if not isinstance(starting_position, list):
        raise ValueError(
            f"startingPosition must be a list in puzzle '{puzzle_id}'"
        )
    if len(starting_position) > PUZZLE_MAX_PIECES:
        raise ValueError(
            f"Puzzle '{puzzle_id}' has {len(starting_position)} pieces; "
            f"max is {PUZZLE_MAX_PIECES}"
        )
    record[PUZZLE_ID_BYTES + 4] = len(starting_position)

    pieces_offset = PUZZLE_ID_BYTES + 5
    pieces_buffer = memoryview(record)[
        pieces_offset : pieces_offset + PUZZLE_PIECE_BYTES
    ]
    for index, piece in enumerate(starting_position):
        try:
            internal_row = json_row_to_internal(int(piece["row"]))
            col_idx = col_to_index(piece["col"])
            player_idx = player_to_index(piece["player"])
        except KeyError as exc:
            raise ValueError(
                f"Missing '{exc.args[0]}' in startingPosition for "
                f"puzzle '{puzzle_id}'"
            ) from exc

        swapped = bool(piece.get("swapped", False))
        base = index * 2
        pieces_buffer[base] = internal_row & 0xFF
        pieces_buffer[base + 1] = pack_piece(player_idx, col_idx, swapped) & 0xFF

    solution = puzzle.get("exampleSolution", [])
    if not isinstance(solution, list):
        raise ValueError(
            f"exampleSolution must be a list in puzzle '{puzzle_id}'"
        )
    if len(solution) > PUZZLE_MAX_SOLUTION_MOVES:
        raise ValueError(
            f"Puzzle '{puzzle_id}' has solution length {len(solution)}; "
            f"max is {PUZZLE_MAX_SOLUTION_MOVES}"
        )

    solution_length_offset = pieces_offset + PUZZLE_PIECE_BYTES
    record[solution_length_offset] = len(solution)

    solution_offset = solution_length_offset + 1
    for move_index, move_data in enumerate(solution):
        try:
            player_idx = player_to_index(move_data["player"])
            move = move_data["move"]
        except KeyError as exc:
            raise ValueError(
                f"Missing '{exc.args[0]}' in exampleSolution for "
                f"puzzle '{puzzle_id}'"
            ) from exc
        if not isinstance(move, dict):
            raise ValueError(
                f"Move entry must be an object in puzzle '{puzzle_id}'"
            )

        try:
            move_type = move["type"].lower()
        except AttributeError as exc:
            raise ValueError(
                f"Move type must be a string in puzzle '{puzzle_id}'"
            ) from exc
        except KeyError as exc:
            raise ValueError(
                f"Missing '{exc.args[0]}' in move definition for "
                f"puzzle '{puzzle_id}'"
            ) from exc

        priority = int(move.get("priority", 0))
        if priority < 0 or priority > 7:
            raise ValueError(
                f"Invalid priority '{priority}' in puzzle '{puzzle_id}'"
            )

        def parse_endpoint(label: str) -> tuple[int, int]:
            try:
                endpoint = move[label]
            except KeyError as exc:
                raise ValueError(
                    f"Missing '{label}' endpoint in puzzle '{puzzle_id}'"
                ) from exc

            if not isinstance(endpoint, dict):
                raise ValueError(
                    f"Endpoint '{label}' must be an object in puzzle "
                    f"'{puzzle_id}'"
                )
            try:
                row = json_row_to_internal(int(endpoint["row"]))
                col = col_to_index(endpoint["col"])
            except KeyError as exc:
                raise ValueError(
                    f"Missing '{exc.args[0]}' in move endpoint '{label}' for "
                    f"puzzle '{puzzle_id}'"
                ) from exc
            return row, col

        from_row, from_col = parse_endpoint("from")
        to_row, to_col = parse_endpoint("to")

        if move_type == "swap":
            packed_move = pack_swap_move(
                from_row, from_col, to_row, to_col, priority
            )
        elif move_type == "empty":
            packed_move = pack_empty_move(
                from_row, from_col, to_row, to_col, priority
            )
        else:
            raise ValueError(
                f"Unknown move type '{move_type}' in puzzle '{puzzle_id}'"
            )

        struct.pack_into(
            "<H", record, solution_offset + (move_index * 4), player_idx & 0xFFFF
        )
        struct.pack_into(
            "<H",
            record,
            solution_offset + (move_index * 4) + 2,
            packed_move & 0xFFFF,
        )

    return bytes(record)

def puzzle_to_string(puzzle: PuzzleDict) -> str:
    """Output single puzzle dictionary into text board representation."""

    board = [['..'] * 4 for _ in range(8)]  # 8 rows x 4 cols

    puzzle_id = puzzle.get("id", "")
    if not isinstance(puzzle_id, str):
        raise ValueError("Puzzle id must be a string")
    encoded_id = puzzle_id.encode("ascii")
    if len(encoded_id) >= PUZZLE_ID_BYTES:
        raise ValueError(
            f"Puzzle id '{puzzle_id}' exceeds {PUZZLE_ID_BYTES - 1} characters"
        )


    starting_position = puzzle.get("startingPosition", [])
    if not isinstance(starting_position, list):
        raise ValueError(
            f"startingPosition must be a list in puzzle '{puzzle_id}'"
        )
    if len(starting_position) > PUZZLE_MAX_PIECES:
        raise ValueError(
            f"Puzzle '{puzzle_id}' has {len(starting_position)} pieces; "
            f"max is {PUZZLE_MAX_PIECES}"
        )

    for index, piece in enumerate(starting_position):
        try:
            row_idx = int(piece["row"]) - 1
            col_idx = col_to_index(piece["col"])
            player = piece["player"]
        except KeyError as exc:
            raise ValueError(
                f"Missing '{exc.args[0]}' in startingPosition for "
                f"puzzle '{puzzle_id}'"
            ) from exc

        swapped = bool(piece.get("swapped", False))
        board[row_idx][col_idx] = f"{player}{'s' if swapped else ' '}"
    board_str = "\n".join("|".join(row) for row in reversed(board))
    return board_str

def serialise_catalog(puzzles: Iterable[PuzzleDict]) -> bytes:
    """Serialise all puzzles into a binary blob with a puzzle-count header."""

    puzzles_list = list(puzzles)
    if len(puzzles_list) > 0xFFFF:
        raise ValueError("Puzzle catalog exceeds 65535 entries")

    signature = int(time.time()) & 0xFFFFFFFFFFFFFFFF

    output = bytearray()
    output.extend(struct.pack("<Q", signature))
    output.extend(struct.pack("<H", len(puzzles_list)))
    for puzzle in puzzles_list:
        output.extend(serialise_puzzle(puzzle))

    padding = (-len(output)) % 4
    if padding:
        output.extend(b"\x00" * padding)

    return bytes(output)


def main() -> None:
    """Parse arguments, serialise puzzles, and emit the binary catalog."""

    if len(sys.argv) < 3:

        print(
            "Usage: python3 convert_puzzles.py output.bin "
            "input1.json [input2.json ...]"
        )
        sys.exit(1)

    output_path = Path(sys.argv[1])
    input_paths = [Path(arg) for arg in sys.argv[2:]]

    try:
        puzzles = load_puzzle_documents(input_paths)
        if not puzzles:
            raise ValueError("No puzzles found across provided JSON files")

        catalog = serialise_catalog(puzzles)

        output_path.parent.mkdir(parents=True, exist_ok=True)
        with output_path.open("wb") as handle:
            handle.write(catalog)

        print(
            f"Wrote {len(puzzles)} puzzles (catalog size {len(catalog)} bytes) "
            f"to {output_path}"
        )

        with output_path.with_suffix(".txt").open("w", encoding="utf-8") as text_handle:
            for puzzle in puzzles:
                board_str = puzzle_to_string(puzzle)
                text_handle.write(f"Puzzle ID: {puzzle.get('id', '')}\n")
                text_handle.write(board_str)
                text_handle.write("\n\n")

    except FileNotFoundError as exc:
        print(f"Input file not found: {exc}")
        sys.exit(1)
    except json.JSONDecodeError as exc:
        print(f"Invalid JSON: {exc}")
        sys.exit(1)
    except Exception as exc:  # pragma: no cover - CLI entry point
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()