#!/usr/bin/env python3
"""Generate placeholder assets for Switcharoo.

The script emits palette-indexed binary blobs sized for the Foenix F256 video
pipeline so the rendering stack has concrete data prior to final art delivery.
"""

from __future__ import annotations

import argparse
import math
from collections import OrderedDict
from pathlib import Path


# Palette slot indexes aligned with `video.c` definitions.
COLOR_TRANSPARENT = 0
COLOR_BACKGROUND = 1
COLOR_BOARD_BORDER = 2
COLOR_UI_PANEL = 3
COLOR_TEXT_PRIMARY = 4
COLOR_BOARD_BASE = 5
BOARD_COLUMNS = 4
BOARD_ROWS = 8
BOARD_CELL_COUNT = BOARD_COLUMNS * BOARD_ROWS
COLOR_HIGHLIGHT_PRIMARY = COLOR_BOARD_BASE + BOARD_CELL_COUNT
COLOR_HIGHLIGHT_SECONDARY = COLOR_HIGHLIGHT_PRIMARY + 1
COLOR_HIGHLIGHT_DISABLED = COLOR_HIGHLIGHT_PRIMARY + 2

BOARD_KEY = "board_bitmap.bin"

ASSET_DEFINITIONS = OrderedDict([
    (BOARD_KEY, "board"),
    ("sprite_white_normal.bin", ("piece", COLOR_BOARD_BASE, COLOR_BOARD_BORDER, COLOR_TEXT_PRIMARY)),
    ("sprite_white_swapped.bin", ("piece", COLOR_HIGHLIGHT_PRIMARY, COLOR_BOARD_BORDER, COLOR_TEXT_PRIMARY)),
    ("sprite_black_normal.bin", ("piece", COLOR_BOARD_BASE + 1, COLOR_BOARD_BORDER, COLOR_TEXT_PRIMARY)),
    ("sprite_black_swapped.bin", ("piece", COLOR_HIGHLIGHT_SECONDARY, COLOR_BOARD_BORDER, COLOR_TEXT_PRIMARY)),
    ("sprite_move_indicator.bin", "move_indicator"),
    ("sprite_highlight.bin", "highlight"),
    ("icon_reset.bin", "icon_reset"),
    ("icon_info.bin", "icon_info"),
    ("icon_difficulty.bin", "icon_difficulty"),
    ("icon_starting_board.bin", "icon_starting_board"),
    ("icon_history.bin", "icon_history"),
    ("icon_exit.bin", "icon_exit"),
])

PIECE_KEYS = [
    "sprite_white_normal.bin",
    "sprite_white_swapped.bin",
    "sprite_black_normal.bin",
    "sprite_black_swapped.bin",
]

ICON_KEYS = [
    "icon_reset.bin",
    "icon_info.bin",
    "icon_difficulty.bin",
    "icon_starting_board.bin",
    "icon_history.bin",
    "icon_exit.bin",
]

MOVE_INDICATOR_KEY = "sprite_move_indicator.bin"


def ensure_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def write_binary(path: Path, data: bytes) -> None:
    ensure_directory(path.parent)
    path.write_bytes(data)


def to_symbol_name(stem: str) -> str:
    sanitized = stem.replace("-", "_")
    return f"g_{sanitized}"


def format_c_array(name: str, data: bytes, values_per_line: int = 12) -> list[str]:
    lines: list[str] = [f"static const uint8_t {name}[{len(data)}] = {{"]
    for index in range(0, len(data), values_per_line):
        chunk = ", ".join(f"0x{byte:02X}" for byte in data[index:index + values_per_line])
        if index + values_per_line >= len(data):
            lines.append(f"    {chunk}")
        else:
            lines.append(f"    {chunk},")
    lines.append("};")
    lines.append("")
    return lines


def generate_board_bitmap() -> bytes:
    width, height = 320, 240
    board_cols, board_rows = BOARD_COLUMNS, BOARD_ROWS
    cell_size = 28

    board_width = board_cols * cell_size
    board_height = board_rows * cell_size
    board_x = (width - board_width) // 2
    board_y = (height - board_height) // 2

    ui_panel_x = board_x + board_width + 16

    buffer = bytearray(width * height)

    for y in range(height):
        for x in range(width):
            idx = y * width + x

            if x >= ui_panel_x:
                buffer[idx] = COLOR_UI_PANEL
                continue

            if y < board_y - 4 or y >= board_y + board_height + 4:
                buffer[idx] = COLOR_BACKGROUND
                continue

            if x < board_x - 4 or x >= board_x + board_width + 4:
                buffer[idx] = COLOR_BACKGROUND
                continue

            if (board_x - 1) <= x < (board_x + board_width + 1) and (
                (y == board_y - 1) or (y == board_y + board_height)
            ):
                buffer[idx] = COLOR_BOARD_BORDER
                continue

            if (board_y - 1) <= y < (board_y + board_height + 1) and (
                (x == board_x - 1) or (x == board_x + board_width)
            ):
                buffer[idx] = COLOR_BOARD_BORDER
                continue

            if board_x <= x < board_x + board_width and board_y <= y < board_y + board_height:
                cell_x = (x - board_x) // cell_size
                cell_y = (y - board_y) // cell_size
                palette_index = COLOR_BOARD_BASE + cell_y * board_cols + cell_x
                buffer[idx] = palette_index
                continue

            buffer[idx] = COLOR_BACKGROUND

    return bytes(buffer)


def generate_piece_sprite(fill_color: int, border_color: int, accent_color: int) -> bytes:
    size = 24
    center = (size - 1) / 2.0
    radius = 10.5
    edge_band = 1.2

    buffer = bytearray([COLOR_TRANSPARENT] * size * size)

    for y in range(size):
        for x in range(size):
            dx = x - center
            dy = y - center
            dist = math.sqrt(dx * dx + dy * dy)
            idx = y * size + x

            if dist <= radius - edge_band:
                buffer[idx] = fill_color
            elif dist <= radius:
                buffer[idx] = border_color
            elif dist <= radius + 0.8:
                buffer[idx] = accent_color

    return bytes(buffer)


def generate_highlight_sprite() -> bytes:
    size = 28
    thickness = 2
    buffer = bytearray([COLOR_TRANSPARENT] * size * size)

    for y in range(size):
        for x in range(size):
            idx = y * size + x
            on_border = (
                x < thickness
                or y < thickness
                or x >= size - thickness
                or y >= size - thickness
            )
            if on_border:
                buffer[idx] = COLOR_HIGHLIGHT_PRIMARY

    return bytes(buffer)


def draw_icon_border(buffer: bytearray, size: int) -> None:
    for x in range(size):
        buffer[x] = COLOR_BOARD_BORDER
        buffer[(size - 1) * size + x] = COLOR_BOARD_BORDER
    for y in range(size):
        buffer[y * size] = COLOR_BOARD_BORDER
        buffer[y * size + (size - 1)] = COLOR_BOARD_BORDER


def generate_icon_reset() -> bytes:
    size = 16
    buffer = bytearray([COLOR_UI_PANEL] * size * size)
    draw_icon_border(buffer, size)

    center = (size - 1) / 2.0
    radius = 5.5

    for y in range(size):
        for x in range(size):
            dx = x - center
            dy = y - center
            dist = math.sqrt(dx * dx + dy * dy)
            idx = y * size + x
            if 4.5 <= dist <= radius:
                buffer[idx] = COLOR_HIGHLIGHT_PRIMARY

    # Arrow head
    for offset in range(3):
        buffer[5 * size + 9 + offset] = COLOR_HIGHLIGHT_PRIMARY
        buffer[(5 + offset) * size + 11] = COLOR_HIGHLIGHT_PRIMARY

    return bytes(buffer)


def generate_icon_info() -> bytes:
    size = 16
    buffer = bytearray([COLOR_UI_PANEL] * size * size)
    draw_icon_border(buffer, size)

    # Outer circle
    center = (size - 1) / 2.0
    for y in range(size):
        for x in range(size):
            dist = math.sqrt((x - center) ** 2 + (y - center) ** 2)
            if 5.0 <= dist <= 6.0:
                buffer[y * size + x] = COLOR_HIGHLIGHT_SECONDARY

    # Dot and stem
    buffer[4 * size + 7] = COLOR_TEXT_PRIMARY
    buffer[5 * size + 7] = COLOR_TEXT_PRIMARY
    for y in range(6, 12):
        buffer[y * size + 7] = COLOR_TEXT_PRIMARY

    return bytes(buffer)


def generate_icon_difficulty() -> bytes:
    size = 16
    buffer = bytearray([COLOR_UI_PANEL] * size * size)
    draw_icon_border(buffer, size)

    heights = [4, 7, 10]
    base_x = 4

    for i, height in enumerate(heights):
        color = COLOR_HIGHLIGHT_PRIMARY if i == len(heights) - 1 else COLOR_HIGHLIGHT_SECONDARY
        for y in range(size - 2, size - 2 - height, -1):
            for x in range(base_x + i * 3, base_x + i * 3 + 2):
                buffer[y * size + x] = color

    return bytes(buffer)


def generate_icon_starting_board() -> bytes:
    size = 16
    buffer = bytearray([COLOR_UI_PANEL] * size * size)
    draw_icon_border(buffer, size)

    origin_x = 3
    origin_y = 3
    cell = 3

    base_light = COLOR_BOARD_BASE
    base_dark = COLOR_BOARD_BASE + 1

    for row in range(4):
        for col in range(4):
            color = base_light if (row + col) % 2 == 0 else base_dark
            for y in range(origin_y + row * cell, origin_y + row * cell + cell):
                for x in range(origin_x + col * cell, origin_x + col * cell + cell):
                    buffer[y * size + x] = color

    return bytes(buffer)


def generate_icon_history() -> bytes:
    size = 16
    buffer = bytearray([COLOR_UI_PANEL] * size * size)
    draw_icon_border(buffer, size)

    for y in range(3, 13):
        buffer[y * size + 4] = COLOR_TEXT_PRIMARY
        buffer[y * size + 11] = COLOR_TEXT_PRIMARY

    for x in range(4, 12):
        buffer[8 * size + x] = COLOR_TEXT_PRIMARY

    # Arrow to indicate scrolling
    buffer[5 * size + 12] = COLOR_HIGHLIGHT_SECONDARY
    buffer[6 * size + 11] = COLOR_HIGHLIGHT_SECONDARY
    buffer[7 * size + 10] = COLOR_HIGHLIGHT_SECONDARY

    return bytes(buffer)


def generate_icon_exit() -> bytes:
    size = 16
    buffer = bytearray([COLOR_UI_PANEL] * size * size)
    draw_icon_border(buffer, size)

    for i in range(3, 13):
        buffer[i * size + i] = COLOR_HIGHLIGHT_PRIMARY
        buffer[(15 - i) * size + i] = COLOR_HIGHLIGHT_PRIMARY

    return bytes(buffer)


def generate_move_indicator() -> bytes:
    size = 16
    radius = 5.0
    buffer = bytearray([COLOR_TRANSPARENT] * size * size)

    for y in range(size):
        for x in range(size):
            dx = x - (size - 1) / 2.0
            dy = y - (size - 1) / 2.0
            dist = math.sqrt(dx * dx + dy * dy)
            if dist <= radius:
                buffer[y * size + x] = COLOR_HIGHLIGHT_PRIMARY

    return bytes(buffer)


GENERATOR_TABLE = {
    "board": generate_board_bitmap,
    "highlight": generate_highlight_sprite,
    "move_indicator": generate_move_indicator,
    "icon_reset": generate_icon_reset,
    "icon_info": generate_icon_info,
    "icon_difficulty": generate_icon_difficulty,
    "icon_starting_board": generate_icon_starting_board,
    "icon_history": generate_icon_history,
    "icon_exit": generate_icon_exit,
}


def build_asset(name: str, descriptor, output_dir: Path) -> None:
    if isinstance(descriptor, tuple) and descriptor[0] == "piece":
        _, fill, border, accent = descriptor
        data = generate_piece_sprite(fill, border, accent)
    else:
        generator = GENERATOR_TABLE[descriptor]
        data = generator()

    write_binary(output_dir / name, data)
    return data


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate placeholder assets")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/generated"),
        help="Output directory for generated assets",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    output_dir = args.output.resolve()

    project_root = Path(__file__).resolve().parents[1]
    src_output = project_root / "src" / "assets" / "generated_assets.c"
    header_output = project_root / "include" / "assets" / "generated_assets.h"

    generated_payloads: dict[str, bytes] = {}

    for filename, descriptor in ASSET_DEFINITIONS.items():
        data = build_asset(filename, descriptor, output_dir)
        generated_payloads[filename] = data

    manifest_path = output_dir / "MANIFEST.txt"
    lines = [f"{name}" for name in sorted(ASSET_DEFINITIONS.keys())]
    manifest_content = "\n".join(lines) + "\n"
    write_binary(manifest_path, manifest_content.encode("ascii"))

    emit_c_sources(src_output, header_output, generated_payloads)


def emit_c_sources(c_path: Path, header_path: Path, payloads: dict[str, bytes]) -> None:
    ensure_directory(c_path.parent)
    ensure_directory(header_path.parent)

    array_meta = {}

    for filename, data in payloads.items():
        if filename == BOARD_KEY:
            continue
        array_name = to_symbol_name(Path(filename).stem)
        array_meta[filename] = (array_name, data)

    lines = [
        "// Auto-generated by scripts/generate_assets.py. Do not edit manually.",
        "#include <stdint.h>",
        "",
        "#include \"platform/video.h\"",
        "#include \"assets/generated_assets.h\"",
        "",
    ]

    for filename in ASSET_DEFINITIONS.keys():
        if filename == BOARD_KEY:
            continue
        array_name, data = array_meta[filename]
        lines.extend(format_c_array(array_name, data))

    board_symbol = "0"
    highlight_symbol = array_meta["sprite_highlight.bin"][0]
    move_indicator_symbol = array_meta[MOVE_INDICATOR_KEY][0]
    piece_symbols = [array_meta[key][0] for key in PIECE_KEYS]
    icon_symbols = [array_meta[key][0] for key in ICON_KEYS]

    lines.append("const video_asset_manifest_t g_video_assets = {")
    lines.append(f"    .highlight_frame = {highlight_symbol},")
    lines.append(f"    .highlight_frame_size = sizeof({highlight_symbol}),")
    lines.append(f"    .move_indicator = {move_indicator_symbol},")
    lines.append(f"    .move_indicator_size = sizeof({move_indicator_symbol}),")
    lines.append("    .piece_sprites = {")
    for symbol in piece_symbols:
        lines.append(f"        {symbol},")
    lines.append("    },")
    lines.append(f"    .piece_sprite_size = sizeof({piece_symbols[0]}),")
    lines.append("    .menu_icons = {")
    for symbol in icon_symbols:
        lines.append(f"        {symbol},")
    lines.append("    },")
    lines.append(f"    .menu_icon_size = sizeof({icon_symbols[0]}),")
    lines.append("};")
    lines.append("")

    c_path.write_text("\n".join(lines), encoding="ascii")

    header_lines = [
        "// Auto-generated by scripts/generate_assets.py. Do not edit manually.",
        "#ifndef ASSETS_GENERATED_ASSETS_H",
        "#define ASSETS_GENERATED_ASSETS_H",
        "",
        "#include \"platform/video.h\"",
        "",
        "extern const video_asset_manifest_t g_video_assets;",
        "",
        "#endif /* ASSETS_GENERATED_ASSETS_H */",
        "",
    ]

    header_path.write_text("\n".join(header_lines), encoding="ascii")


if __name__ == "__main__":
    main()