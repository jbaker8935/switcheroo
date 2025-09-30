#!/usr/bin/env python3
"""Generate assets for F256 Switcharoo game following video_assets.md specification.

This script generates:
- 320x240 board bitmap with 4x8 checkerboard (28x28 cells each)
- 24x24 piece sprites (normal and swapped variants)
- 16x16 icon sprites
- Proper CLUT slot assignments per specification
- PNG files with modern color theme for visualization
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path
from typing import Tuple

try:
    from PIL import Image
except ImportError:
    print("PIL (Python Imaging Library) is required for PNG generation.")
    print("Install with: pip install pillow")
    exit(1)


# CLUT slot assignments per video_assets.md
CLUT_TRANSPARENT = 0
CLUT_BOARD_CELLS = list(range(1, 33))  # Slots 1-32 for board cells
CLUT_BOARD_BORDER = 33
CLUT_FUTURE_USE = list(range(34, 65))  # Slots 34-64 reserved
# Piece sprite CLUT slots 65-76
CLUT_PLAYER_A_EDGE_1 = 65
CLUT_PLAYER_A_EDGE_2 = 66
CLUT_PLAYER_A_FILL_1 = 67
CLUT_PLAYER_A_FILL_2 = 68
CLUT_PLAYER_B_EDGE_1 = 69
CLUT_PLAYER_B_EDGE_2 = 70
CLUT_PLAYER_B_FILL_1 = 71
CLUT_PLAYER_B_FILL_2 = 72
CLUT_PLAYER_A_SWAPPED_1 = 73
CLUT_PLAYER_A_SWAPPED_2 = 74
CLUT_PLAYER_B_SWAPPED_1 = 75
CLUT_PLAYER_B_SWAPPED_2 = 76
# Icon sprite CLUT slots 77-84
CLUT_ICON_EDGE_1 = 77
CLUT_ICON_EDGE_2 = 78
CLUT_ICON_FILL_1 = 79
CLUT_ICON_FILL_2 = 80
CLUT_ICON_SYMBOL_1 = 81
CLUT_ICON_SYMBOL_2 = 82
CLUT_ICON_SYMBOL_3 = 83
CLUT_ICON_SYMBOL_4 = 84

# Modern color palette (RGB tuples) - maps CLUT indices to actual colors
MODERN_COLOR_PALETTE: list[Tuple[int, int, int]] = [
    # 0: Transparent
    (0, 0, 0, 0),  # RGBA with alpha
    
    # 1-32: Board cells - modern gradient from dark blue to light blue
    (26, 26, 46), (31, 31, 51), (36, 36, 56), (41, 41, 61),
    (46, 46, 66), (51, 51, 71), (56, 56, 76), (61, 61, 81),
    (66, 66, 86), (71, 71, 91), (76, 76, 96), (81, 81, 101),
    (86, 86, 106), (91, 91, 111), (96, 96, 116), (101, 101, 121),
    (106, 106, 126), (111, 111, 131), (116, 116, 136), (121, 121, 141),
    (126, 126, 146), (131, 131, 151), (136, 136, 156), (141, 141, 161),
    (146, 146, 166), (151, 151, 171), (156, 156, 176), (161, 161, 181),
    (166, 166, 186), (171, 171, 191), (176, 176, 196), (181, 181, 201),
    
    # 33: Board border - light gray
    (204, 204, 204),
    
    # 34-64: Future use - various grays
    *[(i*4, i*4, i*4) for i in range(31)],
    
    # 65-68: Player A piece colors - blue theme
    (74, 144, 226), (33, 113, 181), (52, 152, 219), (41, 128, 185),
    
    # 69-72: Player B piece colors - purple theme  
    (155, 89, 182), (142, 68, 173), (125, 60, 152), (108, 52, 131),
    
    # 73-76: Swapped symbol colors - white and light variants
    (255, 255, 255), (240, 240, 240), (255, 255, 255), (240, 240, 240),
    
    # 77-84: Icon colors - orange theme
    (230, 126, 34), (211, 84, 0), (243, 156, 18), (230, 126, 34),
    (241, 196, 15), (243, 156, 18), (230, 126, 34), (211, 84, 0),
]

# Board configuration
BOARD_COLUMNS = 4
BOARD_ROWS = 8
CELL_SIZE = 28
BOARD_WIDTH = BOARD_COLUMNS * CELL_SIZE
BOARD_HEIGHT = BOARD_ROWS * CELL_SIZE

# Screen configuration
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 240

# Asset definitions
PIECE_SIZE = 24
ICON_SIZE = 16


def ensure_directory(path: Path) -> None:
    """Create directory if it doesn't exist."""
    path.mkdir(parents=True, exist_ok=True)


def write_binary(path: Path, data: bytes) -> None:
    """Write binary data to file."""
    ensure_directory(path.parent)
    path.write_bytes(data)


def generate_board_bitmap() -> bytes:
    """Generate 320x240 board bitmap with centered 4x8 checkerboard."""
    # Calculate board position (centered)
    board_x = (SCREEN_WIDTH - BOARD_WIDTH) // 2
    board_y = (SCREEN_HEIGHT - BOARD_HEIGHT) // 2
    
    # 4-pixel border around board
    board_margin = 4
    
    buffer = bytearray(SCREEN_WIDTH * SCREEN_HEIGHT)
    
    for y in range(SCREEN_HEIGHT):
        for x in range(SCREEN_WIDTH):
            idx = y * SCREEN_WIDTH + x
            
            # Check if we're in the board area (including border)
            in_board_area = (
                x >= board_x - board_margin and 
                x < board_x + BOARD_WIDTH + board_margin and
                y >= board_y - board_margin and 
                y < board_y + BOARD_HEIGHT + board_margin
            )
            
            if not in_board_area:
                # Outside board area - use first board cell color as background
                buffer[idx] = CLUT_BOARD_CELLS[0]
                continue
            
            # Check if we're in the border area
            in_border = (
                x < board_x or x >= board_x + BOARD_WIDTH or
                y < board_y or y >= board_y + BOARD_HEIGHT
            )
            
            if in_border:
                buffer[idx] = CLUT_BOARD_BORDER
                continue
            
            # We're inside the board - calculate cell
            cell_x = (x - board_x) // CELL_SIZE
            cell_y = (y - board_y) // CELL_SIZE
            
            # Check for single-pixel borders between cells
            local_x = (x - board_x) % CELL_SIZE
            local_y = (y - board_y) % CELL_SIZE
            
            if local_x == CELL_SIZE - 1 or local_y == CELL_SIZE - 1:
                buffer[idx] = CLUT_BOARD_BORDER
            else:
                # Calculate which board cell (0-31)
                cell_index = cell_y * BOARD_COLUMNS + cell_x
                buffer[idx] = CLUT_BOARD_CELLS[cell_index]
    
    return bytes(buffer)


def generate_piece_sprite_normal_a() -> bytes:
    """Generate 24x24 normal piece sprite for Player A."""
    size = PIECE_SIZE
    center = size / 2.0
    buffer = bytearray([CLUT_TRANSPARENT] * size * size)
    
    for y in range(size):
        for x in range(size):
            # 1-pixel transparent border requirement
            if x == 0 or x == size-1 or y == 0 or y == size-1:
                continue
            
            dx = x - center
            dy = y - center
            dist = math.sqrt(dx * dx + dy * dy)
            idx = y * size + x
            
            # 2-pixel edge border, then fill
            if dist <= 8.0:
                buffer[idx] = CLUT_PLAYER_A_FILL_1
            elif dist <= 10.0:
                buffer[idx] = CLUT_PLAYER_A_EDGE_1
    
    return bytes(buffer)


def generate_piece_sprite_swapped_a() -> bytes:
    """Generate 24x24 swapped piece sprite for Player A with inset star."""
    size = PIECE_SIZE
    center = size / 2.0
    buffer = bytearray([CLUT_TRANSPARENT] * size * size)
    
    for y in range(size):
        for x in range(size):
            # 1-pixel transparent border requirement
            if x == 0 or x == size-1 or y == 0 or y == size-1:
                continue
            
            dx = x - center
            dy = y - center
            dist = math.sqrt(dx * dx + dy * dy)
            idx = y * size + x
            
            # Base circle with 2-pixel edge border
            if dist <= 8.0:
                buffer[idx] = CLUT_PLAYER_A_FILL_1
            elif dist <= 10.0:
                buffer[idx] = CLUT_PLAYER_A_EDGE_1
            
            # Add star symbol in center
            if dist <= 6.0:
                angle = math.atan2(dy, dx)
                # Create 5-pointed star pattern
                star_radius = 3.0 + 1.5 * math.cos(5 * angle)
                if dist >= star_radius:
                    buffer[idx] = CLUT_PLAYER_A_SWAPPED_1
    
    return bytes(buffer)


def generate_piece_sprite_normal_b() -> bytes:
    """Generate 24x24 normal piece sprite for Player B."""
    size = PIECE_SIZE
    center = size / 2.0
    buffer = bytearray([CLUT_TRANSPARENT] * size * size)
    
    for y in range(size):
        for x in range(size):
            # 1-pixel transparent border requirement
            if x == 0 or x == size-1 or y == 0 or y == size-1:
                continue
            
            dx = x - center
            dy = y - center
            dist = math.sqrt(dx * dx + dy * dy)
            idx = y * size + x
            
            # Square shape for Player B
            if abs(dx) <= 8.0 and abs(dy) <= 8.0:
                if abs(dx) <= 6.0 and abs(dy) <= 6.0:
                    buffer[idx] = CLUT_PLAYER_B_FILL_1
                else:
                    buffer[idx] = CLUT_PLAYER_B_EDGE_1
    
    return bytes(buffer)


def generate_piece_sprite_swapped_b() -> bytes:
    """Generate 24x24 swapped piece sprite for Player B with inset diamond."""
    size = PIECE_SIZE
    center = size / 2.0
    buffer = bytearray([CLUT_TRANSPARENT] * size * size)
    
    for y in range(size):
        for x in range(size):
            # 1-pixel transparent border requirement
            if x == 0 or x == size-1 or y == 0 or y == size-1:
                continue
            
            dx = x - center
            dy = y - center
            idx = y * size + x
            
            # Square shape for Player B
            if abs(dx) <= 8.0 and abs(dy) <= 8.0:
                if abs(dx) <= 6.0 and abs(dy) <= 6.0:
                    buffer[idx] = CLUT_PLAYER_B_FILL_1
                else:
                    buffer[idx] = CLUT_PLAYER_B_EDGE_1
                
                # Add diamond symbol in center
                diamond_dist = abs(dx) + abs(dy)
                if diamond_dist <= 4.0 and diamond_dist >= 2.0:
                    buffer[idx] = CLUT_PLAYER_B_SWAPPED_1
    
    return bytes(buffer)


def generate_icon_sprite(icon_type: str) -> bytes:
    """Generate 16x16 icon sprite with 2-pixel border."""
    size = ICON_SIZE
    buffer = bytearray([CLUT_ICON_FILL_1] * size * size)
    
    # 2-pixel border
    for i in range(size):
        for j in range(2):
            buffer[j * size + i] = CLUT_ICON_EDGE_1  # Top
            buffer[(size-1-j) * size + i] = CLUT_ICON_EDGE_1  # Bottom
            buffer[i * size + j] = CLUT_ICON_EDGE_1  # Left
            buffer[i * size + (size-1-j)] = CLUT_ICON_EDGE_1  # Right
    
    center = size // 2
    
    # Icon-specific symbols
    if icon_type == "reset":
        # Circular arrow
        for y in range(4, 12):
            for x in range(4, 12):
                dx = x - center
                dy = y - center
                dist = math.sqrt(dx * dx + dy * dy)
                if 2.5 <= dist <= 3.5:
                    buffer[y * size + x] = CLUT_ICON_SYMBOL_1
        # Arrow tip
        buffer[5 * size + 10] = CLUT_ICON_SYMBOL_1
        buffer[6 * size + 11] = CLUT_ICON_SYMBOL_1
    
    elif icon_type == "info":
        # "i" symbol
        buffer[5 * size + center] = CLUT_ICON_SYMBOL_1  # dot
        for y in range(7, 12):
            buffer[y * size + center] = CLUT_ICON_SYMBOL_1  # stem
    
    elif icon_type == "difficulty":
        # Three bars of increasing height
        for i in range(3):
            height = 2 + i * 2
            start_y = 12 - height
            x = 5 + i * 2
            for y in range(start_y, 12):
                buffer[y * size + x] = CLUT_ICON_SYMBOL_1
    
    elif icon_type == "starting_board":
        # Mini checkerboard
        for y in range(6, 10):
            for x in range(6, 10):
                if (x + y) % 2 == 0:
                    buffer[y * size + x] = CLUT_ICON_SYMBOL_1
                else:
                    buffer[y * size + x] = CLUT_ICON_SYMBOL_2
    
    elif icon_type == "history":
        # List lines
        for y in range(5, 11):
            buffer[y * size + 5] = CLUT_ICON_SYMBOL_1
            buffer[y * size + 10] = CLUT_ICON_SYMBOL_1
    
    elif icon_type == "exit":
        # X mark
        for i in range(5, 11):
            buffer[i * size + i] = CLUT_ICON_SYMBOL_1
            buffer[i * size + (15 - i)] = CLUT_ICON_SYMBOL_1
    
    return bytes(buffer)


def create_png_from_clut_data(data: bytes, width: int, height: int, filename: Path) -> None:
    """Create a PNG image from CLUT-indexed data using the modern color palette."""
    # Create RGB image
    img = Image.new('RGB', (width, height), (0, 0, 0))
    pixels = img.load()
    
    for y in range(height):
        for x in range(width):
            clut_index = data[y * width + x]
            if clut_index < len(MODERN_COLOR_PALETTE):
                color = MODERN_COLOR_PALETTE[clut_index]
                # Handle RGBA tuples (for transparency)
                if len(color) == 4:
                    # For transparent pixels, use a dark background color
                    pixels[x, y] = (26, 26, 46) if color[3] == 0 else color[:3]
                else:
                    pixels[x, y] = color
            else:
                # Fallback for out-of-range indices
                pixels[x, y] = (255, 0, 255)  # Magenta for errors
    
    img.save(filename)


def generate_png_assets(output_dir: Path, binary_assets: dict[str, bytes]) -> None:
    """Generate PNG versions of all binary assets."""
    png_dir = output_dir / "png"
    ensure_directory(png_dir)
    
    # Board bitmap
    board_data = binary_assets["board_bitmap.bin"]
    create_png_from_clut_data(board_data, SCREEN_WIDTH, SCREEN_HEIGHT, 
                            png_dir / "board_bitmap.png")
    
    # Piece sprites
    piece_files = [
        ("piece_bitmap_a_normal.bin", "piece_a_normal.png"),
        ("piece_bitmap_a_swapped.bin", "piece_a_swapped.png"),
        ("piece_bitmap_b_normal.bin", "piece_b_normal.png"),
        ("piece_bitmap_b_swapped.bin", "piece_b_swapped.png"),
    ]
    
    for bin_file, png_file in piece_files:
        if bin_file in binary_assets:
            piece_data = binary_assets[bin_file]
            create_png_from_clut_data(piece_data, PIECE_SIZE, PIECE_SIZE,
                                    png_dir / png_file)
    
    # Icon sprites
    icon_types = ["reset", "info", "difficulty", "starting_board", "history", "exit"]
    for icon_type in icon_types:
        bin_file = f"icon_{icon_type}.bin"
        png_file = f"icon_{icon_type}.png"
        if bin_file in binary_assets:
            icon_data = binary_assets[bin_file]
            create_png_from_clut_data(icon_data, ICON_SIZE, ICON_SIZE,
                                    png_dir / png_file)


def generate_assets(output_dir: Path) -> dict[str, bytes]:
    """Generate all game assets."""
    assets = {}
    
    # Board bitmap
    assets["board_bitmap.bin"] = generate_board_bitmap()
    
    # Piece bitmaps (only 4 total - 2 per player)
    assets["piece_bitmap_a_normal.bin"] = generate_piece_sprite_normal_a()
    assets["piece_bitmap_a_swapped.bin"] = generate_piece_sprite_swapped_a()
    assets["piece_bitmap_b_normal.bin"] = generate_piece_sprite_normal_b()
    assets["piece_bitmap_b_swapped.bin"] = generate_piece_sprite_swapped_b()
    
    # Icon sprites
    icon_types = ["reset", "info", "difficulty", "starting_board", "history", "exit"]
    for icon_type in icon_types:
        assets[f"icon_{icon_type}.bin"] = generate_icon_sprite(icon_type)
    
    # Write all binary assets to files
    for filename, data in assets.items():
        write_binary(output_dir / filename, data)
    
    # Generate PNG versions
    generate_png_assets(output_dir, assets)
    
    return assets





def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description="Generate F256 Switcharoo game assets")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/generated"),
        help="Output directory for generated assets",
    )
    return parser.parse_args()


def main() -> None:
    """Main entry point."""
    args = parse_args()
    output_dir = args.output.resolve()
    
    project_root = Path(__file__).resolve().parents[1]

    
    print(f"Generating assets to {output_dir}")
    assets = generate_assets(output_dir)

    
    print(f"Generated {len(assets)} binary assets:")
    for filename in sorted(assets.keys()):
        print(f"  {filename} ({len(assets[filename])} bytes)")
    
    png_dir = output_dir / "png"
    png_files = list(png_dir.glob("*.png")) if png_dir.exists() else []
    print(f"Generated {len(png_files)} PNG assets:")
    for png_file in sorted(png_files):
        print(f"  {png_file.name}")


if __name__ == "__main__":
    main()